cmake_minimum_required(VERSION 3.21)
project(broccoli)

include(FetchContent)
include("cmake/Strict.cmake")

#
# Dependencies:
#

set(FETCHCONTENT_QUIET OFF)
include("cmake/FetchDawn.cmake")
include("cmake/FetchGlm.cmake")
include("cmake/FetchFmt.cmake")
include("cmake/FetchRobinHoodHashing.cmake")

#
# Copying assets to the output folder:
#

configure_file("res/render3d.wgsl" "${CMAKE_BINARY_DIR}/res/render3d.wgsl" COPYONLY)

#
# Build targets:
#

add_strict_library(
  broccoli-engine STATIC
  "src/broccoli/engine/core.cc"
  "src/broccoli/engine/engine.cc"
  "src/broccoli/engine/render.cc"
)
target_include_directories(broccoli-engine PUBLIC "inc")
target_link_libraries(broccoli-engine PUBLIC glm fmt robin_hood)
target_link_libraries(broccoli-engine PUBLIC dawn_proc dawn_glfw dawn_native dawncpp)

add_strict_library(
  broccoli-sample STATIC
  "src/broccoli/sample/sample1.cc"
  "src/broccoli/sample/sample2.cc"
)
target_link_libraries(broccoli-sample PUBLIC broccoli-engine)

add_strict_executable(
  broccoli
  "src/broccoli/main.cc"
)
target_link_libraries(broccoli PRIVATE broccoli-sample)
