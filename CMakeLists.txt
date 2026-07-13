cmake_minimum_required(VERSION 3.20)

project(SayCountNative VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base stc aui adv html xml media)
include(${wxWidgets_USE_FILE})

add_library(say_count_core STATIC
    src/core/parser.cpp
    src/core/outline.cpp
    src/core/autocomplete.cpp
    src/core/find_replace.cpp
    src/core/diagnostics.cpp
    src/core/editor_commands.cpp
    src/core/project.cpp
    src/core/project_bundle.cpp
    src/core/rename.cpp
    src/core/renpy.cpp
    src/core/renpy_runtime.cpp
    src/core/renpy_lint.cpp
    src/core/assets.cpp
    src/core/coverage.cpp
    src/core/routes.cpp
    src/core/flow_layout.cpp
    src/core/prose.cpp
    src/core/voice.cpp
    src/core/translation.cpp
    src/core/snapshots.cpp
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
    src/ui/snapshot_dialog.cpp
    src/ui/main_frame.cpp
    src/ui/rename_dialog.cpp
    src/ui/runtime_preset_dialog.cpp
    src/ui/renpy_lint_panel.cpp
    src/ui/asset_panel.cpp
    src/ui/coverage_panel.cpp
    src/ui/route_panel.cpp
    src/ui/flow_map_panel.cpp
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
        tests/project_bundle_test.cpp
        tests/rename_test.cpp
        tests/renpy_test.cpp
        tests/renpy_runtime_test.cpp
        tests/renpy_lint_test.cpp
        tests/assets_test.cpp
        tests/coverage_test.cpp
        tests/routes_test.cpp
        tests/flow_layout_test.cpp
        tests/prose_test.cpp
        tests/voice_test.cpp
        tests/translation_test.cpp
        tests/snapshots_test.cpp
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
