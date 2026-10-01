if(NOT DEFINED ICCDEV_TOOL OR NOT DEFINED ICCDEV_EXPECT_TELEMETRY)
  message(FATAL_ERROR "Tool path and expected telemetry state are required")
endif()

execute_process(
  COMMAND "${ICCDEV_TOOL}" --help
  RESULT_VARIABLE help_result
  OUTPUT_VARIABLE help_output
  ERROR_VARIABLE help_error
)
if(NOT help_result EQUAL 0)
  message(FATAL_ERROR "--help failed: ${help_result}: ${help_error}")
endif()

if(ICCDEV_EXPECT_TELEMETRY)
  if(NOT help_output MATCHES "--telemetry=" OR
     NOT help_output MATCHES "--evidence-file")
    message(FATAL_ERROR "Telemetry-enabled tool does not advertise both options")
  endif()
else()
  if(help_output MATCHES "--telemetry=" OR
     help_output MATCHES "--evidence-file")
    message(FATAL_ERROR "Telemetry-disabled tool advertises telemetry or evidence")
  endif()
  foreach(option IN ITEMS --telemetry=human --evidence-file)
    execute_process(
      COMMAND "${ICCDEV_TOOL}" "${option}"
      RESULT_VARIABLE reject_result
      OUTPUT_VARIABLE reject_output
      ERROR_VARIABLE reject_error
    )
    if(reject_result EQUAL 0 OR
       NOT reject_error MATCHES "Telemetry is not built")
      message(FATAL_ERROR
        "Telemetry-disabled tool did not reject ${option}: ${reject_result}: ${reject_output}${reject_error}")
    endif()
  endforeach()
endif()
