#include <catch2/catch_test_macros.hpp>

#include "core/editor_commands.h"

TEST_CASE("comment toggle preserves indentation and blank lines") {
    const std::string source = "label start:\n    one\n\n    two";
    auto commented = say_count::toggle_line_comments(source, 13, source.size());
    REQUIRE(commented.text == "label start:\n    # one\n\n    # two");
    auto restored = say_count::toggle_line_comments(commented.text, 13, commented.text.size());
    REQUIRE(restored.text == source);
}

TEST_CASE("move and duplicate lines preserve exact text and selection") {
    const std::string source = "first\n    second\nthird";
    auto up = say_count::move_selected_lines(source, 10, 10, -1);
    REQUIRE(up.text == "    second\nfirst\nthird");
    REQUIRE(up.selection_start == 4);
    auto down = say_count::move_selected_lines(up.text, up.selection_start, up.selection_end, 1);
    REQUIRE(down.text == source);

    auto duplicate = say_count::duplicate_selected_lines(source, 10, 10, 1);
    REQUIRE(duplicate.text == "first\n    second\n    second\nthird");
    REQUIRE(duplicate.selection_start > 10);
}

TEST_CASE("pair input wraps selections closes and types over closers") {
    auto quote = say_count::apply_pair_input("hello", 0, 5, '"');
    REQUIRE(quote);
    REQUIRE(quote->text == "\"hello\"");
    REQUIRE(quote->selection_start == 1);
    REQUIRE(quote->selection_end == 6);

    auto paren = say_count::apply_pair_input("", 0, 0, '(');
    REQUIRE(paren->text == "()");
    REQUIRE(paren->selection_start == 1);
    auto type_over = say_count::apply_pair_input(paren->text, 1, 1, ')');
    REQUIRE(type_over->text == "()");
    REQUIRE(type_over->selection_start == 2);
    REQUIRE_FALSE(say_count::apply_pair_input("\\", 1, 1, '"'));
    REQUIRE_FALSE(say_count::apply_pair_input("\\\"", 1, 1, '"'));
}

TEST_CASE("label navigation finds adjacent declarations without wrapping") {
    const std::string source = "label start:\n    pass\nlabel middle:\n    pass\nlabel end:\n    pass";
    REQUIRE(say_count::adjacent_label_line(source, 2, 1) == 3);
    REQUIRE(say_count::adjacent_label_line(source, 4, -1) == 3);
    REQUIRE_FALSE(say_count::adjacent_label_line(source, 5, 1));
}
