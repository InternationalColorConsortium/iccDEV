cmake_minimum_required(VERSION 3.16)
project(icc-validation-dlopen LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT DEFINED ICCDEV_ROOT)
  get_filename_component(ICCDEV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
else()
  get_filename_component(ICCDEV_ROOT "${ICCDEV_ROOT}" ABSOLUTE)
endif()

add_executable(icc-validate-dlopen main.c)
target_include_directories(icc-validate-dlopen PRIVATE
  "${ICCDEV_ROOT}/IccProfLib"
)
if(UNIX AND NOT APPLE)
  target_link_libraries(icc-validate-dlopen PRIVATE dl)
endif()
