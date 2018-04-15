#include <cmath>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "ospcommon/vec.h"
#include "util.h"

using namespace ospcommon;

AppState::AppState() : fbSize(1024), cameraChanged(false), quit(false),
  fbSizeChanged(false), tfcnChanged(false), timestepChanged(false),
  fieldChanged(false)
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
bool operator<(const UintahTimestep &a, const UintahTimestep &b) {
  return a.timestep < b.timestep;
}

std::set<UintahTimestep> collectUintahTimesteps(const std::vector<std::string> &dirs) {
  std::set<UintahTimestep> timesteps;
  for (const auto &dir : dirs) {
    DIR *dp = opendir(dir.c_str());
    if (!dp) {
      throw std::runtime_error("failed to open directory: " + dir);
    }

    for (dirent *e = readdir(dp); e; e = readdir(dp)) {
      const std::string idxFile = dir + "/" + std::string(e->d_name) + "/l0/CCVars.idx";
      struct stat fileStat = {0};
      if (stat(idxFile.c_str(), &fileStat) == 0) {
        // The timestep files are in the pattern t######, so take out the t
        const std::string fname = e->d_name + 1;
        timesteps.emplace(size_t(std::stoull(e->d_name + 1)), idxFile);
      }
    }
  }
  return timesteps;
}
