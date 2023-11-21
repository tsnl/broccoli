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

file(COPY "res" DESTINATION "${CMAKE_BINARY_DIR}")

#
# Build targets:
#

add_strict_executable(
  broccoli
  "src/broccoli/main.cc"
  "src/broccoli/core.cc"
  "src/broccoli/engine.cc"
  "src/broccoli/draw.cc"
)
target_include_directories(broccoli PRIVATE "inc")
target_link_libraries(broccoli PRIVATE glm fmt robin_hood)
target_link_libraries(broccoli PRIVATE dawn_proc dawn_glfw dawn_native dawncpp)
