cmake_minimum_required(VERSION 3.1)
project(module_pidx)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(PIDX REQUIRED)
find_package(MPI REQUIRED)

# find libjpeg-turbo
FIND_PATH(TURBOJPEG_INCLUDE_DIR
  NAMES turbojpeg.h
  PATHS
  $ENV{LIBJPEG_TURBO_DIR}/include
  $ENV{TURBOJPEG_DIR}/include
  #linux
  /opt/libjpeg-turbo/include
  #mac
  /usr/local/opt/jpeg-turbo/include
)

FIND_LIBRARY(TURBOJPEG_LIBRARY
  NAMES turbojpeg
  PATHS 
  $ENV{LIBJPEG_TURBO_DIR}/lib
  $ENV{TURBOJPEG_DIR}/lib
  #linux
  /opt/libjpeg-turbo/lib
  #mac
  /usr/local/opt/jpeg-turbo/lib
)

include_directories(${GLFW_INCLUDE_DIR}
  ${MPI_CXX_INCLUDE_PATH}
  ${PIDX_INCLUDE_DIRS}
  ${TURBOJPEG_INCLUDE_DIR})

option(OSPRAY_MODULE_PIDX "PIDX module" ON)

if (OSPRAY_MODULE_PIDX)
  include_directories(${CMAKE_SOURCE_DIR}/apps/common
    ${CMAKE_SOURCE_DIR}/apps/exampleViewer)

  add_library(pidx_app_util
    pidx_volume.cpp
    util.cpp
    image_util.cpp
  )
  target_link_libraries(pidx_app_util PUBLIC
    ospray
    ospray_common
    ospray_mpi_common
    ospray
    ${PIDX_LIBRARIES}
    ${MPI_CXX_LIBRARIES}
    ${TURBOJPEG_LIBRARY}
  )
 

  ospray_create_application(pidx_render_worker
    pidx_render_worker.cpp
  LINK
    pidx_app_util
    ospray
    ospray_common
    ospray_mpi_common
    ospray
    ${PIDX_LIBRARIES}
    ${MPI_CXX_LIBRARIES}
    ${TURBOJPEG_LIBRARY}
  )

  ospray_create_application(pidx_viewer
    pidx_viewer.cpp
    arcball.cpp
  LINK
    pidx_app_util
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARY}
    gl3w
    imgui
    ospray
    ospray_common
    ospray_imgui3d_sg
    ospray_sg
    ${TURBOJPEG_LIBRARY}
  )
endif()

