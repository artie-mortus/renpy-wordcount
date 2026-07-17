#include <catch2/catch_test_macros.hpp>

#include "core/count_expression.h"

TEST_CASE("count expressions multiply and divide") {
    CHECK(say_count::evaluate_count_expression("3000 / 2") == 1500);
    CHECK(say_count::evaluate_count_expression("750 * 2") == 1500);
    CHECK(say_count::evaluate_count_expression("100 x 3 / 2") == 150);
    CHECK(say_count::evaluate_count_expression("2.5 * 2") == 5);
}

TEST_CASE("count expressions round fractional results to whole counts") {
    CHECK(say_count::evaluate_count_expression("5 / 2") == 3);
    CHECK(say_count::evaluate_count_expression("4 / 3 * 3") == 4);
}

TEST_CASE("count expressions reject malformed or unsafe calculations") {
    CHECK_FALSE(say_count::evaluate_count_expression(""));
    CHECK_FALSE(say_count::evaluate_count_expression("3000 /"));
    CHECK_FALSE(say_count::evaluate_count_expression("3000 + 2"));
    CHECK_FALSE(say_count::evaluate_count_expression("3000 / 0"));
    CHECK_FALSE(say_count::evaluate_count_expression("words"));
    CHECK_FALSE(say_count::evaluate_count_expression("999999999999999999999999999999999999999999999999999"));
}
