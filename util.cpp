#include <cmath>
#include "ospcommon/vec.h"
#include "util.h"

using namespace ospcommon;

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
