#include <random>
#include <memory>
#include <algorithm>
#include <array>
#include <chrono>
#include <mpiCommon/MPICommon.h>
#include <mpi.h>
#include <unistd.h>
#include "ospray/ospray_cpp/Camera.h"
#include "ospray/ospray_cpp/Data.h"
#include "ospray/ospray_cpp/Device.h"
#include "ospray/ospray_cpp/FrameBuffer.h"
#include "ospray/ospray_cpp/Geometry.h"
#include "ospray/ospray_cpp/Renderer.h"
#include "ospray/ospray_cpp/TransferFunction.h"
#include "ospray/ospray_cpp/Volume.h"
#include "ospray/ospray_cpp/Model.h"
#include "ospcommon/networking/Socket.h"
#include "util.h"
#include "image_util.h"
#include "pidx_volume.h"
#include "client_server.h"

using namespace ospcommon;
using namespace ospray::cpp;

int main(int argc, char **argv) {
  int provided = 0;
  int port = -1;
  // TODO: OpenMPI sucks as always and doesn't support pt2pt one-sided
  // communication with thread multiple. This can trigger a hang in OSPRay
  // if you're not using OpenMPI you can change this to MPI_THREAD_MULTIPLE
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SINGLE, &provided);

  std::string datasetPath;
  std::vector<std::string> timestepDirs;
  AppState app;
  AppData appdata;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp("-dataset", argv[i]) == 0) {
      datasetPath = argv[++i];
    } else if (std::strcmp("-port", argv[i]) == 0) {
      port = std::atoi(argv[++i]);
    } else if (std::strcmp("-timestep", argv[i]) == 0) {
      app.currentTimestep = std::atoll(argv[++i]);
    } else if (std::strcmp("-variable", argv[i]) == 0) {
      appdata.currentVariable = std::string(argv[++i]);
    } else if (std::strcmp("-timesteps", argv[i]) == 0) {
      for (; i + 1 < argc; ++i) {
        if (argv[i + 1][0] == '-') {
          break;
        }
        timestepDirs.push_back(argv[i + 1]);
      }
    }
  }
  if (datasetPath.empty() && timestepDirs.empty()) {
    std::cout << "Usage: mpirun -np <N> ./pidx_render_worker [options]\n"
      << "Options:\n"
      << "-dataset <dataset.idx>\n"
      << "-timesteps [list of timestep dirs]\n"
      << "-port <port>\n"
      << "-timestep <timestep>\n"
      << "-variable <variable>";
    return 1;
  }

  ospLoadModule("mpi");
  Device device("mpi_distributed");
  device.set("masterRank", 0);
  device.commit();
  device.setCurrent();

  const int rank = mpicommon::world.rank;
  const int worldSize = mpicommon::world.size;

  std::unique_ptr<ClientConnection> client;
  if (rank == 0) {
    char hostname[1024] = {0};
    gethostname(hostname, 1023);
    std::cout << "Now listening for client on " << hostname << ":" << port << std::endl;
    client = ospcommon::make_unique<ClientConnection>(port);
  }

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
    const std::vector<float> opacities = {0.0001, 0.02, 0.02, 0.01};
    ospray::cpp::Data colorsData(colors.size(), OSP_FLOAT3, colors.data());
    ospray::cpp::Data opacityData(opacities.size(), OSP_FLOAT, opacities.data());
    colorsData.commit();
    opacityData.commit();
    tfcn.set("colors", colorsData);
    tfcn.set("opacities", opacityData);
  }

  Model model;

  std::set<UintahTimestep> uintahTimesteps;
  if (datasetPath.empty()) {
    // Follow the Uintah directory structure for PIDX to the CCVars.idx for
    // the timestep
    uintahTimesteps = collectUintahTimesteps(timestepDirs);
    std::cout << "Read " << uintahTimesteps.size() << " timestep dirs" << std::endl;
    auto t = uintahTimesteps.begin();
    datasetPath = t->path;
    app.currentTimestep = t->timestep;
    std::cout << "dataset for first timestep = " << datasetPath
      << ", timestep = " << app.currentTimestep << std::endl;
  }

  auto pidxVolume = std::make_shared<PIDXVolume>(datasetPath, tfcn,
      appdata.currentVariable, app.currentTimestep);
  // TODO: Update based on volume
  box3f worldBounds(vec3f(-64), vec3f(64));

  std::vector<box3f> regions{pidxVolume->localRegion};
  ospray::cpp::Data regionData(regions.size() * 2, OSP_FLOAT3, regions.data());
  model.set("regions", regionData);
  model.addVolume(pidxVolume->volume);
  model.commit();

  Camera camera("perspective");
  camera.set("pos", vec3f(0, 0, -500));
  camera.set("dir", vec3f(0, 0, 1));
  camera.set("up", vec3f(0, 1, 0));
  camera.set("aspect", static_cast<float>(app.fbSize.x) / app.fbSize.y);
  camera.commit();

  Renderer renderer("mpi_raycast");
  renderer.set("model", model);
  renderer.set("camera", camera);
  renderer.set("bgColor", vec3f(0.02));
  renderer.commit();
  assert(renderer);

  FrameBuffer fb(app.fbSize, OSP_FB_SRGBA, OSP_FB_COLOR | OSP_FB_ACCUM | OSP_FB_VARIANCE);
  fb.clear(OSP_FB_COLOR | OSP_FB_ACCUM | OSP_FB_VARIANCE);

  if (rank == 0) {
    client->send_metadata(pidxVolume->pidxVars, uintahTimesteps,
        appdata.currentVariable, app.currentTimestep);
  }

  mpicommon::world.barrier();

  while (!app.quit) {
    using namespace std::chrono;

    if (app.cameraChanged) {
      camera.set("pos", app.v[0]);
      camera.set("dir", app.v[1]);
      camera.set("up", app.v[2]);
      camera.commit();

      fb.clear(OSP_FB_COLOR | OSP_FB_ACCUM | OSP_FB_VARIANCE);
      app.cameraChanged = false;
    }
    auto startFrame = high_resolution_clock::now();

    renderer.renderFrame(fb, OSP_FB_COLOR);

    auto endFrame = high_resolution_clock::now();

    if (rank == 0) {
      //std::cout << "Frame took "
      //<< duration_cast<milliseconds>(endFrame - startFrame).count()
      //<< "ms\n";

      uint32_t *img = (uint32_t*)fb.map(OSP_FB_COLOR);
      client->send_frame(img, app.fbSize.x, app.fbSize.y);
      fb.unmap(img);

      client->recieve_app_state(app, appdata);
    }

    // Send out the shared app state that the workers need to know, e.g. camera
    // position, if we should be quitting.
    MPI_Bcast(&app, sizeof(AppState), MPI_BYTE, 0, MPI_COMM_WORLD);

    if (app.fbSizeChanged) {
      fb = FrameBuffer(app.fbSize, OSP_FB_SRGBA, OSP_FB_COLOR | OSP_FB_ACCUM);
      fb.clear(OSP_FB_COLOR | OSP_FB_ACCUM | OSP_FB_VARIANCE);
      camera.set("aspect", static_cast<float>(app.fbSize.x) / app.fbSize.y);
      camera.commit();

      app.fbSizeChanged = false;
    }
    if (app.tfcnChanged) {
      size_t sz = appdata.tfcn_colors.size();
      MPI_Bcast(&sz, sizeof(size_t), MPI_BYTE, 0, MPI_COMM_WORLD);
      if (rank != 0) {
        appdata.tfcn_colors.resize(sz);
      }
      MPI_Bcast(appdata.tfcn_colors.data(), sizeof(vec3f) * appdata.tfcn_colors.size(),
          MPI_BYTE, 0, MPI_COMM_WORLD);

      sz = appdata.tfcn_alphas.size();
      MPI_Bcast(&sz, sizeof(size_t), MPI_BYTE, 0, MPI_COMM_WORLD);
      if (rank != 0) {
        appdata.tfcn_alphas.resize(sz);
      }
      MPI_Bcast(appdata.tfcn_alphas.data(), sizeof(float) * appdata.tfcn_alphas.size(), MPI_BYTE,
                0, MPI_COMM_WORLD);

      Data colorData(appdata.tfcn_colors.size(), OSP_FLOAT3, appdata.tfcn_colors.data());
      Data alphaData(appdata.tfcn_alphas.size(), OSP_FLOAT, appdata.tfcn_alphas.data());
      colorData.commit();
      alphaData.commit();

      tfcn.set("colors", colorData);
      tfcn.set("opacities", alphaData);
      tfcn.commit();

      fb.clear(OSP_FB_COLOR | OSP_FB_ACCUM | OSP_FB_VARIANCE);
      app.tfcnChanged = false;
    }
    if (app.fieldChanged) {
      size_t sz = appdata.currentVariable.size();
      MPI_Bcast(&sz, sizeof(size_t), MPI_BYTE, 0, MPI_COMM_WORLD);
      if (rank != 0) {
        appdata.currentVariable.resize(sz, ' ');
      }
      MPI_Bcast(&appdata.currentVariable[0], sz, MPI_BYTE, 0, MPI_COMM_WORLD);
      std::cout << "Got field change, to field #" << appdata.currentVariable << "\n";
    }
    if (app.timestepChanged) {
      MPI_Bcast(&app.currentTimestep, sizeof(size_t), MPI_BYTE, 0, MPI_COMM_WORLD);
      std::cout << "Got timestep change, to time #" << app.currentTimestep << "\n";
      if (!uintahTimesteps.empty()) {
        auto t = std::find_if(uintahTimesteps.begin(), uintahTimesteps.end(),
            [&](const UintahTimestep &t) {
              return t.timestep == app.currentTimestep;
            });
        datasetPath = t->path;
        std::cout << "Changing to dataset path: " << datasetPath << "\n";
      }
    }
    if (app.timestepChanged || app.fieldChanged) {
      model.removeVolume(pidxVolume->volume);
      pidxVolume = std::make_shared<PIDXVolume>(datasetPath, tfcn,
          appdata.currentVariable, app.currentTimestep);
      model.addVolume(pidxVolume->volume);
      model.commit();

      fb.clear(OSP_FB_COLOR | OSP_FB_ACCUM | OSP_FB_VARIANCE);
      app.fieldChanged = false;
      app.timestepChanged = false;
    }
  }

  pidxVolume = nullptr;

  MPI_Finalize();
  return 0;
}

