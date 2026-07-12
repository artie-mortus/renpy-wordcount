cmake_minimum_required(VERSION 3.20)

project(SayCountNative VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base stc aui adv html xml)
include(${wxWidgets_USE_FILE})

add_library(say_count_core STATIC
    src/core/parser.cpp
    src/core/outline.cpp
    src/core/autocomplete.cpp
    src/core/find_replace.cpp
    src/core/diagnostics.cpp
    src/core/editor_commands.cpp
    src/core/project.cpp
    src/core/tokenizer.cpp
    src/core/version.cpp
)
target_include_directories(say_count_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(say-count
    src/app/main.cpp
    src/app/settings.cpp
    src/ui/editor_notebook.cpp
    src/ui/diagnostics_panel.cpp
    src/ui/conflict_dialog.cpp
    src/ui/main_frame.cpp
    src/ui/outline_panel.cpp
    src/ui/speaker_stats_panel.cpp
)
target_link_libraries(say-count PRIVATE say_count_core ${wxWidgets_LIBRARIES})

include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    # Offline fallback: pre-populate FETCHCONTENT_SOURCE_DIR_CATCH2 or the
    # FetchContent cache with Catch2 v3.8.1 before configuring.
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(say-count-tests
        tests/core_smoke_test.cpp
        tests/parser_test.cpp
        tests/analysis_test.cpp
        tests/tokenizer_test.cpp
        tests/outline_test.cpp
        tests/autocomplete_test.cpp
        tests/find_replace_test.cpp
        tests/diagnostics_test.cpp
        tests/editor_commands_test.cpp
        tests/project_test.cpp
        tests/export_test.cpp
        tests/version_comparison_test.cpp
    )
    target_link_libraries(say-count-tests PRIVATE say_count_core Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(say-count-tests)

    find_program(NODE_EXECUTABLE node REQUIRED)
    add_test(
        NAME parser-parity-simple
        COMMAND ${CMAKE_COMMAND}
            -DNODE=${NODE_EXECUTABLE}
            -DJS_DUMP=${CMAKE_CURRENT_SOURCE_DIR}/tools/parity-dump.mjs
            -DNATIVE_DUMP=$<TARGET_FILE:say-count>
            -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/simple-dialogue.rpy
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity.cmake
    )
    add_test(
        NAME parser-parity-characters
        COMMAND ${CMAKE_COMMAND}
            -DNODE=${NODE_EXECUTABLE}
            -DJS_DUMP=${CMAKE_CURRENT_SOURCE_DIR}/tools/parity-dump.mjs
            -DNATIVE_DUMP=$<TARGET_FILE:say-count>
            -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/characters/defs.rpy
            -DFIXTURE2=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/characters/scene.rpy
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity.cmake
    )
    add_test(
        NAME parser-parity-menu-ignored
        COMMAND ${CMAKE_COMMAND}
            -DNODE=${NODE_EXECUTABLE}
            -DJS_DUMP=${CMAKE_CURRENT_SOURCE_DIR}/tools/parity-dump.mjs
            -DNATIVE_DUMP=$<TARGET_FILE:say-count>
            -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/menu-choices.rpy
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity.cmake
    )
    add_test(
        NAME parser-parity-menu-counted
        COMMAND ${CMAKE_COMMAND}
            -DNODE=${NODE_EXECUTABLE}
            -DJS_DUMP=${CMAKE_CURRENT_SOURCE_DIR}/tools/parity-dump.mjs
            -DNATIVE_DUMP=$<TARGET_FILE:say-count>
            -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/menu-choices.rpy
            -DCOUNT_MENU_CHOICES=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity.cmake
    )
    foreach(parity_fixture IN ITEMS ignored-blocks monologues local-labels average-tenth)
        add_test(
            NAME parser-parity-${parity_fixture}
            COMMAND ${CMAKE_COMMAND}
                -DNODE=${NODE_EXECUTABLE}
                -DJS_DUMP=${CMAKE_CURRENT_SOURCE_DIR}/tools/parity-dump.mjs
                -DNATIVE_DUMP=$<TARGET_FILE:say-count>
                -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/${parity_fixture}.rpy
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity.cmake
        )
    endforeach()
    add_test(
        NAME parser-parity-analysis
        COMMAND ${CMAKE_COMMAND}
            -DNODE=${NODE_EXECUTABLE}
            -DJS_DUMP=${CMAKE_CURRENT_SOURCE_DIR}/tools/parity-dump.mjs
            -DNATIVE_DUMP=$<TARGET_FILE:say-count>
            -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/analysis-aggregates.rpy
            -DFIXTURE2=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/analysis-definitions.rpy
            -DCOUNT_MENU_CHOICES=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity.cmake
    )
    add_test(
        NAME parser-parity-warning-lint
        COMMAND ${CMAKE_COMMAND}
            -DNODE=${NODE_EXECUTABLE}
            -DJS_DUMP=${CMAKE_CURRENT_SOURCE_DIR}/tools/parity-dump.mjs
            -DNATIVE_DUMP=$<TARGET_FILE:say-count>
            -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/warning-lint.rpy
            -DFIXTURE2=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/warning-lint-helper.rpy
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity.cmake
    )
    add_test(
        NAME parser-parity-warning-parser
        COMMAND ${CMAKE_COMMAND}
            -DNODE=${NODE_EXECUTABLE}
            -DJS_DUMP=${CMAKE_CURRENT_SOURCE_DIR}/tools/parity-dump.mjs
            -DNATIVE_DUMP=$<TARGET_FILE:say-count>
            -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/warning-parser.rpy
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity.cmake
    )
    add_test(
        NAME parser-parity-warning-support-files
        COMMAND ${CMAKE_COMMAND}
            -DNODE=${NODE_EXECUTABLE}
            -DJS_DUMP=${CMAKE_CURRENT_SOURCE_DIR}/tools/parity-dump.mjs
            -DNATIVE_DUMP=$<TARGET_FILE:say-count>
            -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/warning-support/script.rpy
            -DFIXTURE2=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/warning-support/gui.rpy
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity.cmake
    )
endif()
