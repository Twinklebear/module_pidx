#include <random>
#include <algorithm>
#include <vector>
#include <array>
#include <chrono>
#include <turbojpeg.h>
#include <GLFW/glfw3.h>
#include "ospray/ospray_cpp/TransferFunction.h"
#include "ospcommon/networking/Socket.h"
#include "ospcommon/utility/SaveImage.h"
#include "widgets/transferFunction.h"
#include "common/sg/transferFunction/TransferFunction.h"
#include "widgets/imgui_impl_glfw_gl3.h"
#include "common/imgui/imgui.h"
#include "arcball.h"
#include "util.h"
#include "image_util.h"

using namespace ospray::cpp;
using namespace ospcommon;

const uint32_t* imgBufData = nullptr;
vec2i           imgBufSize;

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

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
  WindowState *state = static_cast<WindowState*>(glfwGetWindowUserPointer(window));
  if (action == GLFW_PRESS) {
    switch (key) {
      case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window, true);
        break;
      case 'P':
      case 'p':
	if (imgBufData)
	  utility::writePPM("screenshot.ppm",imgBufSize.x,imgBufSize.y, imgBufData);
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
    const bool middleDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
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
    } else if (middleDown) {
      const vec2f mouseFrom(clamp(prev.x * 2.f / state->app.fbSize.x - 1.f,  -1.f, 1.f),
                            clamp(1.f - 2.f * prev.y / state->app.fbSize.y, -1.f, 1.f));
      const vec2f mouseTo(clamp(mouse.x * 2.f / state->app.fbSize.x - 1.f,  -1.f, 1.f),
                          clamp(1.f - 2.f * mouse.y / state->app.fbSize.y, -1.f, 1.f));
      const vec2f mouseDelta = mouseTo - mouseFrom;
      state->camera.pan(mouseDelta);
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
  std::string server;
  int port = -1;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp("-server", argv[i]) == 0) {
      server = argv[++i];
    } else if (std::strcmp("-port", argv[i]) == 0) {
      port = std::atoi(argv[++i]);
    }
  }
  if (server.empty() || port < 0) {
    throw std::runtime_error("Usage: ./pidx_viewer -server <server host> -port <port>");
  }

  AppState app;
  // TODO: Update based on volume?
  box3f worldBounds(vec3f(-64), vec3f(64));
  Arcball arcballCamera(worldBounds);

  if (!glfwInit()) {
    return 1;
  }
  GLFWwindow *window = glfwCreateWindow(app.fbSize.x, app.fbSize.y,
      "PIDX OSPRay Viewer", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);

  auto windowState = std::make_shared<WindowState>(app, arcballCamera);
  //auto transferFcn = std::make_shared<ospray::sg::TransferFunction>();
  //auto tfnWidget = std::make_shared<ospray::TransferFunction>(transferFcn);

  ImGui_ImplGlfwGL3_Init(window, false);

  glfwSetKeyCallback(window, keyCallback);
  glfwSetCursorPosCallback(window, cursorPosCallback);
  glfwSetWindowUserPointer(window, windowState.get());
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

  glfwSetMouseButtonCallback(window, ImGui_ImplGlfwGL3_MouseButtonCallback);
  glfwSetScrollCallback(window, ImGui_ImplGlfwGL3_ScrollCallback);
  glfwSetCharCallback(window, charCallback);

  tjhandle decompressor = tjInitDecompress();
  socket_t renderServer = connect(server.c_str(), port);

  std::vector<vec3f> tfcnColors;
  std::vector<float> tfcnAlphas;
  while (!app.quit) {
    // Receive frame from network
    /*
    int imgBytes = ospcommon::read_int(renderServer);
    std::vector<char> jpgBuf(imgBytes, 0);
    ospcommon::read(renderServer, jpgBuf.data(), imgBytes);

    std::vector<char> imgBuf(sizeof(uint32_t) * app.fbSize.x, app.fbSize.y, 0);
    tjDecompress2(decompressor, jpgBuf.data(), imgBytes, imgBuf.data(),
        app.fbSize.x, width * 4, app.fbSize.y, TJPF_RGBA, 0);
    */
    std::vector<char> imgBuf(sizeof(uint32_t) * app.fbSize.x * app.fbSize.y, 0);
    ospcommon::read(renderServer, imgBuf.data(), imgBuf.size());
    imgBufData = (uint32_t*) imgBuf.data();
    imgBufSize = app.fbSize;

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawPixels(app.fbSize.x, app.fbSize.y, GL_RGBA, GL_UNSIGNED_BYTE, imgBuf.data());

    //const auto tfcnTimeStamp = transferFcn->childrenLastModified();

    ImGui_ImplGlfwGL3_NewFrame();

    //tfnWidget->drawUi();

    ImGui::Render();

    glfwSwapBuffers(window);

    glfwPollEvents();
    if (glfwWindowShouldClose(window)) {
      app.quit = true;
    }

    //tfnWidget->render();

    /*
    if (transferFcn->childrenLastModified() != tfcnTimeStamp) {
      tfcnColors = transferFcn->child("colors").nodeAs<ospray::sg::DataVector3f>()->v;
      const auto &ospAlpha = transferFcn->child("alpha").nodeAs<ospray::sg::DataVector2f>()->v;
      tfcnAlphas.clear();
      std::transform(ospAlpha.begin(), ospAlpha.end(), std::back_inserter(tfcnAlphas),
          [](const vec2f &a) {
            return a.y;
          });
      app.tfcnChanged = true;
    }
    */

    const vec3f eye = windowState->camera.eyePos();
    const vec3f look = windowState->camera.lookDir();
    const vec3f up = windowState->camera.upDir();
    app.v[0] = vec3f(eye.x, eye.y, eye.z);
    app.v[1] = vec3f(look.x, look.y, look.z);
    app.v[2] = vec3f(up.x, up.y, up.z);
    app.cameraChanged = windowState->cameraChanged;
    windowState->cameraChanged = false;
    windowState->isImGuiHovered = ImGui::IsMouseHoveringAnyWindow();

    ospcommon::write(renderServer, &app, sizeof(AppState));
    ospcommon::flush(renderServer);

    if (app.fbSizeChanged) {
      app.fbSizeChanged = false;
      glViewport(0, 0, app.fbSize.x, app.fbSize.y);
    }
  }

  tjDestroy(decompressor);
  ImGui_ImplGlfwGL3_Shutdown();
  glfwDestroyWindow(window);

  return 0;
}

