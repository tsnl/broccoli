cmake_minimum_required(VERSION 3.21)
project(broccoli)

include(FetchContent)
include("cmake/Strict.cmake")

FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        15ead9a40d09a1eb9972215cceac2bf29c9b77f6   # release-2.28.5
)
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(
	FMT
	GIT_REPOSITORY https://github.com/fmtlib/fmt
	GIT_TAG				 f5e54359df4c26b6230fc61d38aa294581393084		# 10.1.1
)
FetchContent_MakeAvailable(FMT)

add_strict_executable(broccoli "src/broccoli/main.cc" "src/broccoli/core.cc" "src/broccoli/kernel.cc")
target_include_directories(broccoli PRIVATE "inc")
target_link_libraries(broccoli PUBLIC SDL2::SDL2main SDL2::SDL2)
target_link_libraries(broccoli PUBLIC fmt::fmt)
