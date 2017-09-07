#pragma once

#include <array>
#include "ospcommon/vec.h"

using vec3sz = ospcommon::vec_t<size_t, 3>;

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

std::string pidx_error_to_string(const PIDX_return_code rc);
