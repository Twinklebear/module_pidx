# Try to find TurboJpeg
# Once done this will export the TurboJpeg target which can be linked against
# and export the following variables:
#
# TurboJpeg_FOUND - System has TurboJpeg

# find libjpeg-turbo
find_path(TurboJpeg_INCLUDE_DIR
  NAMES turbojpeg.h
  PATHS
  $ENV{LIBJPEG_TURBO_DIR}/include
  $ENV{TURBOJPEG_DIR}/include
  ${TURBOJPEG_DIR}/include
  #linux
  /opt/libjpeg-turbo/include
  /usr/include
  #mac
  /usr/local/opt/jpeg-turbo/include
)

find_library(TurboJpeg_LIBRARY
  NAMES turbojpeg
  PATHS
  $ENV{LIBJPEG_TURBO_DIR}/lib
  $ENV{TURBOJPEG_DIR}/lib
  ${TURBOJPEG_DIR}/lib
  #linux
  /opt/libjpeg-turbo/lib
  /usr/lib
  /usr/lib64
  #mac
  /usr/local/opt/jpeg-turbo/lib
)

mark_as_advanced(TurboJpeg_INCLUDE_DIR TurboJpeg_LIBRARY)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TurboJpeg
  REQUIRED_VARS TurboJpeg_INCLUDE_DIR TurboJpeg_LIBRARY)

if (TurboJpeg_FOUND AND NOT TARGET TurboJpeg)
  add_library(TurboJpeg UNKNOWN IMPORTED)
  set_target_properties(TurboJpeg PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${TurboJpeg_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${TurboJpeg_INCLUDE_DIR}")
endif()

