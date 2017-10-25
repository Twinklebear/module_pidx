#pragma once

#include <vector>
#include <string>
#include "ospray/ospray_cpp/Volume.h"
#include "ospray/ospray_cpp/TransferFunction.h"
#include "util.h"
#include "PIDX.h"

struct PIDXVolume {
  std::string datasetPath;
  PIDX_access pidxAccess;
  PIDX_file pidxFile;
  PIDX_point pdims;
  std::vector<std::string> pidxVars;

  int resolution;
  ospray::cpp::Volume volume;
  ospray::cpp::TransferFunction transferFunction;
  vec3sz fullDims, localDims, localOffset;
  ospcommon::box3f localRegion;
  ospcommon::vec2f valueRange;

  // UI data
  std::string currentVariableName;
  int currentVariable;
  size_t currentTimestep;

  PIDXVolume(const std::string &path, ospray::cpp::TransferFunction tfcn,
      const std::string &currentVariableName, size_t currentTimestep);
  PIDXVolume(const PIDXVolume &p) = delete;
  PIDXVolume& operator=(const PIDXVolume &p) = delete;
  ~PIDXVolume();
  void update();
};

