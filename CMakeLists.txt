cmake_minimum_required(VERSION 3.20)

project(SayCountNative VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(PROSE_GUIDE_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/resources/guides/prose.html)
set(CHAT_GUIDE_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/resources/guides/chat.html)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${PROSE_GUIDE_SOURCE} ${CHAT_GUIDE_SOURCE})
file(READ ${PROSE_GUIDE_SOURCE} PROSE_GUIDE_HTML)
file(READ ${CHAT_GUIDE_SOURCE} CHAT_GUIDE_HTML)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)
configure_file(cmake/guide_content.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/guide_content.h @ONLY)

option(SAY_COUNT_BUNDLED_WX "Build and statically link wxWidgets from source" OFF)
if(SAY_COUNT_BUNDLED_WX)
    include(FetchContent)
    set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(wxBUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(wxBUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(wxUSE_MEDIACTRL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        wxWidgets
        GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
        GIT_TAG v3.2.11
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(wxWidgets)
    if(NOT wxwidgets_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Populate(wxWidgets)
        add_subdirectory(${wxwidgets_SOURCE_DIR} ${wxwidgets_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
    set(SAY_COUNT_WX_LIBRARIES
        wx::core wx::base wx::stc wx::aui wx::adv wx::html wx::xml wx::media)
else()
    find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base stc aui adv html xml media)
    include(${wxWidgets_USE_FILE})
    set(SAY_COUNT_WX_LIBRARIES ${wxWidgets_LIBRARIES})
endif()

add_library(say_count_core STATIC
    src/core/count_expression.cpp
    src/core/parser.cpp
    src/core/outline.cpp
    src/core/autocomplete.cpp
    src/core/find_replace.cpp
    src/core/diagnostics.cpp
    src/core/editor_commands.cpp
    src/core/project.cpp
    src/core/project_creation.cpp
    src/core/script_builder.cpp
    src/core/writer_draft.cpp
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
    src/core/manuscript.cpp
    src/core/chat_program_parser.cpp
    src/core/chat_program_formatter.cpp
    src/core/chat_dialogue_adapter.cpp
    src/core/chat_runtime.cpp
    src/core/offline_prose_ai.cpp
    src/core/voice.cpp
    src/core/translation.cpp
    src/core/continuity.cpp
    src/core/accessibility.cpp
    src/core/indent.cpp
    src/core/snapshots.cpp
    src/core/tokenizer.cpp
    src/core/version.cpp
    src/core/workspace.cpp
    src/core/navigator.cpp
    src/core/git_repository.cpp
    src/core/vim.cpp
)
target_include_directories(say_count_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(say_count_core PUBLIC SAY_COUNT_VERSION="${PROJECT_VERSION}")

add_executable(say-count
    src/app/main.cpp
    src/app/settings.cpp
    src/app/git_client.cpp
    src/app/offline_prose_runner.cpp
    src/ui/editor_notebook.cpp
    src/ui/command_model.cpp
    src/ui/style.cpp
    src/ui/guide_dialog.cpp
    src/ui/welcome_dialog.cpp
    src/ui/story_element_dialog.cpp
    src/ui/story_library_panel.cpp
    src/ui/writer_draft_dialog.cpp
    src/ui/palette_dialog.cpp
    src/ui/diagnostics_panel.cpp
    src/ui/conflict_dialog.cpp
    src/ui/snapshot_dialog.cpp
    src/ui/main_frame.cpp
    src/ui/rename_dialog.cpp
    src/ui/manuscript_dialog.cpp
    src/ui/chat_conversion_dialog.cpp
    src/ui/runtime_preset_dialog.cpp
    src/ui/renpy_lint_panel.cpp
    src/ui/coverage_panel.cpp
    src/ui/route_panel.cpp
    src/ui/flow_map_panel.cpp
    src/ui/production_panel.cpp
    src/ui/outline_panel.cpp
    src/ui/speaker_stats_panel.cpp
    src/ui/git_dialog.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(say-count PRIVATE say_count_core ${SAY_COUNT_WX_LIBRARIES} Threads::Threads)
target_include_directories(say-count PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_definitions(say-count PRIVATE
    SAY_COUNT_ICON_PATH="${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/512x512/apps/say-count.png"
    SAY_COUNT_SOURCE_ICON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/say-count.png"
    SAY_COUNT_CHAT_INSTALLED_PATH="${CMAKE_INSTALL_FULL_DATADIR}/say-count/chat_program"
    SAY_COUNT_CHAT_RESOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/chat_program")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(say_count_core PRIVATE $<$<CONFIG:Release>:-O2>)
    target_compile_options(say-count PRIVATE $<$<CONFIG:Release>:-O2>)
    target_link_options(say-count PRIVATE $<$<CONFIG:Release>:-s>)
endif()

install(TARGETS say-count RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
configure_file(resources/say-count.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/say-count.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/say-count.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
foreach(icon_size IN ITEMS 48 128 256 512)
    install(FILES resources/icons/${icon_size}x${icon_size}/say-count.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${icon_size}x${icon_size}/apps)
endforeach()
install(FILES resources/text-x-renpy.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)
install(DIRECTORY resources/chat_program
        DESTINATION ${CMAKE_INSTALL_DATADIR}/say-count)

configure_file(cmake/uninstall.cmake.in uninstall.cmake @ONLY)
if(NOT TARGET uninstall)
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake)
endif()

include(CTest)
if(BUILD_TESTING)
    option(SAY_COUNT_GRAPHICAL_TESTS "Run graphical Ren'Py runtime smoke tests" OFF)
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
        src/app/git_client.cpp
        tests/count_expression_test.cpp
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
        tests/project_creation_test.cpp
        tests/script_builder_test.cpp
        tests/writer_draft_test.cpp
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
        tests/manuscript_test.cpp
        tests/chat_document_test.cpp
        tests/chat_runtime_test.cpp
        tests/voice_test.cpp
        tests/translation_test.cpp
        tests/continuity_test.cpp
        tests/accessibility_test.cpp
        tests/indent_test.cpp
        tests/snapshots_test.cpp
        tests/export_test.cpp
        tests/version_comparison_test.cpp
        tests/workspace_test.cpp
        tests/navigator_test.cpp
        tests/git_repository_test.cpp
        tests/git_client_test.cpp
        tests/vim_test.cpp
    )
    target_link_libraries(say-count-tests PRIVATE say_count_core Catch2::Catch2WithMain)

    add_executable(say-count-chat-ui-tests
        tests/chat_ui_test.cpp
        src/app/settings.cpp
        src/ui/command_model.cpp
        src/ui/chat_conversion_dialog.cpp
        src/ui/guide_dialog.cpp
        src/ui/welcome_dialog.cpp
        src/ui/story_element_dialog.cpp
        src/ui/story_library_panel.cpp
        src/ui/writer_draft_dialog.cpp
        src/ui/editor_notebook.cpp
        src/ui/style.cpp)
    target_link_libraries(say-count-chat-ui-tests PRIVATE
        say_count_core Catch2::Catch2 ${SAY_COUNT_WX_LIBRARIES} Threads::Threads)
    target_include_directories(say-count-chat-ui-tests PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated)
    add_test(NAME chat-conversion-ui COMMAND say-count-chat-ui-tests)
    set_tests_properties(chat-conversion-ui PROPERTIES TIMEOUT 20)

    add_executable(say-count-performance tests/performance_benchmark.cpp)
    target_link_libraries(say-count-performance PRIVATE say_count_core)
    add_custom_target(performance-check
        COMMAND say-count-performance --check
        DEPENDS say-count-performance
        COMMENT "Running performance regression thresholds")

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

    find_program(RENPY_TEST_EXECUTABLE NAMES renpy renpy.sh)
    if(RENPY_TEST_EXECUTABLE)
        add_test(
            NAME chat-program-renpy-lint-compile
            COMMAND ${CMAKE_COMMAND}
                -DRENPY=${RENPY_TEST_EXECUTABLE}
                -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/chat-program-renpy-project
                -DRUN_GRAPHICAL_RUNTIME=${SAY_COUNT_GRAPHICAL_TESTS}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/chat_renpy.cmake)
        set_tests_properties(chat-program-renpy-lint-compile PROPERTIES TIMEOUT 90)
    endif()
endif()
