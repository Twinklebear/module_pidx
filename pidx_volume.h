#pragma once

#include <string>
#include "ospray/ospray_cpp/Volume.h"
#include "util.h"
#include "PIDX.h"

struct PIDXVolume {
  std::string datasetPath;
  PIDX_access pidxAccess;
  PIDX_file pidxFile;
  PIDX_point pdims;
  int resolution;
  ospray::cpp::Volume volume;
  vec3sz fullDims, localDims, localOffset;
  ospcommon::box3f localRegion;
  ospcommon::vec2f valueRange;

  PIDXVolume(const std::string &path);
  ~PIDXVolume();
  void update();
};

