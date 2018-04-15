#include <random>
#include <algorithm>
#include <vector>
#include <array>
#include <chrono>
#include <functional>

#include <turbojpeg.h>
#include <GLFW/glfw3.h>

#include "ospcommon/utility/SaveImage.h"
#ifndef USE_TFN_MODULE
# include "ospray/ospray_cpp/TransferFunction.h"
# include "common/sg/transferFunction/TransferFunction.h"
#endif
#include "common/imgui/imgui.h"
#include "widgets/imgui_impl_glfw_gl3.h"
#ifndef USE_TFN_MODULE
# include "widgets/transferFunction.h"
#else
# include "widgets/TransferFunctionWidget.h"
#endif

#include "arcball.h"
#include "util.h"
#include "image_util.h"
#include "client_server.h"

using namespace ospcommon;

std::vector<unsigned char> jpgBuf;

#ifdef USE_TFN_MODULE
std::vector<ospcommon::vec3f> tfn_c;
std::vector<ospcommon::vec2f> tfn_a;
bool tfn_modified = false;
#endif

// Extra stuff we need in GLFW callbacks
struct WindowState {
  Arcball &camera;
  vec2f prevMouse;
  bool cameraChanged;
  AppState &app;
  int currentVariableIdx, currentTimestepIdx;

  WindowState(AppState &app, Arcball &camera)
    : camera(camera), prevMouse(-1), cameraChanged(true), app(app)
  {}
};

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
  if (ImGui::GetIO().WantCaptureKeyboard) {
    return;
  }

  if (action == GLFW_PRESS) {
    switch (key) {
      case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window, true);
        break;
      case 'P':
      case 'p':
        if (!jpgBuf.empty()) {
          std::ofstream fout("screenshot.jpg", std::ios::binary);
          fout.write(reinterpret_cast<const char*>(jpgBuf.data()), jpgBuf.size());
          std::cout << "Screenshot saved to 'screenshot.jpg'\n";
        }
        break;
      default:
        break;
    }
  }
  // Forward on to ImGui
  ImGui_ImplGlfwGL3_KeyCallback(window, key, scancode, action, mods);
}

