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
# Links ${TARGET_LIB_ICCPROFLIB} on every platform, including Windows shared
# builds.  Until #2219 this tool linked IccProfLib2-static there: it references
# the exported data symbols icMsgValidateWarning / icMsgValidateNonCompliant,
# which WINDOWS_EXPORT_ALL_SYMBOLS cannot export.  Those eight globals now carry
# ICCPROFLIB_DATA_API dllexport/dllimport, so the fallback is gone (#2154).
set(WX_LINK_LIB ${TARGET_LIB_ICCPROFLIB})

# Log configuration
message(STATUS "SRC_PATH: ${SRC_PATH}")
message(STATUS "Sources: ${SOURCES}")

set(wxWidgets_CONFIGURATION mswu)

add_executable(${TARGET_NAME} ${SOURCES})
set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE ON)
if(APPLE)
  # Keep the tool path stable, but give AppKit a main bundle identifier.
  set_target_properties(${TARGET_NAME} PROPERTIES
    XCODE_ATTRIBUTE_CREATE_INFOPLIST_SECTION_IN_BINARY YES
    XCODE_ATTRIBUTE_INFOPLIST_FILE
      "${SRC_PATH}/Tools/wxWidget/wxProfileDump/Info.plist"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
      "org.color.iccdev.iccDumpProfileGui")
endif()
message(STATUS "  Linking libraries for target ${TARGET_NAME}:")
message(STATUS "  ICCProfLib Library: ${WX_LINK_LIB}")
message(STATUS "  wxWidgets Libraries: ${wxWidgets_LIBRARIES}")
target_link_libraries(${TARGET_NAME} PRIVATE ${WX_LINK_LIB} ${wxWidgets_LIBRARIES})

if(ENABLE_INSTALL_RIM)
    install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

message(STATUS "wxProfileDump Configuration Complete")
