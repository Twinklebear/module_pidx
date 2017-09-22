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
#include "ospcommon/networking/Socket.h"
#include "util.h"
#include "image_util.h"
#include "pidx_volume.h"

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
  size_t timestep = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp("-dataset", argv[i]) == 0) {
      datasetPath = argv[++i];
    } else if (std::strcmp("-port", argv[i]) == 0) {
      port = std::atoi(argv[++i]);
    } else if (std::strcmp("-timestep", argv[i]) == 0) {
      timestep = std::atoll(argv[++i]);
    }
  }
  if (datasetPath.empty()) {
    throw std::runtime_error("Usage: mpirun -np <N> ./pidx_render_worker"
        " -dataset <dataset.idx> -port <port> -timestep <timestep>");
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
    // const std::vector<vec3f> colors = {
    //   vec3f(0, 0, 0.56),
    //   vec3f(0, 0, 1),
    //   vec3f(0, 1, 1),
    //   vec3f(0.5, 1, 0.5),
    //   vec3f(1, 1, 0),
    //   vec3f(1, 0, 0),
    //   vec3f(0.5, 0, 0)
    // };
    //const std::vector<float> opacities = {0.0001, 0.02, 0.02, 0.01};
    const std::vector<vec3f> colors = {
      vec3f(0.000000,0.000000,0.501961),
      vec3f(0.000000,0.000000,0.513726),
      vec3f(0.000000,0.000000,0.525490),
      vec3f(0.000000,0.000000,0.537255),
      vec3f(0.000000,0.000000,0.552941),
      vec3f(0.000000,0.000000,0.564706),
      vec3f(0.000000,0.000000,0.576471),
      vec3f(0.000000,0.000000,0.588235),
      vec3f(0.000000,0.000000,0.603922),
      vec3f(0.000000,0.000000,0.615686),
      vec3f(0.000000,0.000000,0.627451),
      vec3f(0.000000,0.000000,0.639216),
      vec3f(0.000000,0.000000,0.654902),
      vec3f(0.000000,0.000000,0.666667),
      vec3f(0.000000,0.000000,0.678431),
      vec3f(0.000000,0.000000,0.690196),
      vec3f(0.000000,0.000000,0.705882),
      vec3f(0.000000,0.000000,0.717647),
      vec3f(0.000000,0.000000,0.729412),
      vec3f(0.000000,0.000000,0.741176),
      vec3f(0.000000,0.000000,0.756863),
      vec3f(0.000000,0.000000,0.768627),
      vec3f(0.000000,0.000000,0.780392),
      vec3f(0.000000,0.000000,0.792157),
      vec3f(0.000000,0.000000,0.807843),
      vec3f(0.000000,0.000000,0.819608),
      vec3f(0.000000,0.000000,0.831373),
      vec3f(0.000000,0.000000,0.843137),
      vec3f(0.000000,0.000000,0.858824),
      vec3f(0.000000,0.000000,0.870588),
      vec3f(0.000000,0.000000,0.882353),
      vec3f(0.000000,0.000000,0.894118),
      vec3f(0.000000,0.000000,0.909804),
      vec3f(0.000000,0.000000,0.921569),
      vec3f(0.000000,0.000000,0.933333),
      vec3f(0.000000,0.000000,0.945098),
      vec3f(0.000000,0.000000,0.960784),
      vec3f(0.000000,0.000000,0.972549),
      vec3f(0.000000,0.000000,0.984314),
      vec3f(0.000000,0.000000,1.000000),
      vec3f(0.000000,0.000000,1.000000),
      vec3f(0.000000,0.019608,1.000000),
      vec3f(0.000000,0.043137,1.000000),
      vec3f(0.000000,0.066667,1.000000),
      vec3f(0.000000,0.086275,1.000000),
      vec3f(0.000000,0.109804,1.000000),
      vec3f(0.000000,0.133333,1.000000),
      vec3f(0.000000,0.152941,1.000000),
      vec3f(0.000000,0.176471,1.000000),
      vec3f(0.000000,0.200000,1.000000),
      vec3f(0.000000,0.219608,1.000000),
      vec3f(0.000000,0.243137,1.000000),
      vec3f(0.000000,0.266667,1.000000),
      vec3f(0.000000,0.286275,1.000000),
      vec3f(0.000000,0.309804,1.000000),
      vec3f(0.000000,0.333333,1.000000),
      vec3f(0.000000,0.352941,1.000000),
      vec3f(0.000000,0.376471,1.000000),
      vec3f(0.000000,0.400000,1.000000),
      vec3f(0.000000,0.419608,1.000000),
      vec3f(0.000000,0.443137,1.000000),
      vec3f(0.000000,0.466667,1.000000),
      vec3f(0.000000,0.486275,1.000000),
      vec3f(0.000000,0.509804,1.000000),
      vec3f(0.000000,0.533333,1.000000),
      vec3f(0.000000,0.552941,1.000000),
      vec3f(0.000000,0.576471,1.000000),
      vec3f(0.000000,0.600000,1.000000),
      vec3f(0.000000,0.619608,1.000000),
      vec3f(0.000000,0.643137,1.000000),
      vec3f(0.000000,0.666667,1.000000),
      vec3f(0.000000,0.686275,1.000000),
      vec3f(0.000000,0.709804,1.000000),
      vec3f(0.000000,0.733333,1.000000),
      vec3f(0.000000,0.752941,1.000000),
      vec3f(0.000000,0.776471,1.000000),
      vec3f(0.000000,0.800000,1.000000),
      vec3f(0.000000,0.819608,1.000000),
      vec3f(0.000000,0.843137,1.000000),
      vec3f(0.000000,0.866667,1.000000),
      vec3f(0.000000,0.886275,1.000000),
      vec3f(0.000000,0.909804,1.000000),
      vec3f(0.000000,0.933333,1.000000),
      vec3f(0.000000,0.952941,1.000000),
      vec3f(0.000000,0.976471,1.000000),
      vec3f(0.000000,1.000000,1.000000),
      vec3f(0.000000,1.000000,1.000000),
      vec3f(0.000000,1.000000,0.972549),
      vec3f(0.000000,1.000000,0.949020),
      vec3f(0.000000,1.000000,0.925490),
      vec3f(0.000000,1.000000,0.901961),
      vec3f(0.000000,1.000000,0.878431),
      vec3f(0.000000,1.000000,0.854902),
      vec3f(0.000000,1.000000,0.831373),
      vec3f(0.000000,1.000000,0.807843),
      vec3f(0.000000,1.000000,0.784314),
      vec3f(0.000000,1.000000,0.760784),
      vec3f(0.000000,1.000000,0.737255),
      vec3f(0.000000,1.000000,0.713726),
      vec3f(0.000000,1.000000,0.690196),
      vec3f(0.000000,1.000000,0.662745),
      vec3f(0.000000,1.000000,0.639216),
      vec3f(0.000000,1.000000,0.615686),
      vec3f(0.000000,1.000000,0.592157),
      vec3f(0.000000,1.000000,0.568627),
      vec3f(0.000000,1.000000,0.545098),
      vec3f(0.000000,1.000000,0.521569),
      vec3f(0.000000,1.000000,0.498039),
      vec3f(0.000000,1.000000,0.474510),
      vec3f(0.000000,1.000000,0.450980),
      vec3f(0.000000,1.000000,0.427451),
      vec3f(0.000000,1.000000,0.403922),
      vec3f(0.000000,1.000000,0.380392),
      vec3f(0.000000,1.000000,0.356863),
      vec3f(0.000000,1.000000,0.329412),
      vec3f(0.000000,1.000000,0.305882),
      vec3f(0.000000,1.000000,0.282353),
      vec3f(0.000000,1.000000,0.258824),
      vec3f(0.000000,1.000000,0.235294),
      vec3f(0.000000,1.000000,0.211765),
      vec3f(0.000000,1.000000,0.188235),
      vec3f(0.000000,1.000000,0.164706),
      vec3f(0.000000,1.000000,0.141176),
      vec3f(0.000000,1.000000,0.117647),
      vec3f(0.000000,1.000000,0.094118),
      vec3f(0.000000,1.000000,0.070588),
      vec3f(0.000000,1.000000,0.047059),
      vec3f(0.000000,1.000000,0.023529),
      vec3f(0.000000,1.000000,0.000000),
      vec3f(0.000000,1.000000,0.000000),
      vec3f(0.019608,1.000000,0.000000),
      vec3f(0.043137,1.000000,0.000000),
      vec3f(0.066667,1.000000,0.000000),
      vec3f(0.090196,1.000000,0.000000),
      vec3f(0.113725,1.000000,0.000000),
      vec3f(0.137255,1.000000,0.000000),
      vec3f(0.160784,1.000000,0.000000),
      vec3f(0.184314,1.000000,0.000000),
      vec3f(0.207843,1.000000,0.000000),
      vec3f(0.231373,1.000000,0.000000),
      vec3f(0.254902,1.000000,0.000000),
      vec3f(0.278431,1.000000,0.000000),
      vec3f(0.301961,1.000000,0.000000),
      vec3f(0.325490,1.000000,0.000000),
      vec3f(0.345098,1.000000,0.000000),
      vec3f(0.368627,1.000000,0.000000),
      vec3f(0.392157,1.000000,0.000000),
      vec3f(0.415686,1.000000,0.000000),
      vec3f(0.439216,1.000000,0.000000),
      vec3f(0.462745,1.000000,0.000000),
      vec3f(0.486275,1.000000,0.000000),
      vec3f(0.509804,1.000000,0.000000),
      vec3f(0.533333,1.000000,0.000000),
      vec3f(0.556863,1.000000,0.000000),
      vec3f(0.580392,1.000000,0.000000),
      vec3f(0.603922,1.000000,0.000000),
      vec3f(0.627451,1.000000,0.000000),
      vec3f(0.650980,1.000000,0.000000),
      vec3f(0.670588,1.000000,0.000000),
      vec3f(0.694118,1.000000,0.000000),
      vec3f(0.717647,1.000000,0.000000),
      vec3f(0.741176,1.000000,0.000000),
      vec3f(0.764706,1.000000,0.000000),
      vec3f(0.788235,1.000000,0.000000),
      vec3f(0.811765,1.000000,0.000000),
      vec3f(0.835294,1.000000,0.000000),
      vec3f(0.858824,1.000000,0.000000),
      vec3f(0.882353,1.000000,0.000000),
      vec3f(0.905882,1.000000,0.000000),
      vec3f(0.929412,1.000000,0.000000),
      vec3f(0.952941,1.000000,0.000000),
      vec3f(0.976471,1.000000,0.000000),
      vec3f(1.000000,1.000000,0.000000),
      vec3f(1.000000,1.000000,0.000000),
      vec3f(1.000000,0.984314,0.000000),
      vec3f(1.000000,0.972549,0.000000),
      vec3f(1.000000,0.960784,0.000000),
      vec3f(1.000000,0.949020,0.000000),
      vec3f(1.000000,0.937255,0.000000),
      vec3f(1.000000,0.925490,0.000000),
      vec3f(1.000000,0.909804,0.000000),
      vec3f(1.000000,0.898039,0.000000),
      vec3f(1.000000,0.886275,0.000000),
      vec3f(1.000000,0.874510,0.000000),
      vec3f(1.000000,0.862745,0.000000),
      vec3f(1.000000,0.850980,0.000000),
      vec3f(1.000000,0.839216,0.000000),
      vec3f(1.000000,0.823529,0.000000),
      vec3f(1.000000,0.811765,0.000000),
      vec3f(1.000000,0.800000,0.000000),
      vec3f(1.000000,0.788235,0.000000),
      vec3f(1.000000,0.776471,0.000000),
      vec3f(1.000000,0.764706,0.000000),
      vec3f(1.000000,0.752941,0.000000),
      vec3f(1.000000,0.737255,0.000000),
      vec3f(1.000000,0.725490,0.000000),
      vec3f(1.000000,0.713726,0.000000),
      vec3f(1.000000,0.701961,0.000000),
      vec3f(1.000000,0.690196,0.000000),
      vec3f(1.000000,0.678431,0.000000),
      vec3f(1.000000,0.666667,0.000000),
      vec3f(1.000000,0.650980,0.000000),
      vec3f(1.000000,0.639216,0.000000),
      vec3f(1.000000,0.627451,0.000000),
      vec3f(1.000000,0.615686,0.000000),
      vec3f(1.000000,0.603922,0.000000),
      vec3f(1.000000,0.592157,0.000000),
      vec3f(1.000000,0.576471,0.000000),
      vec3f(1.000000,0.564706,0.000000),
      vec3f(1.000000,0.552941,0.000000),
      vec3f(1.000000,0.541176,0.000000),
      vec3f(1.000000,0.529412,0.000000),
      vec3f(1.000000,0.517647,0.000000),
      vec3f(1.000000,0.505882,0.000000),
      vec3f(1.000000,0.490196,0.000000),
      vec3f(1.000000,0.478431,0.000000),
      vec3f(1.000000,0.466667,0.000000),
      vec3f(1.000000,0.454902,0.000000),
      vec3f(1.000000,0.443137,0.000000),
      vec3f(1.000000,0.431373,0.000000),
      vec3f(1.000000,0.419608,0.000000),
      vec3f(1.000000,0.403922,0.000000),
      vec3f(1.000000,0.392157,0.000000),
      vec3f(1.000000,0.380392,0.000000),
      vec3f(1.000000,0.368627,0.000000),
      vec3f(1.000000,0.356863,0.000000),
      vec3f(1.000000,0.345098,0.000000),
      vec3f(1.000000,0.333333,0.000000),
      vec3f(1.000000,0.317647,0.000000),
      vec3f(1.000000,0.305882,0.000000),
      vec3f(1.000000,0.294118,0.000000),
      vec3f(1.000000,0.282353,0.000000),
      vec3f(1.000000,0.270588,0.000000),
      vec3f(1.000000,0.258824,0.000000),
      vec3f(1.000000,0.243137,0.000000),
      vec3f(1.000000,0.231373,0.000000),
      vec3f(1.000000,0.219608,0.000000),
      vec3f(1.000000,0.207843,0.000000),
      vec3f(1.000000,0.196078,0.000000),
      vec3f(1.000000,0.184314,0.000000),
      vec3f(1.000000,0.172549,0.000000),
      vec3f(1.000000,0.156863,0.000000),
      vec3f(1.000000,0.145098,0.000000),
      vec3f(1.000000,0.133333,0.000000),
      vec3f(1.000000,0.121569,0.000000),
      vec3f(1.000000,0.109804,0.000000),
      vec3f(1.000000,0.098039,0.000000),
      vec3f(1.000000,0.086275,0.000000),
      vec3f(1.000000,0.070588,0.000000),
      vec3f(1.000000,0.058824,0.000000),
      vec3f(1.000000,0.047059,0.000000),
      vec3f(1.000000,0.035294,0.000000),
      vec3f(1.000000,0.023529,0.000000),
      vec3f(1.000000,0.011765,0.000000),
      vec3f(1.000000,0.000000,0.000000),
      vec3f(1.000000,0.000000,0.000000)
    };
    const std::vector<float> opacities = 
      {0.000000,0.000001,0.000003,0.000024,0.000076,0.000265,0.000729,0.001895,0.003866,0.007282,0.010976,0.015707,0.018081,0.018921,0.017266,0.013545,0.009273,0.005213,0.002768,0.001223,0.000460,0.000134,0.000037,0.000008,0.000003,0.000014,0.000054,0.000134,0.000387,0.000956,0.002304,0.004507,0.008239,0.014243,0.021591,0.031032,0.039975,0.047286,0.050436,0.048844,0.044266,0.035963,0.026563,0.018081,0.010976,0.006401,0.003287,0.001535,0.000630,0.000265,0.000076,0.000024,0.000037,0.000134,0.000387,0.001084,0.002529,0.005213,0.010388,0.018081,0.027638,0.038607,0.048844,0.055411,0.057137,0.052061,0.044266,0.033439,0.022532,0.013545,0.007282,0.003569,0.001709,0.000630,0.000215,0.000076,0.000265,0.000630,0.001535,0.003287,0.006832,0.012870,0.022532,0.037269,0.055411,0.078468,0.104376,0.126322,0.144603,0.151056,0.147807,0.135263,0.115010,0.092006,0.066292,0.045760,0.028741,0.017266,0.009273,0.004852,0.002304,0.000956,0.000460,0.001223,0.002768,0.005988,0.012217,0.023500,0.041374,0.066292,0.099306,0.135263,0.175103,0.205467,0.226069,0.230343,0.217675,0.189892,0.154351,0.117774,0.080628,0.052061,0.031032,0.017266,0.008746,0.004179,0.001895,0.000729,0.000729,0.001895,0.004179,0.009273,0.018081,0.033439,0.055411,0.087338,0.126322,0.167997,0.205467,0.234670,0.252501,0.247964,0.221846,0.186122,0.147807,0.104376,0.070203,0.042804,0.024494,0.012870,0.006401,0.002768,0.001223,0.000460,0.000838,0.001895,0.004179,0.007751,0.014243,0.024494,0.038607,0.057137,0.078468,0.101821,0.126322,0.141445,0.151056,0.151056,0.138331,0.120580,0.099306,0.076346,0.053719,0.035963,0.022532,0.012870,0.007282,0.003569,0.001709,0.000729,0.000322,0.000134,0.000134,0.000265,0.000460,0.000838,0.001223,0.002093,0.003287,0.004852,0.007282,0.010388,0.014963,0.020676,0.027638,0.035963,0.045760,0.057137,0.068229,0.080628,0.092006,0.101821,0.112289,0.120580,0.123430,0.126322,0.123430,0.120580,0.112289,0.101821,0.092006,0.078468,0.066292,0.055411,0.044266,0.034686,0.026563,0.019786,0.014243,0.010388,0.007282,0.004852,0.003020,0.002093,0.001223,0.000729,0.000460,0.000265,0.000134,0.000076,0.000054,0.000171,0.000460,0.001223,0.002768,0.005988,0.010976,0.019786,0.031032,0.044266,0.057137,0.068229,0.072214,0.072214,0.062524,0.050436,0.037269,0.024494,0.014963,0.008239,0.004179,0.001895,0.000729,0.000265,0.000076,0.000024};
    ospray::cpp::Data colorsData(colors.size(), OSP_FLOAT3, colors.data());
    ospray::cpp::Data opacityData(opacities.size(), OSP_FLOAT, opacities.data());
    colorsData.commit();
    opacityData.commit();

    tfcn.set("colors", colorsData);
    tfcn.set("opacities", opacityData);
  }

  AppState app;

  Model model;
  PIDXVolume pidxVolume(datasetPath, tfcn, timestep);
  // TODO: Update based on volume
  box3f worldBounds(vec3f(-64), vec3f(64));

  std::vector<box3f> regions{pidxVolume.localRegion};
  ospray::cpp::Data regionData(regions.size() * 2, OSP_FLOAT3, regions.data());
  model.set("regions", regionData);
  model.addVolume(pidxVolume.volume);
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

  socket_t listenSocket = nullptr;
  socket_t client = nullptr;
  if (rank == 0) {
    listenSocket = bind(port);
    std::cout << "Rank 0 now listening for client" << std::endl;
    client = listen(listenSocket);
  }

  mpicommon::world.barrier();

  std::vector<vec3f> tfcnColors;
  std::vector<float> tfcnAlphas;
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
      std::cout << "Frame took " << duration_cast<milliseconds>(endFrame - startFrame).count()
        << "ms\n";

      uint32_t *img = (uint32_t*)fb.map(OSP_FB_COLOR);
      // TODO: compress it
      ospcommon::write(client, img, app.fbSize.x * app.fbSize.y * sizeof(uint32_t));
      ospcommon::flush(client);
      fb.unmap(img);

      ospcommon::read(client, &app, sizeof(AppState));
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
      size_t sz = tfcnColors.size();
      MPI_Bcast(&sz, sizeof(size_t), MPI_BYTE, 0, MPI_COMM_WORLD);
      if (rank != 0) {
        tfcnColors.resize(sz);
      }
      MPI_Bcast(tfcnColors.data(), sizeof(vec3f) * tfcnColors.size(), MPI_BYTE,
                0, MPI_COMM_WORLD);

      sz = tfcnAlphas.size();
      MPI_Bcast(&sz, sizeof(size_t), MPI_BYTE, 0, MPI_COMM_WORLD);
      if (rank != 0) {
        tfcnAlphas.resize(sz);
      }
      MPI_Bcast(tfcnAlphas.data(), sizeof(float) * tfcnAlphas.size(), MPI_BYTE,
                0, MPI_COMM_WORLD);

      Data colorData(tfcnColors.size(), OSP_FLOAT3, tfcnColors.data());
      Data alphaData(tfcnAlphas.size(), OSP_FLOAT, tfcnAlphas.data());
      colorData.commit();
      alphaData.commit();

      tfcn.set("colors", colorData);
      tfcn.set("opacities", alphaData);
      tfcn.commit();

      fb.clear(OSP_FB_COLOR | OSP_FB_ACCUM | OSP_FB_VARIANCE);
      app.tfcnChanged = false;
    }
  }

  MPI_Finalize();
  return 0;
}