void cursorPosCallback(GLFWwindow *window, double x, double y) {
  if (ImGui::GetIO().WantCaptureMouse) {
    return;
  }

  WindowState *state = static_cast<WindowState*>(glfwGetWindowUserPointer(window));

  const vec2f mouse(x, y);
  if (state->prevMouse != vec2f(-1)) {
    const bool leftDown =
      glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool rightDown =
      glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool middleDown =
      glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    const vec2f prev = state->prevMouse;

    if (leftDown)
    {
      const vec2f mouseFrom(clamp(prev.x * 2.f / state->app.fbSize.x - 1.f,  -1.f, 1.f),
                            clamp(1.f - 2.f * prev.y / state->app.fbSize.y, -1.f, 1.f));
      const vec2f mouseTo  (clamp(mouse.x * 2.f / state->app.fbSize.x - 1.f,  -1.f, 1.f),
			    clamp(1.f - 2.f * mouse.y / state->app.fbSize.y, -1.f, 1.f));
      state->camera.rotate(mouseFrom, mouseTo);
      state->cameraChanged = true;
    }
    else if (rightDown)
    {
      state->camera.zoom(mouse.y - prev.y);
      state->cameraChanged = true;
    }
    else if (middleDown)
    {
      const vec2f mouseFrom(clamp(prev.x * 2.f / state->app.fbSize.x - 1.f,  -1.f, 1.f),
                            clamp(1.f - 2.f * prev.y / state->app.fbSize.y, -1.f, 1.f));
      const vec2f mouseTo   (clamp(mouse.x * 2.f / state->app.fbSize.x - 1.f,  -1.f, 1.f),
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

int main(int argc, const char **argv)
{
  //------------------------------------------------------------
  std::string serverhost;
  int port = -1;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp("-server", argv[i]) == 0) {
      serverhost = argv[++i];
    } else if (std::strcmp("-port", argv[i]) == 0) {
      port = std::atoi(argv[++i]);
    }
  }
  if (serverhost.empty() || port < 0) {
    throw std::runtime_error("Usage: ./pidx_viewer -server <server host> -port <port>");
  }

  //------------------------------------------------------------  
  AppState app;
  AppData appdata;
  // TODO: Update based on volume?
  box3f worldBounds(vec3f(-64), vec3f(64));
  Arcball arcballCamera(worldBounds);
  // Initialize openGL
  if (!glfwInit()) { return 1; }
  GLFWwindow *window = glfwCreateWindow(app.fbSize.x, app.fbSize.y,
      "PIDX OSPRay Viewer", nullptr, nullptr);
  if (!window) { glfwTerminate(); return 1; }
  glfwMakeContextCurrent(window);

  auto windowState = std::make_shared<WindowState>(app, arcballCamera);
#ifndef USE_TFN_MODULE
  auto transferFcn = std::make_shared<ospray::sg::TransferFunction>();
  auto tfnWidget = std::make_shared<ospray::TransferFunction>(transferFcn);
#else
  auto tfnWidget = 
    std::make_shared<tfn::tfn_widget::TransferFunctionWidget>
    ([&](const std::vector<float>& c, const std::vector<float>& a, const std::array<float, 2>& r) 
     {
       tfn_c.resize(256);
       tfn_a.resize(256);
       std::copy(c.data(), c.data() + c.size(), reinterpret_cast<float*>(tfn_c.data()));
       std::copy(a.data(), a.data() + a.size(), reinterpret_cast<float*>(tfn_a.data()));
       // Question: How can i add valueRange to sg::transferFunction ??
       tfn_modified = true;
     });
#endif
  ImGui_ImplGlfwGL3_Init(window, false);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetCursorPosCallback(window, cursorPosCallback);
  glfwSetWindowUserPointer(window, windowState.get());
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
  glfwSetMouseButtonCallback(window, ImGui_ImplGlfwGL3_MouseButtonCallback);
  glfwSetScrollCallback(window, ImGui_ImplGlfwGL3_ScrollCallback);
  glfwSetCharCallback(window, charCallback);

  JPGDecompressor decompressor;
  ServerConnection server(serverhost, port, app);

  std::vector<std::string> variables;
  std::vector<size_t> timesteps;

  std::vector<uint32_t> imgBuf;
  int frameTime = 0;

  while (!app.quit)
  {
    //--------------------------------
    imgBuf.resize(app.fbSize.x * app.fbSize.y, 0);
    if (server.get_new_frame(jpgBuf, frameTime)) {
      decompressor.decompress(jpgBuf.data(), jpgBuf.size(), app.fbSize.x,
          app.fbSize.y, imgBuf);
    }
#ifndef USE_TFN_MODULE
    const auto tfcnTimeStamp = transferFcn->childrenLastModified();
#endif
    //--------------------------------    
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawPixels(app.fbSize.x, app.fbSize.y, GL_RGBA, GL_UNSIGNED_BYTE, imgBuf.data());
    
    ImGui_ImplGlfwGL3_NewFrame();

#ifndef USE_TFN_MODULE
    tfnWidget->drawUi();
#else
    tfnWidget->drawUI();
#endif
    
    //--------------------------------
    const float DISTANCE = 10.0f;
    static int corner = 0;
    ImVec2 window_pos = ImVec2((corner & 1) ?
        ImGui::GetIO().DisplaySize.x - DISTANCE : DISTANCE,
        (corner & 2) ?
        ImGui::GetIO().DisplaySize.y - DISTANCE : DISTANCE);
    ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    // Transparent background
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));
    if (ImGui::Begin("Volume Information", NULL,
          ImGuiWindowFlags_NoTitleBar|
          ImGuiWindowFlags_NoResize|
          ImGuiWindowFlags_AlwaysAutoResize|
          ImGuiWindowFlags_NoMove|
          ImGuiWindowFlags_NoSavedSettings))
    {      
      if (!timesteps.empty()) {
        app.timestepChanged = ImGui::SliderInt("Timestep",
            &windowState->currentTimestepIdx,
            0, timesteps.size() - 1);
        ImGui::Text("Current Timestep %lu", timesteps[windowState->currentTimestepIdx]);
        if (app.timestepChanged) {
          app.currentTimestep = timesteps[windowState->currentTimestepIdx];
        }
      }
      if (!variables.empty()) {
        app.fieldChanged =
          ImGui::ListBox("variable", &windowState->currentVariableIdx,
              [](void *v, int i, const char **out)
              {
                auto *list = reinterpret_cast<std::vector<std::string>*>(v);
                *out = (*list)[i].c_str();
                return true;
              },
              &variables, variables.size());
        if (app.fieldChanged) {
          appdata.currentVariable = variables[windowState->currentVariableIdx];
        }
      }
      ImGui::Separator();
      if (variables.empty() && timesteps.empty()) {
        ImGui::Text("Waiting for server to load data");
        server.get_metadata(variables, timesteps, appdata.currentVariable,
            app.currentTimestep);

        if (!timesteps.empty()) {
          auto t = std::find(timesteps.begin(), timesteps.end(), app.currentTimestep);
          windowState->currentTimestepIdx = std::distance(timesteps.begin(), t);
        }

        if (!variables.empty()) {
          auto v = std::find(variables.begin(), variables.end(), appdata.currentVariable);
          windowState->currentVariableIdx = std::distance(variables.begin(), v);
        }
      } else {
        ImGui::Text("Last frame took %dms", frameTime);
      }
    }
    ImGui::PopStyleColor();    
    ImGui::End();
    ImGui::Render();
    
    //--------------------------------    
    glfwSwapBuffers(window);

    //--------------------------------    
    glfwPollEvents();
    if (glfwWindowShouldClose(window)) { app.quit = true; }

#ifndef USE_TFN_MODULE
    tfnWidget->render();
#else    
    tfnWidget->render(256);
#endif

#ifndef USE_TFN_MODULE
    if (transferFcn->childrenLastModified() != tfcnTimeStamp) {
      appdata.tfcn_colors =
        transferFcn->child("colors").nodeAs<ospray::sg::DataVector3f>()->v;
      const auto &ospAlpha =
        transferFcn->child("alpha").nodeAs<ospray::sg::DataVector2f>()->v;
#else
    if (tfn_modified) {
      tfn_modified = false;
      appdata.tfcn_colors = tfn_c;
      const auto &ospAlpha = tfn_a;
#endif
      appdata.tfcn_alphas.clear();
      std::transform(ospAlpha.begin(), ospAlpha.end(),
          std::back_inserter(appdata.tfcn_alphas),
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

    server.update_app_state(app, appdata);

    if (app.fbSizeChanged) {
      app.fbSizeChanged = false;
      glViewport(0, 0, app.fbSize.x, app.fbSize.y);
    }
    app.tfcnChanged = false;
  }

  //------------------------------------------------------------
  ImGui_ImplGlfwGL3_Shutdown();
  glfwDestroyWindow(window);

  return 0;
}
