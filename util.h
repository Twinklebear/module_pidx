#pragma once

#include <array>
#include <set>
#include <vector>
#include "ospcommon/vec.h"
#include "PIDX.h"

using vec3sz = ospcommon::vec_t<size_t, 3>;

// Struct for bcasting out the camera change info and general app state
struct AppState {
  // eye pos, look dir, up dir
  std::array<ospcommon::vec3f, 3> v;
  ospcommon::vec2i fbSize;
  bool cameraChanged, quit, fbSizeChanged, tfcnChanged;

  AppState();
};

// Struct for holding the other app data buffers and info that
// we can't bcast directly.
struct AppData {
  std::vector<ospcommon::vec3f> tfcn_colors;
  std::vector<float> tfcn_alphas;
};

// Some of these utils for computing the gridding and ghost region
// are from the gensv library in the OSPRay's mpi module
enum GhostFace {
  NEITHER_FACE = 0,
  POS_FACE = 1,
  NEG_FACE = 1 << 1,
};

bool computeDivisor(int x, int &divisor);

/* Compute an X x Y x Z grid to have num bricks,
 * only gives a nice grid for numbers with even factors since
 * we don't search for factors of the number, we just try dividing by two
 */
ospcommon::vec3i computeGrid(int num);

/* Compute which faces of this brick we need to specify ghost voxels for,
 * to have correct interpolation at brick boundaries. Returns mask of
 * GhostFaces for x, y, z.
 */
std::array<int, 3> computeGhostFaces(const ospcommon::vec3i &brickId,
    const ospcommon::vec3i &grid);

#define PIDX_CHECK(F) \
  { \
    PIDX_return_code rc = F; \
    if (rc != PIDX_success) { \
      const std::string er = "PIDX Error at " #F ": " + pidx_error_to_string(rc); \
      std::cerr << er << std::endl; \
      throw std::runtime_error(er); \
    } \
  }

struct UintahTimestep {
  size_t timestep;
  std::string path;

  UintahTimestep(const size_t timestep, const std::string &path);
};
bool operator<(const UintahTimestep &a, const UintahTimestep &b);

std::set<UintahTimestep> collectUintahTimesteps(const std::string &dir);

std::string pidx_error_to_string(const PIDX_return_code rc);

