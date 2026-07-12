#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

struct FindOptions {
    bool case_sensitive = false;
    bool use_regex = false;
    bool whole_word = false;
};

struct FindMatch {
    std::size_t position = 0;
    std::size_t length = 0;
};

struct FindResult {
    bool valid = true;
    std::vector<FindMatch> matches;
};

struct ReplaceResult {
    bool valid = true;
    std::string text;
    std::size_t count = 0;
};

FindResult find_matches(std::string_view text, std::string_view query,
                        FindOptions options = {});
ReplaceResult replace_all_matches(std::string_view text, std::string_view query,
                                  std::string_view replacement, FindOptions options = {});

}  // namespace say_count
