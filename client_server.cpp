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
void ServerConnection::update_app_state(const AppState &state) {
  std::lock_guard<std::mutex> lock(state_mutex);
  app_state = state;
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
      ospcommon::flush(render_server);
      if (app_state.quit) {
        return;
      }
    }
  }
}

