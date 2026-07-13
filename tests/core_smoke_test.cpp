#include "core/version.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("core exposes its version") {
    CHECK(say_count::core_version() == "1.0.0");
}
