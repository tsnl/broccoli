cmake_minimum_required(VERSION 3.21)
project(broccoli)

include(FetchContent)
include("cmake/Strict.cmake")

#
# Tooling:
#

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_BUILD_TYPE MATCHES "[Dd]ebug")
  set(SANITIZE ON)
else()
  set(SANITIZE OFF)
endif()
set(BROCCOLI_SANITIZE "$SANITIZE" CACHE BOOL "Enable sanitizers")

#
# Dependencies:
#

set(BROCCOLI_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(FETCHCONTENT_QUIET OFF)
include("cmake/FetchDawn.cmake")
include("cmake/FetchGlm.cmake")
include("cmake/FetchFmt.cmake")
include("cmake/FetchRobinHoodHashing.cmake")
include("cmake/FetchCxxopts.cmake")
include("cmake/FetchStbImage.cmake")

#
# Build targets:
#

add_strict_library(
  broccoli-engine STATIC
  "src/broccoli/engine/core.cc"
  "src/broccoli/engine/engine.cc"
  "src/broccoli/engine/render.cc"
  "src/broccoli/engine/bitmap.cc"
)
target_include_directories(broccoli-engine PUBLIC "inc")
target_link_libraries(broccoli-engine PUBLIC glm fmt robin_hood stb-image)
target_link_libraries(broccoli-engine PUBLIC dawn_proc dawn_glfw dawn_native dawncpp)

add_strict_library(
  broccoli-sample STATIC
  "src/broccoli/sample/sample1.cc"
  "src/broccoli/sample/sample2.cc"
  "src/broccoli/sample/sample3.cc"
)
target_link_libraries(broccoli-sample PUBLIC broccoli-engine)

add_strict_executable(
  broccoli
  "src/broccoli/main.cc"
)
target_link_libraries(broccoli PRIVATE broccoli-sample)
target_link_libraries(broccoli PRIVATE cxxopts)

#
# Copying assets to the output folder:
#

add_custom_command(
  TARGET broccoli POST_BUILD
  COMMAND 
    ${CMAKE_COMMAND} -E copy
      ${CMAKE_SOURCE_DIR}/res
      ${CMAKE_CURRENT_BINARY_DIR}/res
)
