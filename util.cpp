#include <cmath>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "ospcommon/vec.h"
#include "PIDX.h"
#include "util.h"

using namespace ospcommon;

AppState::AppState() : fbSize(1024), cameraChanged(false), quit(false),
  fbSizeChanged(false), tfcnChanged(false)
{}

bool computeDivisor(int x, int &divisor) {
  int upperBound = std::sqrt(x);
  for (int i = 2; i <= upperBound; ++i) {
    if (x % i == 0) {
      divisor = i;
      return true;
    }
  }
  return false;
}
vec3i computeGrid(int num) {
  vec3i grid(1);
  int axis = 0;
  int divisor = 0;
  while (computeDivisor(num, divisor)) {
    grid[axis] *= divisor;
    num /= divisor;
    axis = (axis + 1) % 3;
  }
  if (num != 1) {
    grid[axis] *= num;
  }
  return grid;
}

std::array<int, 3> computeGhostFaces(const vec3i &brickId, const vec3i &grid) {
  std::array<int, 3> faces = {NEITHER_FACE, NEITHER_FACE, NEITHER_FACE};
  for (size_t i = 0; i < 3; ++i) {
    if (brickId[i] < grid[i] - 1) {
      faces[i] |= POS_FACE;
    }
    if (brickId[i] > 0) {
      faces[i] |= NEG_FACE;
    }
  }
  return faces;
}

UintahTimestep::UintahTimestep(const size_t timestep, const std::string &path)
  : timestep(timestep), path(path)
{}

std::vector<UintahTimestep> collectUintahTimesteps(const std::string &dir) {
  DIR *dp = opendir(dir.c_str());
  if (!dp) {
    throw std::runtime_error("failed to open directory: " + dir);
  }

  std::vector<UintahTimestep> timesteps;
  for (dirent *e = readdir(dp); e; e = readdir(dp)) {
    const std::string idxFile = dir + "/" + std::string(e->d_name) + "/l0/CCVars.idx";
    struct stat fileStat = {0};
    if (stat(idxFile.c_str(), &fileStat) == 0) {
      // The timestep files are in the pattern t######, so take out the t
      const std::string fname = e->d_name + 1;
      timesteps.emplace_back(size_t(std::stoull(e->d_name + 1)), idxFile);
    } else {
      std::cout << "Non-dir: " << e->d_name << "\n";
    }
  }
  return timesteps;
}

std::string pidx_error_to_string(const PIDX_return_code rc) {
  if (rc == PIDX_success) return "PIDX_success";
  else if (rc == PIDX_err_id) return "PIDX_err_id";
  else if (rc == PIDX_err_unsupported_flags) return "PIDX_err_unsupported_flags";
  else if (rc == PIDX_err_file_exists) return "PIDX_err_file_exists";
  else if (rc == PIDX_err_name) return "PIDX_err_name";
  else if (rc == PIDX_err_box) return "PIDX_err_box";
  else if (rc == PIDX_err_file) return "PIDX_err_file";
  else if (rc == PIDX_err_time) return "PIDX_err_time";
  else if (rc == PIDX_err_block) return "PIDX_err_block";
  else if (rc == PIDX_err_comm) return "PIDX_err_comm";
  else if (rc == PIDX_err_count) return "PIDX_err_count";
  else if (rc == PIDX_err_size) return "PIDX_err_size";
  else if (rc == PIDX_err_offset) return "PIDX_err_offset";
  else if (rc == PIDX_err_type) return "PIDX_err_type";
  else if (rc == PIDX_err_variable) return "PIDX_err_variable";
  else if (rc == PIDX_err_not_implemented) return "PIDX_err_not_implemented";
  else if (rc == PIDX_err_point) return "PIDX_err_point";
  else if (rc == PIDX_err_access) return "PIDX_err_access";
  else if (rc == PIDX_err_mpi) return "PIDX_err_mpi";
  else if (rc == PIDX_err_rst) return "PIDX_err_rst";
  else if (rc == PIDX_err_chunk) return "PIDX_err_chunk";
  else if (rc == PIDX_err_compress) return "PIDX_err_compress";
  else if (rc == PIDX_err_hz) return "PIDX_err_hz";
  else if (rc == PIDX_err_agg) return "PIDX_err_agg";
  else if (rc == PIDX_err_io) return "PIDX_err_io";
  else if (rc == PIDX_err_unsupported_compression_type) return "PIDX_err_unsupported_compression_type";
  else if (rc == PIDX_err_close) return "PIDX_err_close";
  else if (rc == PIDX_err_flush) return "PIDX_err_flush";
  else if (rc == PIDX_err_header) return "PIDX_err_header";
  else if (rc == PIDX_err_wavelet) return "PIDX_err_wavelet";
  else return "Unknown PIDX Error";
}
