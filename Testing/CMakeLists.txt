#  Last Updated: 08-FEB-2026 at 0700Z by David Hoyt
#
#  Changes: Add WASM Toolchain, CFL & libFuzzer
#
#

# Testing subdirectory
cmake_minimum_required(VERSION 3.15)

# Only add fuzzing harnesses when enabled
if(ENABLE_FUZZING)
  add_subdirectory(Fuzzing)
endif()
