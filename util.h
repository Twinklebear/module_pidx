#pragma once

#include <array>
#include "ospcommon/vec.h"

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

