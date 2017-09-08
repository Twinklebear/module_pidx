#include <random>
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
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SINGLE, &provided);

  vec2i fbSize(1920, 1080);
  std::string datasetPath;
  std::string outputPrefix = "frame";
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp("-dataset", argv[i]) == 0) {
      datasetPath = argv[++i];
    } else if (std::strcmp("-o", argv[i]) == 0) {
      outputPrefix = argv[++i];
    }
  }

  if (datasetPath.empty()) {
    throw std::runtime_error("Usage: mpirun -np <N> ./pidx_movie_renderer"
        " -dataset <dataset.idx>");
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
  PIDXVolume pidxVolume(datasetPath, tfcn);
  //pidxVolume.volume.set("samplingRate", 2.0);
  pidxVolume.volume.commit();
  // TODO: Update based on volume
  box3f worldBounds(vec3f(-64), vec3f(64));

  std::vector<box3f> regions{pidxVolume.localRegion};
  ospray::cpp::Data regionData(regions.size() * 2, OSP_FLOAT3, regions.data());
  model.set("regions", regionData);
  model.addVolume(pidxVolume.volume);
  model.commit();

  Camera camera("perspective");
  camera.set("pos", vec3f(0, 0, -1100));
  camera.set("dir", vec3f(0, 0, 1));
  camera.set("up", vec3f(0, 1, 0));
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

  mpicommon::world.barrier();

  float avgFrameTime = 0;
  size_t nframes = 25;
  for (size_t i = 0; i < nframes; ++i) {
    using namespace std::chrono;
    auto startFrame = high_resolution_clock::now();

    renderer.renderFrame(fb, OSP_FB_COLOR);

    auto endFrame = high_resolution_clock::now();
    float frameTime = duration_cast<milliseconds>(endFrame - startFrame).count() / 1000.f;
    if (rank == 0) {
      std::cout << "Frame took " << frameTime << "ms\n";
    }
    avgFrameTime += frameTime;

    if (rank == 0) {
      uint32_t *img = (uint32_t*)fb.map(OSP_FB_COLOR);
      char frameStr[16] = {0};
      std::snprintf(frameStr, 15, "%08lu", i);
      save_jpeg_file(outputPrefix + "-" + std::string(frameStr) + ".jpg",
          img, fbSize.x, fbSize.y);
      fb.unmap(img);
    }
  }
  if (rank == 0) {
    std::cout << "Avg. frame time: " << avgFrameTime / nframes << "ms\n";
  }

  MPI_Finalize();
  return 0;
}


