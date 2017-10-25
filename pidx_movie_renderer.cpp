#include <random>
#include <cmath>
#include <algorithm>
#include <array>
#include <chrono>
#include <mpiCommon/MPICommon.h>
#include <mpi.h>
#include "ospray/ospray_cpp/Camera.h"
#include "ospray/ospray_cpp/Data.h"
#include "ospray/ospray_cpp/Device.h"
#include "ospray/ospray_cpp/FrameBuffer.h"
#include "ospray/ospray_cpp/Geometry.h"
#include "ospray/ospray_cpp/Renderer.h"
#include "ospray/ospray_cpp/TransferFunction.h"
#include "ospray/ospray_cpp/Volume.h"
#include "ospray/ospray_cpp/Model.h"
#include "util.h"
#include "image_util.h"
#include "pidx_volume.h"

using namespace ospcommon;
using namespace ospray::cpp;

int main(int argc, char **argv) {
  int provided = 0;
  // TODO: OpenMPI sucks as always and doesn't support pt2pt one-sided
  // communication with thread multiple. This can trigger a hang in OSPRay
  // if you're not using OpenMPI you can change this to MPI_THREAD_MULTIPLE
  //MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SINGLE, &provided);

  vec2i fbSize(1080, 1920);
  std::string datasetPath;
  std::string timestepDir;
  std::string outputPrefix = "frame";
  size_t framesPerTimestep = 2;
  std::string variableName;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp("-dataset", argv[i]) == 0) {
      datasetPath = argv[++i];
    } else if (std::strcmp("-o", argv[i]) == 0) {
      outputPrefix = argv[++i];
    } else if (std::strcmp("-timesteps", argv[i]) == 0) {
      timestepDir = argv[++i];
    } else if (std::strcmp("-variable", argv[i]) == 0) {
      variableName = std::string(argv[++i]);
    }
  }

  if (datasetPath.empty() && timestepDir.empty()) {
    throw std::runtime_error("Usage: mpirun -np <N> ./pidx_movie_renderer [options]\n"
        "Options:\n"
        "-dataset <dataset.idx>       Specify the IDX datset to load and render\n"
        "-timesteps <dir>             Specify the directory containing Uintah timesteps\n"
        );
  }

  ospLoadModule("mpi");
  Device device("mpi_distributed");
  device.set("masterRank", 0);
  device.commit();
  device.setCurrent();

  const int rank = mpicommon::world.rank;
  const int worldSize = mpicommon::world.size;

  TransferFunction tfcn("piecewise_linear");
  // Fill in some initial data for transfer fcn
  {
    const std::vector<vec3f> colors = {
      vec3f(0, 0, 0.56),
      vec3f(0, 0, 1),
      vec3f(0, 1, 1),
      vec3f(0.5, 1, 0.5),
      vec3f(1, 1, 0),
      vec3f(1, 0, 0),
      vec3f(0.5, 0, 0)
    };
    const std::vector<float> opacities = {0.0001, 1.0};
    ospray::cpp::Data colorsData(colors.size(), OSP_FLOAT3, colors.data());
    ospray::cpp::Data opacityData(opacities.size(), OSP_FLOAT, opacities.data());
    colorsData.commit();
    opacityData.commit();

    tfcn.set("colors", colorsData);
    tfcn.set("opacities", opacityData);
  }

  Model model;
  // We've got a set of timesteps instead of a single dataset
  std::set<UintahTimestep> uintahTimesteps;
  std::set<UintahTimestep>::const_iterator currentTimestep;
  if (datasetPath.empty()) {
    // Follow the Uintah directory structure for PIDX to the CCVars.idx for
    // the timestep
    uintahTimesteps = collectUintahTimesteps(timestepDir);
    std::cout << "Read " << uintahTimesteps.size() << " timestep dirs" << std::endl;
    currentTimestep = uintahTimesteps.cbegin();
    datasetPath = currentTimestep->path;
    std::cout << "dataset for first timestep = " << datasetPath
      << ", timestep = " << currentTimestep->timestep << std::endl;
  }
  auto pidxVolume = std::make_shared<PIDXVolume>(datasetPath, tfcn,
      variableName, currentTimestep->timestep);
  pidxVolume->volume.commit();
  // TODO: Update based on volume
  box3f worldBounds(vec3f(-64), vec3f(64));

  std::vector<box3f> regions{pidxVolume->localRegion};
  ospray::cpp::Data regionData(regions.size() * 2, OSP_FLOAT3, regions.data());
  model.set("regions", regionData);
  model.addVolume(pidxVolume->volume);
  model.commit();

  Camera camera("perspective");
  vec3f cameraPos(0, 0, -1600);
  camera.set("pos", cameraPos);
  camera.set("dir", -cameraPos);
  camera.set("up", vec3f(-1, 0, 0));
  camera.set("aspect", static_cast<float>(fbSize.x) / fbSize.y);
  camera.commit();

  Renderer renderer("mpi_raycast");
  renderer.set("model", model);
  renderer.set("camera", camera);
  renderer.set("bgColor", vec3f(0.02));
  renderer.commit();
  assert(renderer);

  FrameBuffer fb(fbSize, OSP_FB_SRGBA, OSP_FB_COLOR | OSP_FB_ACCUM);
  fb.clear(OSP_FB_COLOR | OSP_FB_ACCUM);

  JPGCompressor compressor(90);

  mpicommon::world.barrier();

  float avgFrameTime = 0;
  size_t nframes = 100;
  size_t spp = 1;
  float radiansPerSecond = 0.5;
  for (size_t i = 0; i < nframes; ++i) {
    using namespace std::chrono;

    if (i != 0 && !uintahTimesteps.empty() && i % framesPerTimestep == 0) {
      std::cout << "Moving to next sim timestep" << std::endl;
      if (std::distance(currentTimestep, uintahTimesteps.cend()) > 1) {
        ++currentTimestep;
        datasetPath = currentTimestep->path;
        std::cout << "dataset for timestep  = " << datasetPath << std::endl;

        model.removeVolume(pidxVolume->volume);
        pidxVolume = std::make_shared<PIDXVolume>(datasetPath, tfcn,
            variableName, currentTimestep->timestep);
        model.addVolume(pidxVolume->volume);
        model.commit();
      }
    }

    std::cout << "Rendering frame " << i << std::endl;

    float time = i / 24.0;
    cameraPos.y = 1600 * std::sin(time * radiansPerSecond);
    cameraPos.z = -1600 * std::cos(time * radiansPerSecond);
    camera.set("pos", cameraPos);
    camera.set("dir", -cameraPos);
    camera.commit();
    fb.clear(OSP_FB_COLOR | OSP_FB_ACCUM);

    auto startFrame = high_resolution_clock::now();
    // We use progressive refinment for multiple samples per-pixel,
    // seems like a bug with using spp > 1 in the distrib raycast renderer
    // where we start seeing block boundary artifacts. TODO: investigate
    for (size_t s = 0; s < spp; ++s) {
      renderer.renderFrame(fb, OSP_FB_COLOR);
    }
    auto endFrame = high_resolution_clock::now();

    float frameTime = duration_cast<milliseconds>(endFrame - startFrame).count() / 1000.f;
    if (rank == 0) {
      std::cout << "Frame took " << frameTime << "s\n";
    }
    avgFrameTime += frameTime;

    if (rank == 0) {
      uint32_t *img = (uint32_t*)fb.map(OSP_FB_COLOR);
      char frameStr[16] = {0};
      std::snprintf(frameStr, 15, "%08lu", i);
      auto jpg = compressor.compress(img, fbSize.x, fbSize.y);
      fb.unmap(img);

      const std::string imgName = outputPrefix + "-" + std::string(frameStr) + ".jpg";
      std::ofstream fout(imgName.c_str(), std::ios::binary);
      fout.write(reinterpret_cast<const char*>(jpg.first), jpg.second);
    }
  }
  if (rank == 0) {
    std::cout << "Avg. frame time: " << avgFrameTime / nframes << "s\n";
  }
  pidxVolume = nullptr;

  MPI_Finalize();
  return 0;
}


