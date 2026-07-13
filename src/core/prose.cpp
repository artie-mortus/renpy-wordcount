#include "core/prose.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace say_count {
namespace {

const std::unordered_set<std::string>& Stopwords() {
    static const std::unordered_set<std::string> words = [] {
        constexpr const char* source =
            "a an the and or but nor of to in on at by for with from as into onto over under after before between through during "
            "is are was were be been being am do does did done doing have has had having will would can could should shall may might must "
            "i you he she it we they me him her us them my your his hers its our their mine yours myself yourself himself herself itself ourselves themselves "
            "this that these those there here what which who whom whose why how when where whether because while until unless although though since "
            "not no if then else than s t ll re ve d m "
            "dont didnt doesnt isnt arent wasnt werent wont wouldnt couldnt shouldnt cant cannot im ive ill id youre youve youll youd hes shes theyre theyve theyll weve well wed lets thats whats heres theres";
        std::unordered_set<std::string> result;
        std::string current;
        for (const char character : std::string(source) + " ") {
            if (character == ' ') { if (!current.empty()) result.insert(std::move(current)); current.clear(); }
            else current += character;
        }
        return result;
    }();
    return words;
}

std::string Lower(std::string value) {
    for (char& character : value) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte < 0x80) character = static_cast<char>(std::tolower(byte));
    }
    return value;
}

std::string Bare(std::string value) {
    value.erase(std::remove(value.begin(), value.end(), '\''), value.end());
    const std::string curly = "\xe2\x80\x99";
    for (auto found = value.find(curly); found != std::string::npos; found = value.find(curly))
        value.erase(found, curly.size());
    return value;
}

std::vector<std::string> SpeakerWords(const std::string& value) {
    auto words = word_tokens(Lower(value));
    for (auto& word : words) word = Lower(std::move(word));
    return words;
}

template <typename CountMap>
std::vector<ProseFrequency> Frequencies(const CountMap& counts,
                                        const std::vector<std::string>& order,
                                        std::size_t minimum) {
    std::vector<ProseFrequency> result;
    for (const auto& text : order) {
        const auto found = counts.find(text);
        if (found != counts.end() && found->second >= minimum)
            result.push_back({text, found->second});
    }
    std::stable_sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.count > right.count;
    });
    return result;
}

}  // namespace

ProseAnalysis analyze_prose(const ScriptAnalysis& analysis,
                            const std::set<std::string>& extra_exclusions) {
    std::unordered_set<std::string> names;
    for (const auto& speaker : analysis.speakers)
        for (auto word : SpeakerWords(speaker.name)) names.insert(std::move(word));
    for (const auto& [alias, display] : analysis.character_names) {
        names.insert(Lower(alias));
        for (auto word : SpeakerWords(display)) names.insert(std::move(word));
    }
    std::unordered_set<std::string> exclusions;
    for (const auto& word : extra_exclusions) exclusions.insert(Lower(word));

    ProseAnalysis result;
    std::unordered_set<std::string> unique;
    std::unordered_map<std::string, std::size_t> word_counts, phrase_counts;
    std::vector<std::string> word_order, phrase_order;
    std::unordered_map<std::string, std::size_t> speaker_index;

    for (const auto& item : analysis.counted) {
        if (item.is_menu_choice) continue;
        auto tokens = word_tokens(Lower(item.text));
        for (auto& token : tokens) token = Lower(std::move(token));
        result.total_words += tokens.size();
        for (const auto& token : tokens) {
            unique.insert(token);
            const std::string bare = Bare(token);
            if (token.size() < 3 || Stopwords().count(bare) || names.count(token) ||
                exclusions.count(token) || (!token.empty() && std::isdigit(static_cast<unsigned char>(token.front()))))
                continue;
            if (!word_counts.count(token)) word_order.push_back(token);
            ++word_counts[token];
        }
        for (std::size_t size = 2; size <= 3; ++size) {
            for (std::size_t index = 0; index + size <= tokens.size(); ++index) {
                bool all_stopwords = true;
                std::string phrase;
                for (std::size_t part = 0; part < size; ++part) {
                    const auto& token = tokens[index + part];
                    if (!Stopwords().count(Bare(token))) all_stopwords = false;
                    if (part) phrase += ' ';
                    phrase += token;
                }
                if (all_stopwords) continue;
                if (!phrase_counts.count(phrase)) phrase_order.push_back(phrase);
                ++phrase_counts[phrase];
            }
        }

        auto [speaker, inserted] = speaker_index.emplace(item.speaker, result.speakers.size());
        if (inserted) result.speakers.push_back({item.speaker});
        auto& stats = result.speakers[speaker->second];
        std::size_t segment_start = 0;
        for (std::size_t index = 0; index <= item.text.size(); ++index) {
            const bool ellipsis = index + 2 < item.text.size() &&
                item.text.compare(index, 3, "\xe2\x80\xa6") == 0;
            const bool boundary = index == item.text.size() || item.text[index] == '.' ||
                item.text[index] == '!' || item.text[index] == '?' || ellipsis;
            if (!boundary) continue;
            const std::size_t words = count_words(std::string_view(item.text).substr(segment_start, index - segment_start));
            if (words) { ++stats.sentences; stats.words += words; stats.longest_sentence = std::max(stats.longest_sentence, words); }
            if (ellipsis) index += 2;
            segment_start = index + 1;
        }
    }
    result.unique_words = unique.size();
    result.overused_words = Frequencies(word_counts, word_order, 4);
    if (result.overused_words.size() > 8) result.overused_words.resize(8);

    auto repeated = Frequencies(phrase_counts, phrase_order, 3);
    std::vector<ProseFrequency> trigrams, bigrams;
    for (const auto& phrase : repeated) {
        if (std::count(phrase.text.begin(), phrase.text.end(), ' ') == 2 && trigrams.size() < 6)
            trigrams.push_back(phrase);
    }
    for (const auto& phrase : repeated) {
        if (std::count(phrase.text.begin(), phrase.text.end(), ' ') != 1) continue;
        if (std::none_of(trigrams.begin(), trigrams.end(), [&](const auto& longer) {
            return longer.text.find(phrase.text) != std::string::npos;
        })) bigrams.push_back(phrase);
    }
    result.repeated_phrases = std::move(trigrams);
    result.repeated_phrases.insert(result.repeated_phrases.end(), bigrams.begin(), bigrams.end());
    std::stable_sort(result.repeated_phrases.begin(), result.repeated_phrases.end(),
        [](const auto& left, const auto& right) { return left.count > right.count; });
    if (result.repeated_phrases.size() > 6) result.repeated_phrases.resize(6);
    return result;
}

}  // namespace say_count
