#################################################################################
# Cmake Project Configuration| iccMAX Project
# Copyright (C) 2024-2025 The International Color Consortium. 
#                                        All rights reserved.
#  Last Updated: Mon Apr 28 14:50:05 EDT 2025 by David Hoyt
#
#
#
#  
#  Changes: Modified Build/Cmake/CMakeLists.txt and others
#           Improved dependency linking and error handling
#           Adjusted cross-platform build args
#
#  TODO: Refactor Debug, Release, Asan 
#        Collapse 3-tier CMakeLists.txt, Housekeeping
#        Refactor Base Configs & logging to .mk's
#
#
#
#################################################################################

message(STATUS "Configuring wxProfileDump")

# Resolve source root properly
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(SRC_PATH "${PROJECT_ROOT}")
set(SOURCES ${SRC_PATH}/Tools/wxWidget/wxProfileDump/wxProfileDump.cpp)
set(TARGET_NAME iccDumpProfileGui)
set(WX_LINK_LIB ${TARGET_LIB_ICCPROFLIB})

IF(WIN32 AND ENABLE_SHARED_LIBS AND TARGET IccProfLib2-static)
  # wxProfileDump references exported data symbols (icMsgValidateWarning,
  # icMsgValidateNonCompliant) that WINDOWS_EXPORT_ALL_SYMBOLS cannot export.
  SET(WX_LINK_LIB IccProfLib2-static)
ENDIF()

# Log configuration
message(STATUS "SRC_PATH: ${SRC_PATH}")
message(STATUS "Sources: ${SOURCES}")

set(wxWidgets_CONFIGURATION mswu)

add_executable(${TARGET_NAME} ${SOURCES})
set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE ON)
message(STATUS "  Linking libraries for target ${TARGET_NAME}:")
message(STATUS "  ICCProfLib Library: ${WX_LINK_LIB}")
message(STATUS "  wxWidgets Libraries: ${wxWidgets_LIBRARIES}")
target_link_libraries(${TARGET_NAME} PRIVATE ${WX_LINK_LIB} ${wxWidgets_LIBRARIES})

if(ENABLE_INSTALL_RIM)
    install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

message(STATUS "wxProfileDump Configuration Complete")
