cmake_minimum_required(VERSION 3.1)
project(module_pidx)

option(OSPRAY_MODULE_PIDX        "PIDX module"               OFF)
option(OSPRAY_MODULE_PIDX_VIEWER "PIDX remote viewer client" OFF)

if (OSPRAY_MODULE_PIDX)

  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} 
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
  find_package(PIDX REQUIRED)
  find_package(MPI REQUIRED)
  find_package(TurboJpeg REQUIRED)

  include_directories(
    ${MPI_CXX_INCLUDE_PATH}
    ${PIDX_INCLUDE_DIRS})

  include_directories(
    ${CMAKE_SOURCE_DIR}/apps/common
    ${CMAKE_SOURCE_DIR}/apps/exampleViewer
    ${CMAKE_SOURCE_DIR}/apps/exampleViewer/common/imgui)

  add_library(pidx_app_util
    pidx_volume.cpp
    util.cpp
    image_util.cpp
    client_server.cpp
    socket_fabric.cpp
    )
  target_link_libraries(pidx_app_util PUBLIC
    ospray
    ospray_common
    ospray_mpi_common
    ospray
    ${PIDX_LIBRARIES}
    ${MPI_CXX_LIBRARIES}
    TurboJpeg
    )

  ospray_create_application(pidx_movie_renderer
    pidx_movie_renderer.cpp
    LINK
    pidx_app_util
    ospray
    ospray_common
    ospray_mpi_common
    ospray
    ${PIDX_LIBRARIES}
    ${MPI_CXX_LIBRARIES}
    TurboJpeg
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
    TurboJpeg
    )

  if (OSPRAY_MODULE_PIDX_VIEWER)

    find_package(GLFW REQUIRED)
    include_directories(${GLFW_INCLUDE_DIRS})
    mark_as_advanced(GLFW_ROOT)

    # optional transfer function module
    if (TFN_MODULE_ROOT)
      include_directories(${TFN_MODULE_ROOT}/src)      
      set(TFN_MODULE_SRC 
        ${TFN_MODULE_ROOT}/src/TransferFunctionModule.cpp
        ${TFN_MODULE_ROOT}/src/widgets/TransferFunctionWidget.cpp)
      add_definitions(-DUSE_TFN_MODULE)
      mark_as_advanced(TFN_MODULE_SRC)
    endif(TFN_MODULE_ROOT)

    ospray_create_application(pidx_viewer
      pidx_viewer.cpp
      arcball.cpp
      ${TFN_MODULE_SRC}
      LINK
      pidx_app_util
      ${OPENGL_LIBRARIES}
      ${GLFW_LIBRARY}
      ospray_imgui
      ospray
      ospray_common
      ospray_imgui3d_sg
      ospray_sg
      TurboJpeg
      )

  endif()  

endif()

