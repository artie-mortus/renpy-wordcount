#include "core/version.h"

#include <sstream>
#include <algorithm>
#include <cstdlib>

namespace say_count {

std::string_view core_version() noexcept {
#ifdef SAY_COUNT_VERSION
    return SAY_COUNT_VERSION;
#else
    return "development";
#endif
}

namespace {
std::string json_escape(std::string_view value) {
    std::string out;
    for (unsigned char c : value) {
        switch (c) { case '"': out += "\\\""; break; case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break;
        default: if (c < 0x20) { const char hex[] = "0123456789abcdef"; out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; } else out += static_cast<char>(c); }
    }
    return out;
}
std::string html_escape(std::string_view value) {
    std::string out;
    for (char c : value) { switch (c) { case '&': out += "&amp;"; break; case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break; case '"': out += "&quot;"; break; default: out += c; } }
    return out;
}
std::string csv_cell(std::string_view value) {
    std::string out = "\"";
    for (char c : value) { out += c; if (c == '"') out += '"'; }
    return out + '"';
}
}

std::string statistics_csv(const ScriptAnalysis& analysis,
                           const std::map<std::string, std::pair<long, long>>& targets) {
    std::ostringstream out;
    out << "\"speaker\",\"words\",\"lines\",\"word_target\",\"line_target\",\"word_delta\",\"line_delta\"\n";
    for (const auto& s : analysis.speakers) {
        const auto found = targets.find(s.name); const long wt = found == targets.end() ? 0 : found->second.first;
        const long lt = found == targets.end() ? 0 : found->second.second;
        out << csv_cell(s.name) << ',' << csv_cell(std::to_string(s.words)) << ',' << csv_cell(std::to_string(s.lines)) << ',';
        auto optional = [&out](long target, long actual) { if (target) out << csv_cell(std::to_string(target)); else out << "\"\"";
            out << ','; if (target) out << csv_cell(std::to_string(actual - target)); else out << "\"\""; };
        optional(wt, static_cast<long>(s.words)); out << ','; optional(lt, static_cast<long>(s.lines)); out << '\n';
    }
    return out.str();
}

std::string statistics_json(const ScriptAnalysis& a,
                            const std::map<std::string, std::pair<long, long>>& speaker_targets,
                            const std::map<std::string, std::pair<long, long>>& scene_targets,
                            long word_target, long line_target) {
    std::ostringstream o; o << "{\n  \"totals\": {\"words\": " << a.total_words << ", \"dialogueLines\": " << a.dialogue_lines
      << ", \"scriptLines\": " << a.script_lines << ", \"averageWords\": " << a.average_words
      << ", \"readingMinutes\": " << a.reading_minutes << ", \"wordTarget\": " << word_target << ", \"lineTarget\": " << line_target << "},\n";
    auto aggregates = [&](const char* key, const auto& values, const auto& targets) { o << "  \"" << key << "\": [";
        bool first = true; for (const auto& s : values) { if (!first) o << ','; first = false; auto it = targets.find(s.name);
            const long wt = it == targets.end() ? 0 : it->second.first, lt = it == targets.end() ? 0 : it->second.second;
            o << "\n    {\"" << (std::string(key)=="speakers"?"speaker":"scene") << "\": \"" << json_escape(s.name)
              << "\", \"words\": " << s.words << ", \"lines\": " << s.lines << ", \"targets\": {\"words\": " << wt << ", \"lines\": " << lt << "}}"; }
        if (!values.empty()) o << '\n'; o << "  ],\n"; };
    aggregates("speakers", a.speakers, speaker_targets); aggregates("scenes", a.scenes, scene_targets);
    o << "  \"warnings\": ["; for (std::size_t i=0;i<a.warnings.size();++i) { const auto& w=a.warnings[i]; if(i)o<<',';
        o << "\n    {\"type\": \""<<json_escape(w.type)<<"\", \"lineNumber\": "<<w.line_number<<", \"file\": \""<<json_escape(w.file)<<"\", \"message\": \""<<json_escape(w.message)<<"\"}"; }
    if(!a.warnings.empty())o<<'\n'; o << "  ],\n  \"countedLines\": [";
    for(std::size_t i=0;i<a.counted.size();++i){const auto& l=a.counted[i];if(i)o<<',';o<<"\n    {\"lineNumber\": "<<l.line_number<<", \"speaker\": \""<<json_escape(l.speaker)<<"\", \"text\": \""<<json_escape(l.text)<<"\", \"words\": "<<l.words<<", \"scene\": \""<<json_escape(l.scene)<<"\", \"raw\": \""<<json_escape(l.raw)<<"\"}";}
    if(!a.counted.empty())o<<'\n'; o << "  ]\n}\n"; return o.str();
}

std::string statistics_html(const ScriptAnalysis& a, std::string_view title, std::string_view generated_at) {
    std::ostringstream o; o << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><title>"
      << html_escape(title) << "</title><style>body{font:16px system-ui;max-width:900px;margin:2rem auto;padding:0 1rem;color:#222}table{border-collapse:collapse;width:100%}th,td{padding:.5rem;border-bottom:1px solid #ccc;text-align:left}th{background:#eee}</style></head><body><h1>"
      << html_escape(title) << "</h1><p>Generated " << html_escape(generated_at) << "</p><p><strong>" << a.total_words << "</strong> words · <strong>" << a.dialogue_lines << "</strong> dialogue lines · " << a.reading_minutes << " min reading time</p><h2>Speakers</h2><table><thead><tr><th>Speaker</th><th>Words</th><th>Lines</th></tr></thead><tbody>";
    for(const auto& s:a.speakers)o<<"<tr><td>"<<html_escape(s.name)<<"</td><td>"<<s.words<<"</td><td>"<<s.lines<<"</td></tr>";
    o<<"</tbody></table><h2>Scenes</h2><table><thead><tr><th>Scene</th><th>Words</th><th>Lines</th></tr></thead><tbody>";
    for(const auto& s:a.scenes)o<<"<tr><td>"<<html_escape(s.name)<<"</td><td>"<<s.words<<"</td><td>"<<s.lines<<"</td></tr>";
    return o.str()+"</tbody></table></body></html>\n";
}

VersionComparison compare_versions(const ScriptAnalysis& before, const ScriptAnalysis& after) {
    VersionComparison result;
    result.before_words = before.total_words; result.after_words = after.total_words;
    result.net_words = static_cast<long>(after.total_words) - static_cast<long>(before.total_words);
    result.added_words = result.net_words > 0 ? static_cast<std::size_t>(result.net_words) : 0;
    result.removed_words = result.net_words < 0 ? static_cast<std::size_t>(-result.net_words) : 0;
    std::map<std::string, std::pair<std::size_t, std::size_t>> speakers;
    for (const auto& speaker : before.speakers) speakers[speaker.name].first = speaker.words;
    for (const auto& speaker : after.speakers) speakers[speaker.name].second = speaker.words;
    for (const auto& [name, counts] : speakers) result.speakers.push_back({name, counts.first, counts.second,
        static_cast<long>(counts.second) - static_cast<long>(counts.first)});
    std::stable_sort(result.speakers.begin(), result.speakers.end(), [](const auto& left, const auto& right) {
        return std::labs(left.delta) > std::labs(right.delta);
    });
    return result;
}

}  // namespace say_count
