project(iccIisIsapi)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_ROOT "${CMAKE_SOURCE_DIR}/../..")
set(WINNT_ROOT "${SRC_ROOT}/Tools/Winnt/IccIisIsapi")
set(IIS_SAMPLE_SITE_FILES
  "${WINNT_ROOT}/index.html"
  "${WINNT_ROOT}/index2.html"
  "${WINNT_ROOT}/endpoints.html"
  "${WINNT_ROOT}/integration.html"
  "${WINNT_ROOT}/site.css"
  "${WINNT_ROOT}/site.js"
  "${WINNT_ROOT}/web.config"
)
set(IIS_SAMPLE_TOOL_WORK_INDEX "${WINNT_ROOT}/tool-work-index.html")
set(IIS_SAMPLE_ASSET_DIR "${WINNT_ROOT}/assets")
set(IIS_SAMPLE_COPY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/copy-files-if-present.cmake")

if(NOT WIN32)
  return()
endif()

if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/install-iis-runtime-deps.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/install-iis-runtime-deps.cmake"
    @ONLY
  )
endif()

add_library(iccIisIsapi SHARED
  "${WINNT_ROOT}/iccIisIsapi.cpp"
  "${WINNT_ROOT}/iccIisIsapi.def"
)
target_compile_features(iccIisIsapi PRIVATE cxx_std_17)
target_link_libraries(iccIisIsapi PRIVATE IccProfLib2 IccXML2)
set_target_properties(iccIisIsapi PROPERTIES
  OUTPUT_NAME "iccIisIsapi"
  DEBUG_POSTFIX ""
)

if(WIN32)
  add_custom_command(TARGET iccIisIsapi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:iccIisIsapi>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:iccIisIsapi>/_tool-work"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${IIS_SAMPLE_SITE_FILES}
      $<TARGET_FILE_DIR:iccIisIsapi>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${IIS_SAMPLE_TOOL_WORK_INDEX}"
      "$<TARGET_FILE_DIR:iccIisIsapi>/_tool-work/index.html"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${IIS_SAMPLE_ASSET_DIR}"
      "$<TARGET_FILE_DIR:iccIisIsapi>/assets"
    COMMAND ${CMAKE_COMMAND}
      "-DFILES_TO_COPY=$<JOIN:$<TARGET_RUNTIME_DLLS:iccIisIsapi>,;>"
      "-DDEST_DIR=$<TARGET_FILE_DIR:iccIisIsapi>"
      -P "${IIS_SAMPLE_COPY_SCRIPT}"
    COMMAND_EXPAND_LISTS)
endif()

if(ENABLE_INSTALL_RIM)
  install(TARGETS iccIisIsapi
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT shared-proof
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT shared-proof
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT shared-proof)
  install(FILES ${IIS_SAMPLE_SITE_FILES}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT shared-proof)
  install(FILES "${IIS_SAMPLE_TOOL_WORK_INDEX}"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/_tool-work"
    RENAME index.html
    COMPONENT shared-proof)
  install(DIRECTORY "${IIS_SAMPLE_ASSET_DIR}/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/assets"
    COMPONENT shared-proof)
  if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/install-iis-runtime-deps.cmake" COMPONENT shared-proof)
  endif()
endif()

add_executable(iccIisIsapiSmoke
  "${WINNT_ROOT}/iccIisIsapiSmoke.cpp"
)
target_compile_features(iccIisIsapiSmoke PRIVATE cxx_std_17)
add_dependencies(iccIisIsapiSmoke iccIisIsapi)
set_target_properties(iccIisIsapiSmoke PROPERTIES
  OUTPUT_NAME "iccIisIsapiSmoke"
  DEBUG_POSTFIX ""
)

if(WIN32)
  add_custom_command(TARGET iccIisIsapiSmoke POST_BUILD
    COMMAND ${CMAKE_COMMAND}
      "-DFILES_TO_COPY=$<TARGET_FILE:iccIisIsapi>;$<JOIN:$<TARGET_RUNTIME_DLLS:iccIisIsapi>,;>"
      "-DDEST_DIR=$<TARGET_FILE_DIR:iccIisIsapiSmoke>"
      -P "${IIS_SAMPLE_COPY_SCRIPT}"
    COMMAND_EXPAND_LISTS)
endif()

if(ENABLE_INSTALL_RIM)
  install(TARGETS iccIisIsapiSmoke DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT shared-proof)
endif()
