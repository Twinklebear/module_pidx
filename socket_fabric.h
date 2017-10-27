#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "ospcommon/networking/Fabric.h"

struct SocketFabric : public ospcommon::networking::Fabric {
  int sock_fd;
  std::vector<char> buffer;

public:
  SocketFabric(const uint16_t port);
  SocketFabric(const std::string &hostname, const uint16_t port);
  ~SocketFabric();

  SocketFabric(const SocketFabric&) = delete;
  SocketFabric& operator=(const SocketFabric&) = delete;

  virtual void send(void *mem, size_t s) override;
  virtual size_t read(void *&mem) override;
};

