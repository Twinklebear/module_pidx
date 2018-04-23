#pragma once

#include <vector>
#include <atomic>
#include <thread>
#include <set>
#include <mutex>
#include "ospcommon/networking/Socket.h"
#include "ospcommon/networking/SocketFabric.h"
#include "ospcommon/networking/BufferedDataStreaming.h"
#include "util.h"
#include "image_util.h"

// A connection to the render worker server
class ServerConnection {
  std::string server_host;
  int server_port;

  std::vector<unsigned char> jpg_buf;
  int frame_time;
  bool new_frame;
  std::mutex frame_mutex;

  AppState app_state;
  AppData app_data;
  std::mutex state_mutex;

  std::thread server_thread;
  std::vector<std::string> variables;
  std::vector<size_t> timesteps;
  std::atomic<bool> have_metadata;

public:
  ServerConnection(const std::string &server, const int port,
      const AppState &app_state);
  ~ServerConnection();
  /* Check if we've gotten metadata back from the server, returns true
   * if we have, in which case the vectors will contain the corresponding
   * meta data returned.
   */
  bool get_metadata(std::vector<std::string> &vars,
      std::vector<size_t> &timesteps, std::string &variableName,
      size_t &timestep);
  /* Get the new JPG recieved from the network, if we've got a new one,
   * otherwise the buf is unchanged.
   */
  bool get_new_frame(std::vector<unsigned char> &buf, int &frame_time);
  // Update the app state to be sent over the network for the next frame
  void update_app_state(const AppState &state, const AppData &data);

private:
  void connection_thread();
};

// A client connecting to the render worker server
class ClientConnection {
  JPGCompressor compressor;
  ospcommon::networking::SocketFabric fabric;
  ospcommon::networking::BufferedReadStream read_stream;
  ospcommon::networking::BufferedWriteStream write_stream;

public:
  ClientConnection(const int port);
  void send_metadata(const std::vector<std::string> &vars,
      const std::set<UintahTimestep> &timesteps,
      const std::string &variableName, const size_t timestep);
  void send_frame(uint32_t *img, int width, int height, int frame_time);
  void recieve_app_state(AppState &app, AppData &data);
};

