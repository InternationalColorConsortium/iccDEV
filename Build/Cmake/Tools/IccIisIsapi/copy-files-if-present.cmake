if(NOT DEFINED DEST_DIR OR DEST_DIR STREQUAL "")
  message(FATAL_ERROR "DEST_DIR is required")
endif()

if(NOT DEFINED FILES_TO_COPY OR FILES_TO_COPY STREQUAL "")
  return()
endif()

file(MAKE_DIRECTORY "${DEST_DIR}")

foreach(_copy_file IN LISTS FILES_TO_COPY)
  if(EXISTS "${_copy_file}")
    file(COPY "${_copy_file}" DESTINATION "${DEST_DIR}")
  endif()
endforeach()
