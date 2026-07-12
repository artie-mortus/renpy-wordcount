#pragma once

#include <string_view>
#include <string>
#include <map>
#include <vector>

#include "core/parser.h"

namespace say_count {

std::string_view core_version() noexcept;
std::string statistics_csv(const ScriptAnalysis& analysis,
                           const std::map<std::string, std::pair<long, long>>& targets = {});
std::string statistics_json(const ScriptAnalysis& analysis,
                            const std::map<std::string, std::pair<long, long>>& speaker_targets = {},
                            const std::map<std::string, std::pair<long, long>>& scene_targets = {},
                            long word_target = 0, long line_target = 0);
std::string statistics_html(const ScriptAnalysis& analysis, std::string_view title,
                            std::string_view generated_at);

struct SpeakerChange { std::string name; std::size_t before = 0; std::size_t after = 0; long delta = 0; };
struct VersionComparison {
    std::size_t before_words = 0, after_words = 0, added_words = 0, removed_words = 0;
    long net_words = 0;
    std::vector<SpeakerChange> speakers;
};
VersionComparison compare_versions(const ScriptAnalysis& before, const ScriptAnalysis& after);

}  // namespace say_count
