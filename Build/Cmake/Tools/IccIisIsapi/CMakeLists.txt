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
  "${WINNT_ROOT}/sanitize.js"
  "${WINNT_ROOT}/web.config"
)
set(IIS_SAMPLE_TOOL_WORK_INDEX "${WINNT_ROOT}/tool-work-index.html")
set(IIS_SAMPLE_ASSET_DIR "${WINNT_ROOT}/assets")
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
  "${WINNT_ROOT}/IccIsapiSanitize.cpp"
  "${WINNT_ROOT}/IccIsapiHttp.cpp"
)
target_compile_features(iccIisIsapi PRIVATE cxx_std_17)
target_include_directories(iccIisIsapi PRIVATE "${WINNT_ROOT}")
target_link_libraries(iccIisIsapi PRIVATE IccProfLib2 IccXML2)
if(ENABLE_ICCJSON)
  if(NOT TARGET IccJSON2)
    message(FATAL_ERROR
      "ENABLE_ICCJSON is ON but the IccJSON2 target is not defined; "
      "iccIisIsapi requires IccJSON2 for the JSON endpoints (issue #823)")
  endif()
  target_link_libraries(iccIisIsapi PRIVATE IccJSON2)
  target_include_directories(iccIisIsapi PRIVATE
    "${SRC_ROOT}/IccJSON/IccLibJSON")
  target_compile_definitions(iccIisIsapi PRIVATE USE_ICCJSON=1)
  if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # issue #823: anchor an exported IccJSON2 symbol so MSVC link
    # (/OPT:REF,ICF) and any antivirus / parallel-build race on the
    # windows-latest runner cannot silently elide the IccJSON2.dll
    # import from iccIisIsapi.dll. The CIccProfileJson destructor is
    # exported and stable (see dumpbin /exports IccJSON2.dll).
    target_link_options(iccIisIsapi PRIVATE
      "/INCLUDE:??1CIccProfileJson@@UEAA@XZ")
  endif()
endif()
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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:iccIisIsapi>
      $<TARGET_FILE_DIR:iccIisIsapi>
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
  install(FILES "${WINNT_ROOT}/Install-IccIisIsapiSite.ps1"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/iccdev
    COMPONENT shared-proof)
  install(FILES "${WINNT_ROOT}/Uninstall-IccIisIsapiSite.ps1"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/iccdev
    COMPONENT shared-proof)
  install(FILES
      "${WINNT_ROOT}/Export-IccIisIsapiSite.ps1"
      "${WINNT_ROOT}/Import-IccIisIsapiSite.ps1"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/iccdev
    COMPONENT shared-proof)
  install(FILES
      "${WINNT_ROOT}/Readme.md"
      "${WINNT_ROOT}/api.md"
      "${WINNT_ROOT}/isapi-instructions.md"
      "${WINNT_ROOT}/iis-isapi.openapi.yaml"
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/iccdev
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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:iccIisIsapi>
      $<TARGET_RUNTIME_DLLS:iccIisIsapi>
      $<TARGET_FILE_DIR:iccIisIsapiSmoke>
    COMMAND_EXPAND_LISTS)
endif()

if(ENABLE_INSTALL_RIM)
  install(TARGETS iccIisIsapiSmoke DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT shared-proof)
endif()
