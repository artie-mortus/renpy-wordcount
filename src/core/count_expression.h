#pragma once

#include <optional>
#include <string_view>

namespace say_count {

// Evaluates a non-negative count made from decimal numbers, multiplication,
// and division. Fractional results are rounded to the nearest whole count.
std::optional<long> evaluate_count_expression(std::string_view expression);

}  // namespace say_count
