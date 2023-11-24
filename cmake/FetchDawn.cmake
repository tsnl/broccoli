# Requires: 'BROCCOLI_ROOT'

# Running 'gclient' to setup Dawn.
# FETCH_DEPENDENCIES does not work as of writing this file.
if (NOT EXISTS "${BROCCOLI_ROOT}/dep/dawn/.gclient")
  file(COPY_FILE "${BROCCOLI_ROOT}/dep/dawn/scripts/standalone.gclient" "${BROCCOLI_ROOT}/dep/dawn/.gclient")
endif()
if (WIN32)
  set(ENV{PATH} "$ENV{PATH};${BROCCOLI_ROOT}/dep/depot_tools")
else()
  set(ENV{PATH} "$ENV{PATH}:${BROCCOLI_ROOT}/dep/depot_tools")
endif()
execute_process(
  COMMAND gclient sync
  WORKING_DIRECTORY "${BROCCOLI_ROOT}/dep/dawn"
)

# A more minimalistic choice of backends than Dawn's default: either use Vulkan or Metal.
if (APPLE)
  set(USE_VULKAN OFF)
  set(USE_METAL ON)
else()
  set(USE_VULKAN ON)
  set(USE_METAL OFF)
endif()
set(DAWN_ENABLE_D3D11 OFF)
set(DAWN_ENABLE_D3D12 OFF)
set(DAWN_ENABLE_METAL ${USE_METAL})
set(DAWN_ENABLE_NULL OFF)
set(DAWN_ENABLE_DESKTOP_GL OFF)
set(DAWN_ENABLE_OPENGLES OFF)
set(DAWN_ENABLE_VULKAN ${USE_VULKAN})
set(TINT_BUILD_SPV_READER OFF)

# Disable unneeded parts
set(DAWN_BUILD_SAMPLES OFF)
set(TINT_BUILD_SAMPLES OFF)
set(TINT_BUILD_DOCS OFF)
set(TINT_BUILD_TESTS OFF)
set(TINT_BUILD_FUZZERS OFF)
set(TINT_BUILD_SPIRV_TOOLS_FUZZER OFF)
set(TINT_BUILD_AST_FUZZER OFF)
set(TINT_BUILD_REGEX_FUZZER OFF)
set(TINT_BUILD_BENCHMARKS OFF)
set(TINT_BUILD_TESTS OFF)
set(TINT_BUILD_AS_OTHER_OS OFF)
set(TINT_BUILD_REMOTE_COMPILE OFF)

add_subdirectory("dep/dawn")
