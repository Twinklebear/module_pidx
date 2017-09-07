#include <random>
#include <algorithm>
#include <array>
#include <chrono>
#include <GLFW/glfw3.h>
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
#include "widgets/transferFunction.h"
#include "common/sg/transferFunction/TransferFunction.h"
#include "widgets/imgui_impl_glfw_gl3.h"
#include "common/imgui/imgui.h"
#include "PIDX.h"
#include "arcball.h"
#include "util.h"

using namespace ospray::cpp;
using namespace ospcommon;

using vec3sz = vec_t<size_t, 3>;

#define PIDX_CHECK(F) \
  { \
    PIDX_return_code rc = F; \
    if (rc != PIDX_success) { \
      const std::string er = "PIDX Error at " #F ": " + pidx_error_to_string(rc); \
      std::cerr << er << std::endl; \
      throw std::runtime_error(er); \
    } \
  }

std::string pidx_error_to_string(const PIDX_return_code rc);

// Struct for bcasting out the camera change info and general app state
struct AppState {
  // eye pos, look dir, up dir
  std::array<vec3f, 3> v;
  vec2i fbSize;
  bool cameraChanged, quit, fbSizeChanged, tfcnChanged;

  AppState() : fbSize(1024), cameraChanged(false), quit(false),
    fbSizeChanged(false)
  {}
};

// Extra stuff we need in GLFW callbacks
struct WindowState {
  Arcball &camera;
  vec2f prevMouse;
  bool cameraChanged;
  AppState &app;
  bool isImGuiHovered;

  WindowState(AppState &app, Arcball &camera)
    : camera(camera), prevMouse(-1), cameraChanged(false), app(app),
    isImGuiHovered(false)
  {}
};

struct PIDXVolume {
  std::string datasetPath;
  PIDX_access pidxAccess;
  PIDX_file pidxFile;
  PIDX_point pdims;
  int resolution;
  Volume volume;
  vec3sz fullDims, localDims, localOffset;
  box3f localRegion;
  vec2f valueRange;

  PIDXVolume(const std::string &path) : datasetPath(path), volume("block_bricked_volume") {
    PIDX_CHECK(PIDX_create_access(&pidxAccess));
    PIDX_CHECK(PIDX_set_mpi_access(pidxAccess, MPI_COMM_WORLD));
    update();
  }
  ~PIDXVolume() {
    PIDX_close_access(pidxAccess);
  }
  void update() {
    const int rank = mpicommon::world.rank;
    const int numRanks = mpicommon::world.size;

    PIDX_CHECK(PIDX_file_open(datasetPath.c_str(), PIDX_MODE_RDONLY,
          pidxAccess, pdims, &pidxFile));
    fullDims = vec3sz(pdims[0], pdims[1], pdims[2]);

    PIDX_CHECK(PIDX_set_current_time_step(pidxFile, 0));

    int variableCount = 0;
    PIDX_CHECK(PIDX_get_variable_count(pidxFile, &variableCount));

    PIDX_CHECK(PIDX_set_current_variable_index(pidxFile, 0));
    PIDX_variable variable;
    PIDX_CHECK(PIDX_get_current_variable(pidxFile, &variable));

    int valuesPerSample = 0;
    int bitsPerSample = 0;
    PIDX_CHECK(PIDX_values_per_datatype(variable->type_name, &valuesPerSample,
          &bitsPerSample));
    const int bytesPerSample = bitsPerSample / 8;

    if (rank == 0) {
      std::cout << "Volume dimensions: " << fullDims << "\n"
        << "Variable count = " << variableCount << "\n"
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
    PIDX_CHECK(PIDX_close(pidxFile));

    auto minmax = std::minmax_element(reinterpret_cast<float*>(data.data()),
        reinterpret_cast<float*>(data.data()) + nLocalVals);
    // TODO: Need MPI Allreduce here
    vec2f localValueRange = vec2f(*minmax.first, *minmax.second);
    MPI_Allreduce(&localValueRange.x, &valueRange.x, 1, MPI_FLOAT,
        MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&localValueRange.y, &valueRange.y, 1, MPI_FLOAT,
        MPI_MAX, MPI_COMM_WORLD);
    std::cout << "Value range = [" << valueRange.x
      << ", " << valueRange.y << "]\n";

    // TODO: Parse the IDX type name into the OSPRay type name
    volume.set("voxelType", "float");
    // TODO: This will be the local dimensions later
    volume.set("dimensions", vec3i(localDims));
    volume.set("gridOrigin", vec3f(localOffset) - vec3f(fullDims) / 2.f);
    // TODO: Use the logic box to figure out grid spacing
    //volume.set("gridSpacing", vec3f(dimensions) / vec3f(ospDims));

    // Now we have some row-major data in the array we can pass to an OSPRay volume
    volume.setRegion(data.data(), vec3i(0), vec3i(localDims));
    localRegion = box3f(vec3f(brickId * brickDims) - vec3f(fullDims) / 2.f,
        vec3f(brickId * brickDims + brickDims) - vec3f(fullDims) / 2.f);
  }
};

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
  WindowState *state = static_cast<WindowState*>(glfwGetWindowUserPointer(window));
  if (action == GLFW_PRESS) {
    switch (key) {
      case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window, true);
        break;
      default:
        break;
    }
  }
  // Forward on to ImGui
  ImGui_ImplGlfwGL3_KeyCallback(window, key, scancode, action, mods);
}
void cursorPosCallback(GLFWwindow *window, double x, double y) {
  WindowState *state = static_cast<WindowState*>(glfwGetWindowUserPointer(window));
  if (state->isImGuiHovered) {
    return;
  }
  const vec2f mouse(x, y);
  if (state->prevMouse != vec2f(-1)) {
    const bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const vec2f prev = state->prevMouse;

    if (leftDown) {
      const vec2f mouseFrom(clamp(prev.x * 2.f / state->app.fbSize.x - 1.f,  -1.f, 1.f),
                            clamp(1.f - 2.f * prev.y / state->app.fbSize.y, -1.f, 1.f));
      const vec2f mouseTo(clamp(mouse.x * 2.f / state->app.fbSize.x - 1.f,  -1.f, 1.f),
                          clamp(1.f - 2.f * mouse.y / state->app.fbSize.y, -1.f, 1.f));
      state->camera.rotate(mouseFrom, mouseTo);
      state->cameraChanged = true;
    } else if (rightDown) {
      state->camera.zoom(mouse.y - prev.y);
      state->cameraChanged = true;
    }
  }
  state->prevMouse = mouse;
}
void framebufferSizeCallback(GLFWwindow *window, int width, int height) {
  WindowState *state = static_cast<WindowState*>(glfwGetWindowUserPointer(window));
  state->app.fbSize = vec2i(width, height);
  state->app.fbSizeChanged = true;
}
void charCallback(GLFWwindow *window, unsigned int c) {
  ImGuiIO& io = ImGui::GetIO();
  if (c > 0 && c < 0x10000) {
    io.AddInputCharacter((unsigned short)c);
  }
}

