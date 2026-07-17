#include "core/count_expression.h"

#include <cmath>
#include <cctype>
#include <limits>

namespace say_count {
namespace {

void SkipSpaces(std::string_view expression, std::size_t& position) {
    while (position < expression.size() &&
           std::isspace(static_cast<unsigned char>(expression[position]))) {
        ++position;
    }
}

std::optional<long double> ParseNumber(std::string_view expression, std::size_t& position) {
    SkipSpaces(expression, position);
    long double value = 0;
    bool has_digit = false;
    while (position < expression.size() &&
           std::isdigit(static_cast<unsigned char>(expression[position]))) {
        has_digit = true;
        value = value * 10 + (expression[position] - '0');
        if (!std::isfinite(value)) return std::nullopt;
        ++position;
    }
    if (position < expression.size() && expression[position] == '.') {
        ++position;
        long double place = 0.1L;
        while (position < expression.size() &&
               std::isdigit(static_cast<unsigned char>(expression[position]))) {
            has_digit = true;
            value += (expression[position] - '0') * place;
            place *= 0.1L;
            ++position;
        }
    }
    return has_digit ? std::optional<long double>{value} : std::nullopt;
}

}  // namespace

std::optional<long> evaluate_count_expression(std::string_view expression) {
    std::size_t position = 0;
    auto result = ParseNumber(expression, position);
    if (!result) return std::nullopt;

    while (true) {
        SkipSpaces(expression, position);
        if (position == expression.size()) break;
        const char operation = expression[position++];
        if (operation != '*' && operation != '/' && operation != 'x' && operation != 'X') {
            return std::nullopt;
        }
        const auto operand = ParseNumber(expression, position);
        if (!operand || ((operation == '/') && *operand == 0)) return std::nullopt;
        if (operation == '/') *result /= *operand;
        else *result *= *operand;
        if (!std::isfinite(*result)) return std::nullopt;
    }

    const long double rounded = std::round(*result);
    if (rounded < 0 || rounded > static_cast<long double>(std::numeric_limits<long>::max())) {
        return std::nullopt;
    }
    return static_cast<long>(rounded);
}

}  // namespace say_count
