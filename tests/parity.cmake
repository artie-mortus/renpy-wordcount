execute_process(
    COMMAND "${NODE}" "${JS_DUMP}" "${FIXTURE}"
    RESULT_VARIABLE js_status OUTPUT_VARIABLE js_output ERROR_VARIABLE js_error)
if(NOT js_status EQUAL 0)
    message(FATAL_ERROR "JavaScript parity dump failed: ${js_error}")
endif()
execute_process(
    COMMAND "${NATIVE_DUMP}" --dump-json "${FIXTURE}"
    RESULT_VARIABLE native_status OUTPUT_VARIABLE native_output ERROR_VARIABLE native_error)
if(NOT native_status EQUAL 0)
    message(FATAL_ERROR "Native parity dump failed: ${native_error}")
endif()
if(NOT js_output STREQUAL native_output)
    message(FATAL_ERROR "Parity mismatch\nJS:     ${js_output}\nNative: ${native_output}")
endif()
