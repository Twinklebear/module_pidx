#include <mpiCommon/MPICommon.h>
#include <mpi.h>
#include "common/imgui/imgui.h"
#include "pidx_volume.h"

using namespace ospray::cpp;
using namespace ospcommon;

PIDXVolume::PIDXVolume(const std::string &path, TransferFunction tfcn,
    const std::string &currentVariableName, size_t currentTimestep)
  : datasetPath(path), volume("block_bricked_volume"), transferFunction(tfcn),
  currentVariableName(currentVariableName), currentTimestep(currentTimestep)
{
  PIDX_CHECK(PIDX_create_access(&pidxAccess));
  PIDX_CHECK(PIDX_set_mpi_access(pidxAccess, MPI_COMM_WORLD));
  currentVariable = -1;
  update();
}
PIDXVolume::~PIDXVolume() {
  PIDX_close_access(pidxAccess);
  volume.release();
  transferFunction.release();
}
void PIDXVolume::update() {
  const int rank = mpicommon::world.rank;
  const int numRanks = mpicommon::world.size;

  PIDX_CHECK(PIDX_file_open(datasetPath.c_str(), PIDX_MODE_RDONLY,
        pidxAccess, pdims, &pidxFile));
  fullDims = vec3sz(pdims[0], pdims[1], pdims[2]);

  if (rank == 0) {
     std::cout << "currentimestep = " << currentTimestep << std::endl;
  }
  PIDX_CHECK(PIDX_set_current_time_step(pidxFile, currentTimestep));

  if (pidxVars.empty()) {
    int variableCount = 0;
    PIDX_CHECK(PIDX_get_variable_count(pidxFile, &variableCount));
    if (rank == 0) {
      std::cout << "Variable count = " << variableCount << "\n";
    }

    for (int i = 0; i < variableCount; ++i) {
      PIDX_CHECK(PIDX_set_current_variable_index(pidxFile, i));
      PIDX_variable variable;
      PIDX_CHECK(PIDX_get_current_variable(pidxFile, &variable));
      pidxVars.push_back(variable->var_name);
      if (currentVariableName.compare(variable->var_name) == 0) {
        currentVariable = i;
      }
    }

    if (currentVariable == -1) {
      currentVariable = 0;
      std::cerr << "Variable name is not set. "
        "Loading the first variable by default" << std::endl;
    }
  }

  MPI_Bcast(&currentVariable, 1, MPI_INT, 0, MPI_COMM_WORLD);

  PIDX_CHECK(PIDX_set_current_variable_index(pidxFile, currentVariable));
  PIDX_variable variable;
  PIDX_CHECK(PIDX_get_current_variable(pidxFile, &variable));

  int valuesPerSample = 0;
  int bitsPerSample = 0;
  PIDX_CHECK(PIDX_values_per_datatype(variable->type_name, &valuesPerSample,
        &bitsPerSample));
  const int bytesPerSample = bitsPerSample / 8;

  if (rank == 0) {
    std::cout << "Volume dimensions: " << fullDims << "\n"
      << "Variable type name: " << variable->type_name << "\n"
      << "Values per sample: " << valuesPerSample << "\n"
      << "Bits per sample: " << bitsPerSample << std::endl;
  }

  const vec3sz grid = vec3sz(computeGrid(numRanks));
  const vec3sz brickDims = vec3sz(fullDims) / grid;
  const vec3sz brickId(rank % grid.x, (rank / grid.x) % grid.y, rank / (grid.x * grid.y));
  const vec3f gridOrigin = vec3f(brickId) * vec3f(brickDims);

  const std::array<int, 3> ghosts = computeGhostFaces(vec3i(brickId), vec3i(grid));
  vec3sz ghostDims(0);
  for (size_t i = 0; i < 3; ++i) {
    if (ghosts[i] & POS_FACE) {
      ghostDims[i] += 1;
    }
    if (ghosts[i] & NEG_FACE) {
      ghostDims[i] += 1;
    }
  }
  localDims = brickDims + ghostDims;
  const vec3sz ghostOffset(ghosts[0] & NEG_FACE ? 1 : 0,
      ghosts[1] & NEG_FACE ? 1 : 0,
      ghosts[2] & NEG_FACE ? 1 : 0);

  localOffset = brickId * brickDims - ghostOffset;
  PIDX_point pLocalOffset, pLocalDims;
  PIDX_set_point(pLocalOffset, localOffset.x, localOffset.y, localOffset.z);
  PIDX_set_point(pLocalDims, localDims.x, localDims.y, localDims.z);

  const size_t nLocalVals = localDims.x * localDims.y * localDims.z;
  std::vector<char> data(bytesPerSample * valuesPerSample * nLocalVals, 0);
  PIDX_CHECK(PIDX_variable_read_data_layout(variable, pLocalOffset, pLocalDims,
        data.data(), PIDX_row_major));

  using namespace std::chrono;
  auto startLoad = high_resolution_clock::now();
  PIDX_CHECK(PIDX_close(pidxFile));
  auto endLoad = high_resolution_clock::now();

  std::cout << "Rank " << rank << " load time: "
    << duration_cast<milliseconds>(endLoad - startLoad).count() << "ms\n";

  auto minmax = std::minmax_element(reinterpret_cast<float*>(data.data()),
      reinterpret_cast<float*>(data.data()) + nLocalVals);
  // TODO: Need MPI Allreduce here
  vec2f localValueRange = vec2f(*minmax.first, *minmax.second);
  // std::cout << "Local range = " << localValueRange << "\n";

  MPI_Allreduce(&localValueRange.x, &valueRange.x, 1, MPI_FLOAT,
      MPI_MIN, MPI_COMM_WORLD);
  MPI_Allreduce(&localValueRange.y, &valueRange.y, 1, MPI_FLOAT,
      MPI_MAX, MPI_COMM_WORLD);
  valueRange = vec2f(0.0, 0.15);
  transferFunction.set("valueRange", valueRange);
  transferFunction.commit();

  if (rank == 0) {
    std::cout << "Value range = " << valueRange << "\n";
  }

  volume.set("transferFunction", transferFunction);
  // TODO: Parse the IDX type name into the OSPRay type name
  volume.set("voxelType", "float");
  // TODO: This will be the local dimensions later
  volume.set("dimensions", vec3i(localDims));
  volume.set("gridOrigin", vec3f(localOffset) - vec3f(fullDims) / 2.f);
  // TODO: Use the logic box to figure out grid spacing
  //volume.set("gridSpacing", vec3f(dimensions) / vec3f(ospDims));

  // Now we have some row-major data in the array we can pass to an OSPRay volume
  volume.setRegion(data.data(), vec3i(0), vec3i(localDims));
  volume.commit();

  localRegion = box3f(vec3f(brickId * brickDims) - vec3f(fullDims) / 2.f,
      vec3f(brickId * brickDims + brickDims) - vec3f(fullDims) / 2.f);
}

