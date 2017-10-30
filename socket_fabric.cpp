#include <iostream>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include "socket_fabric.h"

SocketFabric::SocketFabric(const uint16_t port) {
  int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
#ifdef SO_REUSEADDR
  {
    int flag = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(int));
  }
#endif
  struct sockaddr_in serv_addr = {0};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(listen_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    throw std::runtime_error("Failed to bind to socket");
  }

  if (listen(listen_socket, 1) < 0) {
    throw std::runtime_error("Failed to listen on socket");
  }

  // Accept an incoming connection
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  sock_fd = accept(listen_socket, (struct sockaddr*)&addr, &len);
  if (sock_fd == -1) {
    throw std::runtime_error("Failed to accept connection");
  }

#ifdef TCP_NODELAY
  {
    int flag = 1; 
    setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
  }
#endif

#ifdef SO_NOSIGPIPE
  {
    int flag = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&flag, sizeof(int));
  }
#endif
  close(listen_socket);
}
SocketFabric::SocketFabric(const std::string &hostname, const uint16_t port) {
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);

  struct hostent* server = gethostbyname(hostname.c_str());
  if (server == nullptr) {
    throw std::runtime_error("Hostname " + hostname + " not found!");
  }

  struct sockaddr_in serv_addr = {0};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  std::memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);

  if (connect(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    throw std::runtime_error("Connection to " + hostname + " on port " + std::to_string(port) + " failed");
  }

#ifdef TCP_NODELAY
  {
    int flag = 1; 
    setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
  }
#endif

#ifdef SO_NOSIGPIPE
  {
    int flag = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&flag, sizeof(int));
  }
#endif
}
SocketFabric::~SocketFabric() {
  close(sock_fd);
}

void SocketFabric::send(void *mem, size_t s) {
  if (write(sock_fd, mem, s) != s) {
    throw std::runtime_error("Failed to write all bytes");
  }
}
size_t SocketFabric::read(void *&mem) {
  if (buffer.empty()) {
    buffer.resize(4096, 0);
  }
  const size_t s = ::read(sock_fd, buffer.data(), buffer.size());
  mem = buffer.data();
  return s;
}

