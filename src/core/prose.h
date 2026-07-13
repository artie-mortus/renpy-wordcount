#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "core/parser.h"

namespace say_count {

struct ProseFrequency {
    std::string text;
    std::size_t count = 0;
};

struct SpeakerProseStats {
    std::string speaker;
    std::size_t sentences = 0;
    std::size_t words = 0;
    std::size_t longest_sentence = 0;

    double average_words() const noexcept {
        return sentences ? static_cast<double>(words) / sentences : 0.0;
    }
};

struct ProseAnalysis {
    std::size_t unique_words = 0;
    std::size_t total_words = 0;
    std::vector<ProseFrequency> overused_words;
    std::vector<ProseFrequency> repeated_phrases;
    std::vector<SpeakerProseStats> speakers;
};

ProseAnalysis analyze_prose(const ScriptAnalysis& analysis,
                            const std::set<std::string>& extra_exclusions = {});

}  // namespace say_count
