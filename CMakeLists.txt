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

#
# Build targets:
#

add_strict_executable(broccoli "src/broccoli/main.cc" "src/broccoli/core.cc" "src/broccoli/kernel.cc")
target_include_directories(broccoli PRIVATE "inc")
target_link_libraries(broccoli PRIVATE glm)
target_link_libraries(broccoli PRIVATE glfw)
target_link_libraries(broccoli PRIVATE dawn_proc dawn_common dawn_glfw dawn_native dawn_utils dawncpp)
target_link_libraries(broccoli PRIVATE fmt::fmt)