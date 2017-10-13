#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include "ospcommon/networking/Socket.h"
#include "util.h"

// A connection to the render worker server
class ServerConnection {
  std::string server_host;
  int server_port;

  std::vector<unsigned char> jpg_buf;
  bool new_frame;
  std::mutex frame_mutex;

  AppState app_state;
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
  void update_app_state(const AppState &state);

private:
  void connection_thread();
};


