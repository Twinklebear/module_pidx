#include <iostream>
#include <unistd.h>
#include "client_server.h"

ServerConnection::ServerConnection(const std::string &server, const int port,
    const AppState &app_state)
  : server_host(server), server_port(port), new_frame(false), app_state(app_state),
  have_metadata(false)
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
bool ServerConnection::get_metadata(std::vector<std::string> &vars,
      std::vector<size_t> &times, std::string &variableName,
      size_t &timestep)
{
  if (have_metadata) {
    vars = variables;
    times = timesteps;
    variableName = app_data.currentVariable;
    timestep = app_state.currentTimestep;
    return true;
  }
  return false;
}
bool ServerConnection::get_new_frame(std::vector<unsigned char> &buf, int &time) {
  std::lock_guard<std::mutex> lock(frame_mutex);
  if (new_frame) {
    buf = jpg_buf;
    time = frame_time;
    new_frame = false;
    return true;
  }
  return false;
}
void ServerConnection::update_app_state(const AppState &state, const AppData &data) {
  std::lock_guard<std::mutex> lock(state_mutex);
  app_state.v = state.v;
  app_state.fbSize = state.fbSize;
  if (state.timestepChanged) {
    app_state.currentTimestep = state.currentTimestep;
  }
  app_state.cameraChanged = state.cameraChanged ? state.cameraChanged : app_state.cameraChanged;
  app_state.fbSizeChanged = state.fbSizeChanged ? state.fbSizeChanged : app_state.fbSizeChanged;
  app_state.tfcnChanged = state.tfcnChanged ? state.tfcnChanged : app_state.tfcnChanged;
  app_state.timestepChanged = state.timestepChanged ? state.timestepChanged : app_state.timestepChanged;
  app_state.fieldChanged = state.fieldChanged ? state.fieldChanged : app_state.fieldChanged;

  if (state.fieldChanged) {
    app_data.currentVariable = data.currentVariable;
  }
  app_data.tfcn_colors = data.tfcn_colors;
  app_data.tfcn_alphas = data.tfcn_alphas;
}
void ServerConnection::connection_thread() {
  SocketFabric fabric(server_host, server_port);
  ospcommon::networking::BufferedReadStream read_stream(fabric);
  ospcommon::networking::BufferedWriteStream write_stream(fabric);

  // Receive metadata from the server
  read_stream >> variables >> timesteps >> app_data.currentVariable >> app_state.currentTimestep;
  have_metadata = true;

  while (true) {
    // Receive a frame from the server
    {
      unsigned long jpg_size = 0;
      read_stream >> jpg_size;

      std::lock_guard<std::mutex> lock(frame_mutex);
      jpg_buf.resize(jpg_size, 0);
      read_stream.read(jpg_buf.data(), jpg_size);
      read_stream >> frame_time;
      new_frame = true;
    }

    // Send over the latest app state
    {
      std::lock_guard<std::mutex> lock(state_mutex);
      write_stream.write(&app_state, sizeof(AppState));
      if (app_state.fieldChanged) {
        write_stream << app_data.currentVariable;
      }
      if (app_state.tfcnChanged) {
        write_stream << app_data.tfcn_colors << app_data.tfcn_alphas;
      }
      write_stream.flush();

      if (app_state.quit) {
        return;
      }
      app_state.tfcnChanged = false;
      app_state.cameraChanged = false;
      app_state.fbSizeChanged = false;
      app_state.timestepChanged = false;
      app_state.fieldChanged = false;
    }
  }
}

ClientConnection::ClientConnection(const int port)
  : compressor(90), fabric(port), read_stream(fabric), write_stream(fabric)
{}
void ClientConnection::send_metadata(const std::vector<std::string> &vars,
    const std::set<UintahTimestep> &timesteps, const std::string &variableName,
    const size_t timestep)
{
  std::vector<size_t> times;
  for (const auto &t : timesteps) {
    times.push_back(t.timestep);
  }
  write_stream << vars << times << variableName << timestep;
  write_stream.flush();
}
void ClientConnection::send_frame(uint32_t *img, int width, int height, int frame_time) {
  auto jpg = compressor.compress(img, width, height);
  write_stream << jpg.second;
  write_stream.write(jpg.first, jpg.second);
  write_stream << frame_time;
  write_stream.flush();
}
void ClientConnection::recieve_app_state(AppState &app, AppData &data) {
  read_stream.read(&app, sizeof(AppState));
  if (app.fieldChanged) {
    read_stream >> data.currentVariable;
  }
  if (app.tfcnChanged) {
    read_stream >> data.tfcn_colors >> data.tfcn_alphas;
  }
}

