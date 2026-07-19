if(NOT EXISTS "${RENPY}")
    message(FATAL_ERROR "Ren'Py executable not found: ${RENPY}")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/game/vendor/chat_program")
file(COPY "${SOURCE}/tests/fixtures/chat_program/runtime_project/game/"
     DESTINATION "${WORK}/game")
file(COPY
    "${SOURCE}/resources/chat_program/chat_program.rpy"
    "${SOURCE}/resources/chat_program/say_count_chat_bridge.rpy"
    "${SOURCE}/resources/chat_program/say_count_chat_kik.rpy"
    "${SOURCE}/resources/chat_program/LICENSE.txt"
    "${SOURCE}/resources/chat_program/NOTICE"
    DESTINATION "${WORK}/game/vendor/chat_program")
file(COPY "${SOURCE}/resources/chat_program/say_count_chat_config.rpy"
     DESTINATION "${WORK}/game")
file(COPY "${SOURCE}/resources/chat_program/assets/"
     DESTINATION "${WORK}/game")

get_filename_component(renpy_directory "${RENPY}" DIRECTORY)
if(EXISTS "${renpy_directory}/the_question/game/screens.rpy")
    file(COPY
        "${renpy_directory}/the_question/game/screens.rpy"
        "${renpy_directory}/the_question/game/gui.rpy"
        "${renpy_directory}/the_question/game/options.rpy"
        DESTINATION "${WORK}/game")
    file(COPY "${renpy_directory}/the_question/game/gui"
         DESTINATION "${WORK}/game")
endif()

execute_process(
    COMMAND "${RENPY}" "${WORK}" lint
    RESULT_VARIABLE lint_result
    OUTPUT_VARIABLE lint_output
    ERROR_VARIABLE lint_error)
if(NOT lint_result EQUAL 0)
    message(FATAL_ERROR "Ren'Py lint failed:\n${lint_output}\n${lint_error}")
endif()

execute_process(
    COMMAND "${RENPY}" "${WORK}" compile
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error
    TIMEOUT 60)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "Ren'Py compilation failed:\n${compile_output}\n${compile_error}")
endif()

if(DEFINED RUN_GRAPHICAL_RUNTIME AND RUN_GRAPHICAL_RUNTIME)
    execute_process(
        COMMAND "${RENPY}" "${WORK}" test chat_runtime_suite --hide-header
        RESULT_VARIABLE runtime_result
        OUTPUT_VARIABLE runtime_output
        ERROR_VARIABLE runtime_error
        TIMEOUT 30)
    if(NOT runtime_result EQUAL 0)
        message(FATAL_ERROR "Ren'Py runtime smoke failed:\n${runtime_output}\n${runtime_error}")
    endif()
endif()
