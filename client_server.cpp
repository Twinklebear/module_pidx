#include <iostream>
#include <unistd.h>
#include "client_server.h"

ServerConnection::ServerConnection(const std::string &server, const int port,
    const AppState &app_state)
  : server_host(server), server_port(port), new_frame(false), app_state(app_state)
{
  server_thread = std::thread([&](){ connection_thread(); });
}
ServerConnection::~ServerConnection() {
  {
    std::lock_guard<std::mutex> lock(state_mutex);
    app_state.quit = true;
  }
  server_thread.join();
}
bool ServerConnection::get_new_frame(std::vector<unsigned char> &buf) {
  std::lock_guard<std::mutex> lock(frame_mutex);
  if (new_frame) {
    buf = jpg_buf;
    new_frame = false;
    return true;
  }
  return false;
}
void ServerConnection::update_app_state(const AppState &state, const AppData &data) {
  std::lock_guard<std::mutex> lock(state_mutex);
  app_state.v = state.v;
  app_state.fbSize = state.fbSize;
  app_state.cameraChanged = state.cameraChanged ? state.cameraChanged : app_state.cameraChanged;
  app_state.fbSizeChanged = state.fbSizeChanged ? state.fbSizeChanged : app_state.fbSizeChanged;
  app_state.tfcnChanged = state.tfcnChanged ? state.tfcnChanged : app_state.tfcnChanged;

  app_data = data;
}
void ServerConnection::connection_thread() {
  ospcommon::socket_t render_server = ospcommon::connect(server_host.c_str(), server_port);
  while (true) {
    // Receive a frame from the server
    {
      unsigned long jpg_size = 0;
      ospcommon::read(render_server, &jpg_size, sizeof(jpg_size));

      std::lock_guard<std::mutex> lock(frame_mutex);
      jpg_buf.resize(jpg_size, 0);
      ospcommon::read(render_server, jpg_buf.data(), jpg_size);
      new_frame = true;
    }

    // Send over the latest app state
    {
      std::lock_guard<std::mutex> lock(state_mutex);
      ospcommon::write(render_server, &app_state, sizeof(AppState));
      if (app_state.tfcnChanged) {
        size_t size = app_data.tfcn_colors.size();
        ospcommon::write(render_server, &size, sizeof(size_t));
        ospcommon::write(render_server, app_data.tfcn_colors.data(),
            app_data.tfcn_colors.size() * sizeof(ospcommon::vec3f));

        size = app_data.tfcn_alphas.size();
        ospcommon::write(render_server, &size, sizeof(size_t));
        ospcommon::write(render_server, app_data.tfcn_alphas.data(),
            app_data.tfcn_alphas.size() * sizeof(float));
      }
      ospcommon::flush(render_server);
      if (app_state.quit) {
        return;
      }
      app_state.tfcnChanged = false;
      app_state.cameraChanged = false;
      app_state.fbSizeChanged = false;
    }
  }
}

ClientConnection::ClientConnection(const int port) : compressor(90) {
  listen_socket = ospcommon::bind(port);
  char hostname[1024] = {0};
  gethostname(hostname, 1023);
  std::cout << "Now listening for client on "
    << hostname << ":" << port << std::endl;

  client = ospcommon::listen(listen_socket);
}
void ClientConnection::send_frame(const uint32_t *img, int width, int height) {
  auto jpg = compressor.compress(img, width, height);
  ospcommon::write(client, &jpg.second, sizeof(jpg.second));
  ospcommon::write(client, jpg.first, jpg.second);
  ospcommon::flush(client);
}
void ClientConnection::recieve_app_state(AppState &app, AppData &data) {
  ospcommon::read(client, &app, sizeof(AppState));
  if (app.tfcnChanged) {
    size_t size = 0;
    ospcommon::read(client, &size, sizeof(size_t));
    data.tfcn_colors.resize(size, ospcommon::vec3f(0));
    ospcommon::read(client, data.tfcn_colors.data(),
        data.tfcn_colors.size() * sizeof(ospcommon::vec3f));

    ospcommon::read(client, &size, sizeof(size_t));
    data.tfcn_alphas.resize(size, 0.f);
    ospcommon::read(client, data.tfcn_alphas.data(),
        data.tfcn_alphas.size() * sizeof(float));
  }
}

