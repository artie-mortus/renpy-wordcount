cmake_minimum_required(VERSION 3.20)

project(SayCountNative VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base stc aui adv html xml)
include(${wxWidgets_USE_FILE})

add_library(say_count_core STATIC
    src/core/version.cpp
)
target_include_directories(say_count_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(say-count
    src/app/main.cpp
    src/ui/placeholder.cpp
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

    add_executable(say-count-tests tests/core_smoke_test.cpp)
    target_link_libraries(say-count-tests PRIVATE say_count_core Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(say-count-tests)
endif()

