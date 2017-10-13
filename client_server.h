#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include "ospcommon/networking/Socket.h"
#include "util.h"
#include "image_util.h"

// A connection to the render worker server
class ServerConnection {
  std::string server_host;
  int server_port;

  std::vector<unsigned char> jpg_buf;
  bool new_frame;
  std::mutex frame_mutex;

  AppState app_state;
  AppData app_data;
  std::mutex state_mutex;

  std::thread server_thread;

public:
  ServerConnection(const std::string &server, const int port,
      const AppState &app_state);
  ~ServerConnection();
  /* Get the new JPG recieved from the network, if we've got a new one,
   * otherwise the buf is unchanged.
   */
  bool get_new_frame(std::vector<unsigned char> &buf);
  // Update the app state to be sent over the network for the next frame
  void update_app_state(const AppState &state, const AppData &data);

private:
  void connection_thread();
};

// A client connecting to the render worker server
class ClientConnection {
  JPGCompressor compressor;

  ospcommon::socket_t listen_socket, client;

public:
  ClientConnection(const int port);
  void send_frame(const uint32_t *img, int width, int height);
  void recieve_app_state(AppState &app, AppData &data);
};
