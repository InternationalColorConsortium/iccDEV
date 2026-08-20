#
# IccBenchApply CMakeLists.txt | iccDEV Project
# Copyright (c) 2026 The International Color Consortium. All rights reserved.
#
# Throughput harness for the IccProfLib apply paths.
#
# Links ${TARGET_LIB_ICCPROFLIB} and nothing else, deliberately. The tool exists
# to measure IccProfLib, so pulling in IccConnect or IccJSON would put their code
# in the binary being measured -- and per the note in Build/Cmake/Testing/
# CMakeLists.txt, a target linking IccXML/IccJson plus the static IccProfLib gets
# two copies of the library's globals.
#

IF(NOT TARGET iccBenchApply)
  SET(SRC_PATH ../../../..)
  SET(SOURCES
    ${SRC_PATH}/Tools/CmdLine/IccBenchApply/iccBenchApply.cpp
    ${SRC_PATH}/Tools/CmdLine/IccBenchApply/BenchCases.cpp
  )
  SET(TARGET_NAME iccBenchApply)

  ADD_EXECUTABLE(${TARGET_NAME} ${SOURCES})
  TARGET_COMPILE_FEATURES(${TARGET_NAME} PRIVATE cxx_std_17)
  TARGET_LINK_LIBRARIES(${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB})

  # Linked now rather than when the thread sweep arrives, so the link line for
  # this target never has to change again.
  FIND_PACKAGE(Threads REQUIRED)
  TARGET_LINK_LIBRARIES(${TARGET_NAME} Threads::Threads)

  IF(ENABLE_INSTALL_RIM)
    INSTALL(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
  ENDIF()
ELSE()
  MESSAGE(WARNING "Target iccBenchApply already exists. Skipping duplicate addition.")
ENDIF()
