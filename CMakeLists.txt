cmake_minimum_required(VERSION 3.1)
project(module_pidx)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(PIDX REQUIRED)
find_package(MPI REQUIRED)
include_directories(${GLFW_INCLUDE_DIR}
  ${MPI_CXX_INCLUDE_PATH}
  ${PIDX_INCLUDE_DIRS})

option(OSPRAY_MODULE_PIDX "PIDX module" ON)

if (OSPRAY_MODULE_PIDX)
  include_directories(${CMAKE_SOURCE_DIR}/apps/common
    ${CMAKE_SOURCE_DIR}/apps/exampleViewer)

  ospray_create_application(pidx_viewer
    pidx_viewer.cpp
    arcball.cpp
    util.cpp
  LINK
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARY}
    gl3w
    imgui
    ospray
    ospray_common
    ospray_mpi_common
    ospray_imgui3d_sg
    ospray_sg
    ospray
    ${PIDX_LIBRARIES}
    ${MPI_CXX_LIBRARIES}
  )
endif()