int main(int argc, char **argv) {
  int provided = 0;
  // TODO: OpenMPI sucks as always and doesn't support pt2pt one-sided
  // communication with thread multiple. This can trigger a hang in OSPRay
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SINGLE, &provided);

  std::string datasetPath;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp("-dataset", argv[i]) == 0) {
      datasetPath = argv[++i];
    }
  }
  if (datasetPath.empty()) {
    throw std::runtime_error("Usage: ./pidx_viewer -dataset <dataset.idx>");
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

  PIDXVolume pidxVolume(datasetPath);
  tfcn.set("valueRange", vec2f(pidxVolume.valueRange.x, pidxVolume.valueRange.y));
  tfcn.commit();

  pidxVolume.volume.set("transferFunction", tfcn);
  pidxVolume.volume.commit();

  AppState app;
  Model model;
  box3f worldBounds(vec3f(-64), vec3f(64));
  Arcball arcballCamera(worldBounds);

  // TODO: Set up the regions
  std::vector<box3f> regions{pidxVolume.localRegion};
  ospray::cpp::Data regionData(regions.size() * 2, OSP_FLOAT3, regions.data());
  model.set("regions", regionData);
  model.addVolume(pidxVolume.volume);
  model.commit();

  Camera camera("perspective");
  camera.set("pos", arcballCamera.eyePos());
  camera.set("dir", arcballCamera.lookDir());
  camera.set("up", arcballCamera.upDir());
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

  mpicommon::world.barrier();

  std::shared_ptr<ospray::sg::TransferFunction> transferFcn;
  std::shared_ptr<ospray::TransferFunction> tfnWidget;
  std::shared_ptr<WindowState> windowState;
  GLFWwindow *window = nullptr;
  if (rank == 0) {
    if (!glfwInit()) {
      return 1;
    }
    window = glfwCreateWindow(app.fbSize.x, app.fbSize.y,
        "PIDX OSPRay Viewer", nullptr, nullptr);
    if (!window) {
      glfwTerminate();
      return 1;
    }
    glfwMakeContextCurrent(window);

    windowState = std::make_shared<WindowState>(app, arcballCamera);
    transferFcn = std::make_shared<ospray::sg::TransferFunction>();
    tfnWidget = std::make_shared<ospray::TransferFunction>(transferFcn);

    ImGui_ImplGlfwGL3_Init(window, false);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetWindowUserPointer(window, windowState.get());
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    glfwSetMouseButtonCallback(window, ImGui_ImplGlfwGL3_MouseButtonCallback);
    glfwSetScrollCallback(window, ImGui_ImplGlfwGL3_ScrollCallback);
    glfwSetCharCallback(window, charCallback);
  }

  std::vector<vec3f> tfcnColors;
  std::vector<float> tfcnAlphas;
  while (!app.quit) {
    if (app.cameraChanged) {
      camera.set("pos", app.v[0]);
      camera.set("dir", app.v[1]);
      camera.set("up", app.v[2]);
      camera.commit();

      fb.clear(OSP_FB_COLOR | OSP_FB_ACCUM | OSP_FB_VARIANCE);
      app.cameraChanged = false;
    }
    renderer.renderFrame(fb, OSP_FB_COLOR);

    if (rank == 0) {
      glClear(GL_COLOR_BUFFER_BIT);
      uint32_t *img = (uint32_t*)fb.map(OSP_FB_COLOR);
      glDrawPixels(app.fbSize.x, app.fbSize.y, GL_RGBA, GL_UNSIGNED_BYTE, img);
      fb.unmap(img);

      const auto tfcnTimeStamp = transferFcn->childrenLastModified();

      ImGui_ImplGlfwGL3_NewFrame();
      tfnWidget->drawUi();
      ImGui::Render();

      glfwSwapBuffers(window);

      glfwPollEvents();
      if (glfwWindowShouldClose(window)) {
        app.quit = true;
      }

      tfnWidget->render();

      if (transferFcn->childrenLastModified() != tfcnTimeStamp) {
        tfcnColors = transferFcn->child("colors").nodeAs<ospray::sg::DataVector3f>()->v;
        const auto &ospAlpha = transferFcn->child("alpha").nodeAs<ospray::sg::DataVector2f>()->v;
        tfcnAlphas.clear();
        std::transform(ospAlpha.begin(), ospAlpha.end(), std::back_inserter(tfcnAlphas),
            [](const vec2f &a) { return a.y; });
        app.tfcnChanged = true;
      }

      const vec3f eye = windowState->camera.eyePos();
      const vec3f look = windowState->camera.lookDir();
      const vec3f up = windowState->camera.upDir();
      app.v[0] = vec3f(eye.x, eye.y, eye.z);
      app.v[1] = vec3f(look.x, look.y, look.z);
      app.v[2] = vec3f(up.x, up.y, up.z);
      app.cameraChanged = windowState->cameraChanged;
      windowState->cameraChanged = false;
      windowState->isImGuiHovered = ImGui::IsMouseHoveringAnyWindow();
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
      if (rank == 0) {
        glViewport(0, 0, app.fbSize.x, app.fbSize.y);
      }
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
  if (rank == 0) {
      ImGui_ImplGlfwGL3_Shutdown();
      glfwDestroyWindow(window);
  }

  MPI_Finalize();
  return 0;
}

std::string pidx_error_to_string(const PIDX_return_code rc) {
  if (rc == PIDX_success) return "PIDX_success";
  else if (rc == PIDX_err_id) return "PIDX_err_id";
  else if (rc == PIDX_err_unsupported_flags) return "PIDX_err_unsupported_flags";
  else if (rc == PIDX_err_file_exists) return "PIDX_err_file_exists";
  else if (rc == PIDX_err_name) return "PIDX_err_name";
  else if (rc == PIDX_err_box) return "PIDX_err_box";
  else if (rc == PIDX_err_file) return "PIDX_err_file";
  else if (rc == PIDX_err_time) return "PIDX_err_time";
  else if (rc == PIDX_err_block) return "PIDX_err_block";
  else if (rc == PIDX_err_comm) return "PIDX_err_comm";
  else if (rc == PIDX_err_count) return "PIDX_err_count";
  else if (rc == PIDX_err_size) return "PIDX_err_size";
  else if (rc == PIDX_err_offset) return "PIDX_err_offset";
  else if (rc == PIDX_err_type) return "PIDX_err_type";
  else if (rc == PIDX_err_variable) return "PIDX_err_variable";
  else if (rc == PIDX_err_not_implemented) return "PIDX_err_not_implemented";
  else if (rc == PIDX_err_point) return "PIDX_err_point";
  else if (rc == PIDX_err_access) return "PIDX_err_access";
  else if (rc == PIDX_err_mpi) return "PIDX_err_mpi";
  else if (rc == PIDX_err_rst) return "PIDX_err_rst";
  else if (rc == PIDX_err_chunk) return "PIDX_err_chunk";
  else if (rc == PIDX_err_compress) return "PIDX_err_compress";
  else if (rc == PIDX_err_hz) return "PIDX_err_hz";
  else if (rc == PIDX_err_agg) return "PIDX_err_agg";
  else if (rc == PIDX_err_io) return "PIDX_err_io";
  else if (rc == PIDX_err_unsupported_compression_type) return "PIDX_err_unsupported_compression_type";
  else if (rc == PIDX_err_close) return "PIDX_err_close";
  else if (rc == PIDX_err_flush) return "PIDX_err_flush";
  else if (rc == PIDX_err_header) return "PIDX_err_header";
  else if (rc == PIDX_err_wavelet) return "PIDX_err_wavelet";
  else return "Unknown PIDX Error";
}
