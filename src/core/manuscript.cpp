#include "core/manuscript.h"
#include "core/tokenizer.h"
#include "core/unicode_casefold.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>

namespace say_count {
namespace {

std::string Trim(std::string value) {
    const auto whitespace = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), whitespace));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), whitespace).base(), value.end());
    return value;
}

void AppendUtf8(std::string* output, std::uint32_t codepoint) {
    if (codepoint <= 0x7F) output->push_back(static_cast<char>(codepoint));
    else if (codepoint <= 0x7FF) {
        output->push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        output->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        output->push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        output->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        output->push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        output->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        output->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

std::string UnicodeCaseFold(std::string_view value) {
    std::string result;
    for (std::size_t index = 0; index < value.size();) {
        const auto first = static_cast<unsigned char>(value[index]);
        std::uint32_t codepoint = first;
        std::size_t length = 1;
        if ((first & 0xE0) == 0xC0) { codepoint = first & 0x1F; length = 2; }
        else if ((first & 0xF0) == 0xE0) { codepoint = first & 0x0F; length = 3; }
        else if ((first & 0xF8) == 0xF0) { codepoint = first & 0x07; length = 4; }
        if (index + length > value.size()) length = 1;
        bool valid = length == 1 || first >= 0xC2;
        for (std::size_t part = 1; valid && part < length; ++part) {
            const auto continuation = static_cast<unsigned char>(value[index + part]);
            if ((continuation & 0xC0) != 0x80) valid = false;
            else codepoint = (codepoint << 6) | (continuation & 0x3F);
        }
        if (!valid) {
            result.push_back(value[index++]);
            continue;
        }
        const auto begin = std::begin(unicode_casefold_data::kMappings);
        const auto end = std::end(unicode_casefold_data::kMappings);
        const auto found = std::lower_bound(begin, end, codepoint,
            [](const auto& mapping, std::uint32_t target) { return mapping.source < target; });
        if (found != end && found->source == codepoint) {
            for (std::size_t part = 0; part < found->length; ++part)
                AppendUtf8(&result, found->folded[part]);
        } else {
            AppendUtf8(&result, codepoint);
        }
        index += length;
    }
    return result;
}

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool EndsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

struct QuotedSpan {
    std::size_t begin;
    std::size_t end;
    std::string_view opening;
    std::string_view closing;
};

const std::vector<std::pair<std::string_view, std::string_view>>& QuotePairs() {
    static const std::vector<std::pair<std::string_view, std::string_view>> pairs{
        {"\"", "\""}, {"\xe2\x80\x9c", "\xe2\x80\x9d"},
        {"\xe2\x80\x98", "\xe2\x80\x99"}, {"\xc2\xab", "\xc2\xbb"},
        {"\xe2\x80\xb9", "\xe2\x80\xba"}, {"\xe2\x80\x9e", "\xe2\x80\x9c"},
        {"\xe3\x80\x8c", "\xe3\x80\x8d"}, {"\xe3\x80\x8e", "\xe3\x80\x8f"}
    };
    return pairs;
}

std::optional<QuotedSpan> FindQuotedSpan(std::string_view value, std::size_t from = 0) {
    std::optional<QuotedSpan> result;
    for (const auto& [opening, closing] : QuotePairs()) {
        const auto begin = value.find(opening, from);
        if (begin == std::string_view::npos || (result && begin >= result->begin)) continue;
        const auto close = value.find(closing, begin + opening.size());
        if (close == std::string_view::npos) continue;
        result = QuotedSpan{begin, close + closing.size(), opening, closing};
    }
    return result;
}

bool IsQuoted(std::string_view value) {
    const auto span = FindQuotedSpan(value);
    return span && span->begin == 0 && span->end == value.size();
}

std::string Unquote(std::string value) {
    const auto span = FindQuotedSpan(value);
    if (span && span->begin == 0 && span->end == value.size())
        return value.substr(span->opening.size(),
                            value.size() - span->opening.size() - span->closing.size());
    return value;
}

std::string Identifier(std::string_view value, std::string_view fallback) {
    std::string result;
    bool separator = false;
    for (const unsigned char byte : value) {
        if (byte < 0x80 && std::isalnum(byte)) {
            if (separator && !result.empty()) result += '_';
            result += static_cast<char>(std::tolower(byte));
            separator = false;
        } else {
            separator = true;
        }
    }
    while (!result.empty() && result.back() == '_') result.pop_back();
    if (result.empty()) result = std::string(fallback);
    if (std::isdigit(static_cast<unsigned char>(result.front()))) result.insert(0, "character_");
    return result;
}

std::string Escape(std::string_view value) {
    std::string result;
    result.reserve(value.size() + 8);
    for (char character : value) {
        if (character == '\\' || character == '"') result += '\\';
        if (character == '[') result += '[';  // Ren'Py's literal opening bracket is [[.
        result += character;
    }
    return result;
}

bool IsSpeakerName(std::string_view value) {
    if (value.empty() || value.size() > 60) return false;
    bool has_letter = false;
    for (const unsigned char byte : value) {
        if (byte < 0x80 && std::isalpha(byte)) has_letter = true;
        if (byte < 0x80 && !(std::isalnum(byte) || byte == ' ' || byte == '_' || byte == '-' || byte == '\'' || byte == '.'))
            return false;
    }
    return has_letter || std::any_of(value.begin(), value.end(), [](unsigned char c) { return c >= 0x80; });
}

struct SpeakerSpec {
    std::string name;
    std::string attributes;
};

std::optional<SpeakerSpec> ParseSpeakerSpec(std::string value) {
    value = Trim(std::move(value));
    SpeakerSpec result{value, {}};
    if (!value.empty() && value.back() == ']') {
        const auto open = value.find_last_of('[');
        if (open == std::string::npos) return std::nullopt;
        result.name = Trim(value.substr(0, open));
        std::string raw = value.substr(open + 1, value.size() - open - 2);
        std::replace(raw.begin(), raw.end(), ',', ' ');
        std::istringstream words(raw);
        for (std::string word; words >> word;) {
            if (word.empty() || !(std::isalpha(static_cast<unsigned char>(word.front())) || word.front() == '_') ||
                !std::all_of(word.begin() + 1, word.end(), [](unsigned char c) {
                    return std::isalnum(c) || c == '_';
                })) return std::nullopt;
            std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (!result.attributes.empty()) result.attributes += ' ';
            result.attributes += word;
        }
        if (result.attributes.empty()) return std::nullopt;
    }
    if (!IsSpeakerName(result.name)) return std::nullopt;
    return result;
}

const std::vector<std::string>& SpeechVerbs() {
    static const std::vector<std::string> verbs{
        "say", "says", "said", "ask", "asks", "asked", "reply", "replies", "replied",
        "answer", "answers", "answered", "whisper", "whispers", "whispered",
        "shout", "shouts", "shouted", "yell", "yells", "yelled", "murmur", "murmurs",
        "murmured", "mutter", "mutters", "muttered", "cry", "cries", "cried",
        "call", "calls", "called", "add", "adds", "added", "continue", "continues",
        "continued", "breathe", "breathes", "breathed", "hiss", "hisses", "hissed",
        "snap", "snaps", "snapped", "groan", "groans", "groaned", "sigh", "sighs",
        "sighed", "laugh", "laughs", "laughed", "exclaim", "exclaims", "exclaimed",
        "insist", "insists", "insisted", "admit", "admits", "admitted", "warn", "warns",
        "warned", "promise", "promises", "promised", "observe", "observes", "observed",
        "remark", "remarks", "remarked", "stammer", "stammers", "stammered",
        "demand", "demands", "demanded", "respond", "responds", "responded",
        "announce", "announces", "announced", "declare", "declares", "declared",
        "explain", "explains", "explained", "suggest", "suggests", "suggested",
        "urge", "urges", "urged", "protest", "protests", "protested",
        "plead", "pleads", "pleaded", "beg", "begs", "begged", "joke", "jokes", "joked",
        "agree", "agrees", "agreed", "object", "objects", "objected", "repeat", "repeats",
        "repeated"
    };
    return verbs;
}

bool IsPronoun(std::string_view value) {
    const std::string folded = UnicodeCaseFold(Trim(std::string(value)));
    return folded == "he" || folded == "she" || folded == "they" || folded == "he's" ||
           folded == "she's" || folded == "they're" || folded == "his" ||
           folded == "her" || folded == "their";
}

bool IsLikelyAdverb(std::string_view value) {
    std::string word = UnicodeCaseFold(Trim(std::string(value)));
    while (!word.empty() && std::ispunct(static_cast<unsigned char>(word.back()))) word.pop_back();
    static const std::set<std::string> common{
        "softly", "quietly", "loudly", "flatly", "dryly", "coldly", "warmly", "gently",
        "quickly", "slowly", "finally", "simply", "calmly", "angrily", "nervously",
        "hesitantly", "sharply", "firmly", "sadly", "brightly", "breathlessly",
        "cheerfully", "anxiously", "reluctantly", "impatiently", "excitedly", "weakly"
    };
    return common.count(word) != 0 || (word.size() > 4 && EndsWith(word, "ly"));
}

bool IdentifierWord(std::string_view value) {
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_'))
        return false;
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '_' || character == '.';
    });
}

bool LooksLikeSayStatement(const TokenizedLine& token) {
    if (token.type != LineType::Dialogue || token.quoted.empty()) return false;
    std::string prefix = Trim(token.code.substr(0, token.quoted.front().begin));
    if (!prefix.empty() && prefix.back() == ',') return false;  // Book attribution.
    if (!prefix.empty() && (prefix.back() == '.' || prefix.back() == '!' || prefix.back() == '?'))
        return false;  // Action sentence before book dialogue.
    std::istringstream words(prefix);
    std::string word;
    std::size_t count = 0;
    while (words >> word) {
        if (!IdentifierWord(word) || ++count > 3) return false;
    }
    return count > 0;
}

bool StrongRenpyLine(std::string_view line) {
    static const std::set<std::string> code_keywords{
        "image", "hide", "stop", "queue", "window", "with", "return", "if", "elif",
        "else", "while", "for", "init", "transform", "screen", "style", "voice", "pause",
        "camera", "translate", "pass", "default", "layeredimage", "nvl", "extend"
    };
    const std::string trimmed = Trim(std::string(line));
    if (trimmed.empty() || StartsWith(trimmed, "::")) return false;
    if (StartsWith(trimmed, "#")) return true;
    const auto token = tokenize_line(line, 0);
    switch (token.type) {
        case LineType::Label:
        case LineType::Define:
        case LineType::Jump:
        case LineType::Call:
        case LineType::Menu:
        case LineType::Python:
        case LineType::Scene:
        case LineType::Show:
        case LineType::Play:
            return true;
        case LineType::Dialogue:
            if (LooksLikeSayStatement(token)) return true;
            break;
        case LineType::Narration:
            if (token.indentation > 0) return true;
            break;
        default:
            break;
    }
    std::string keyword = token.keyword;
    std::transform(keyword.begin(), keyword.end(), keyword.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return code_keywords.count(keyword) != 0;
}

bool ProtectsIndentedBlock(const TokenizedLine& token) {
    if (token.code.empty() || token.code.back() != ':') return false;
    if (token.type == LineType::Python || token.type == LineType::Menu) return true;
    static const std::set<std::string> block_keywords{
        "screen", "transform", "style", "translate", "init", "image", "layeredimage"
    };
    std::string keyword = token.keyword;
    std::transform(keyword.begin(), keyword.end(), keyword.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return block_keywords.count(keyword) != 0;
}

std::size_t LeadingIndent(std::string_view line) {
    std::size_t result = 0;
    for (char character : line) {
        if (character == ' ') ++result;
        else if (character == '\t') result += 4;
        else break;
    }
    return result;
}

std::vector<std::string> Lines(std::string_view text) {
    std::vector<std::string> result;
    std::istringstream input{std::string(text)};
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        result.push_back(std::move(line));
    }
    if (text.empty()) result.clear();
    return result;
}

void AppendUniqueCharacters(std::vector<ManuscriptCharacter>* target,
                            const std::vector<ManuscriptCharacter>& additions) {
    for (const auto& character : additions) {
        const bool exists = std::any_of(target->begin(), target->end(), [&](const auto& item) {
            return item.alias == character.alias;
        });
        if (!exists) target->push_back(character);
        else if (character.uses_image_attributes) {
            const auto found = std::find_if(target->begin(), target->end(), [&](const auto& item) {
                return item.alias == character.alias;
            });
            found->uses_image_attributes = true;
        }
    }
}

void AppendUniqueStrings(std::vector<std::string>* target,
                         const std::vector<std::string>& additions) {
    for (const auto& addition : additions)
        if (std::find(target->begin(), target->end(), addition) == target->end())
            target->push_back(addition);
}

std::string ResolveSpeaker(std::string candidate, std::string_view recent_speaker) {
    candidate = Trim(std::move(candidate));
    if (!candidate.empty() && candidate.back() == ',') candidate.pop_back();
    candidate = Trim(std::move(candidate));
    return IsPronoun(candidate) ? std::string(recent_speaker) : candidate;
}

std::string InferExpression(std::string_view phrase) {
    static const std::map<std::string, std::string> cues{
        {"happy", "happy"}, {"happily", "happy"}, {"cheerful", "happy"},
        {"cheerfully", "happy"}, {"smile", "happy"}, {"smiled", "happy"}, {"smiles", "happy"},
        {"grin", "happy"}, {"grinned", "happy"}, {"grins", "happy"}, {"beamed", "happy"},
        {"angry", "angry"}, {"angrily", "angry"}, {"furious", "angry"},
        {"furiously", "angry"}, {"glared", "angry"}, {"glares", "angry"},
        {"sad", "sad"}, {"sadly", "sad"}, {"sorrowful", "sad"},
        {"sorrowfully", "sad"}, {"frowned", "sad"}, {"frowns", "sad"},
        {"nervous", "nervous"}, {"nervously", "nervous"}, {"anxious", "nervous"},
        {"anxiously", "nervous"}, {"worried", "nervous"},
        {"surprised", "surprised"}, {"startled", "surprised"}, {"gasped", "surprised"},
        {"embarrassed", "embarrassed"}, {"shyly", "embarrassed"}, {"blushed", "embarrassed"}
    };
    std::istringstream input(UnicodeCaseFold(phrase));
    std::vector<std::string> words;
    for (std::string word; input >> word;) {
        while (!word.empty() && std::ispunct(static_cast<unsigned char>(word.front()))) word.erase(word.begin());
        while (!word.empty() && std::ispunct(static_cast<unsigned char>(word.back()))) word.pop_back();
        if (!word.empty()) words.push_back(std::move(word));
    }
    std::string result;
    for (std::size_t index = 0; index < words.size(); ++index) {
        const auto found = cues.find(words[index]);
        if (found == cues.end()) continue;
        if (index > 0 && (words[index - 1] == "not" || words[index - 1] == "never" ||
                          words[index - 1] == "no")) continue;
        result = found->second;
    }
    return result;
}

bool IsSpeechVerb(std::string word) {
    while (!word.empty() && std::ispunct(static_cast<unsigned char>(word.front()))) word.erase(word.begin());
    while (!word.empty() && std::ispunct(static_cast<unsigned char>(word.back()))) word.pop_back();
    word = UnicodeCaseFold(word);
    return std::find(SpeechVerbs().begin(), SpeechVerbs().end(), word) != SpeechVerbs().end();
}

std::string JoinWords(const std::vector<std::string>& words) {
    std::string result;
    for (const auto& word : words) {
        if (!result.empty()) result += ' ';
        result += word;
    }
    return result;
}

std::string StripWordPunctuation(std::string word) {
    while (!word.empty() && (word.front() == ',' || word.front() == ';' || word.front() == ':'))
        word.erase(word.begin());
    while (!word.empty() && (word.back() == ',' || word.back() == ';' || word.back() == ':' ||
                             word.back() == '.' || word.back() == '!' || word.back() == '?'))
        word.pop_back();
    return word;
}

bool ParseAttributionPhrase(std::string phrase, std::string_view recent_speaker,
                            std::string* speaker, std::string* attributes) {
    phrase = Trim(std::move(phrase));
    while (!phrase.empty() && (phrase.front() == ',' || phrase.front() == ';'))
        phrase = Trim(phrase.substr(1));
    while (!phrase.empty() && (phrase.back() == '.' || phrase.back() == ',' || phrase.back() == ';'))
        phrase.pop_back();
    std::istringstream input(phrase);
    std::vector<std::string> words;
    for (std::string word; input >> word;) words.push_back(std::move(word));
    auto verb_begin = words.begin();
    if (!words.empty()) {
        const std::string first = UnicodeCaseFold(StripWordPunctuation(words.front()));
        static const std::set<std::string> introductions{
            "with", "after", "before", "without", "despite", "as", "at", "to", "while"
        };
        if (introductions.count(first)) {
            const auto comma = std::find_if(words.begin(), words.end(), [](const auto& word) {
                return word.find(',') != std::string::npos;
            });
            if (comma != words.end()) verb_begin = std::next(comma);
        }
    }
    const auto verb = std::find_if(verb_begin, words.end(), IsSpeechVerb);
    if (verb == words.end()) return false;
    const std::size_t verb_index = static_cast<std::size_t>(verb - words.begin());
    const bool speaker_follows_verb = verb_index == 0 || std::all_of(
        words.begin(), verb, [](const auto& word) { return IsLikelyAdverb(word); });
    std::vector<std::string> name_words;
    if (!speaker_follows_verb) {
        name_words.assign(words.begin(), words.begin() + static_cast<std::ptrdiff_t>(verb_index));
        const std::string first = UnicodeCaseFold(StripWordPunctuation(name_words.front()));
        static const std::set<std::string> introductions{
            "with", "after", "before", "without", "despite", "as", "at", "to", "while"
        };
        if (introductions.count(first)) {
            for (std::size_t index = name_words.size(); index-- > 0;) {
                if (name_words[index].find(',') == std::string::npos) continue;
                name_words.erase(name_words.begin(), name_words.begin() +
                    static_cast<std::ptrdiff_t>(index + 1));
                break;
            }
        }
        while (name_words.size() > 1 && IsLikelyAdverb(name_words.front()))
            name_words.erase(name_words.begin());
        while (name_words.size() > 1 && IsLikelyAdverb(name_words.back())) name_words.pop_back();
    } else {
        name_words.assign(std::next(verb), words.end());
        while (name_words.size() > 1 && IsLikelyAdverb(name_words.front()))
            name_words.erase(name_words.begin());
        static const std::set<std::string> modifiers{
            "with", "while", "as", "after", "before", "and", "but", "then"
        };
        for (std::size_t index = 1; index < name_words.size(); ++index) {
            const std::string folded = UnicodeCaseFold(StripWordPunctuation(name_words[index]));
            if (modifiers.count(folded) || IsLikelyAdverb(name_words[index])) {
                name_words.resize(index);
                break;
            }
        }
    }
    for (auto& word : name_words) word = StripWordPunctuation(std::move(word));
    name_words.erase(std::remove(name_words.begin(), name_words.end(), std::string{}), name_words.end());
    std::string name = JoinWords(name_words);
    name = ResolveSpeaker(std::move(name), recent_speaker);
    const auto spec = ParseSpeakerSpec(name);
    if (!spec) return false;
    *speaker = spec->name;
    *attributes = spec->attributes.empty() ? InferExpression(phrase) : spec->attributes;
    return true;
}

bool ParseAttributed(std::string_view line, std::string_view recent_speaker,
                     std::string* speaker, std::string* attributes, std::string* dialogue) {
    // Alice said happily, "Hello." / Alice [happy] said, "Hello."
    const auto first = FindQuotedSpan(line);
    if (!first) return false;
    const auto quote_text = [&](const QuotedSpan& span) {
        return std::string(line.substr(span.begin + span.opening.size(),
            span.end - span.begin - span.opening.size() - span.closing.size()));
    };
    if (first->begin > 0) {
        if (ParseAttributionPhrase(std::string(line.substr(0, first->begin)),
                                   recent_speaker, speaker, attributes)) {
            *dialogue = quote_text(*first);
            for (auto next = FindQuotedSpan(line, first->end); next; next = FindQuotedSpan(line, next->end)) {
                if (!dialogue->empty()) *dialogue += ' ';
                *dialogue += quote_text(*next);
            }
            return true;
        }
    }

    // "Hello," Alice said. International quote pairs are accepted too.
    if (first->begin != 0) return false;
    const auto second = FindQuotedSpan(line, first->end);
    const std::size_t attribution_end = second ? second->begin : line.size();
    std::string attribution = Trim(std::string(line.substr(first->end, attribution_end - first->end)));
    if (!attribution.empty() && attribution.front() == ',') attribution = Trim(attribution.substr(1));
    if (!attribution.empty() && attribution.back() == '.') attribution.pop_back();
    if (ParseAttributionPhrase(attribution, recent_speaker, speaker, attributes)) {
        *dialogue = quote_text(*first);
        for (auto next = second; next; next = FindQuotedSpan(line, next->end)) {
            if (!dialogue->empty()) *dialogue += ' ';
            *dialogue += quote_text(*next);
        }
        return true;
    }
    return false;
}

struct RichDialogue {
    std::string speaker;
    std::string attributes;
    std::string dialogue;
    std::string narration_before;
    std::string narration_after;
};

std::string SpeakerFromAction(
    std::string action, const std::map<std::string, std::string>& existing_characters,
    std::string_view recent_speaker) {
    action = Trim(std::move(action));
    if (!action.empty() && action.back() == '.') action.pop_back();
    std::string best;
    const std::string folded_action = UnicodeCaseFold(action);
    for (const auto& [alias, display] : existing_characters) {
        (void)alias;
        if (display.size() <= best.size()) continue;
        const std::string folded_display = UnicodeCaseFold(display);
        for (auto found = folded_action.find(folded_display); found != std::string::npos;
             found = folded_action.find(folded_display, found + 1)) {
            const bool left = found == 0 || !std::isalnum(static_cast<unsigned char>(folded_action[found - 1]));
            const std::size_t end = found + folded_display.size();
            const bool right = end == folded_action.size() ||
                !std::isalnum(static_cast<unsigned char>(folded_action[end]));
            if (left && right) { best = display; break; }
        }
    }
    if (!best.empty()) return best;

    std::string subject = action;
    const std::string folded_subject = UnicodeCaseFold(subject);
    static const std::vector<std::string> introductions{
        "with ", "after ", "before ", "without ", "despite ", "as ", "at ", "to "
    };
    if (std::any_of(introductions.begin(), introductions.end(), [&](const auto& prefix) {
            return StartsWith(folded_subject, prefix);
        })) {
        const auto comma = subject.find(',');
        if (comma != std::string::npos) subject = Trim(subject.substr(comma + 1));
    }

    std::istringstream first_words(subject);
    std::string first;
    first_words >> first;
    while (!first.empty() && std::ispunct(static_cast<unsigned char>(first.back()))) first.pop_back();
    if (IsPronoun(first)) return std::string(recent_speaker);

    // Possessive action beats: "Eileen's hand tightened."
    const auto possessive = subject.find("'s ");
    if (possessive != std::string::npos) {
        std::string candidate = Trim(subject.substr(0, possessive));
        const auto comma = candidate.find_last_of(",;:—");
        if (comma != std::string::npos) candidate = Trim(candidate.substr(comma + 1));
        if (IsSpeakerName(candidate)) return candidate;
    }

    std::istringstream words(subject);
    std::string name, word;
    while (words >> word) {
        const auto first = static_cast<unsigned char>(word.front());
        if (first < 0x80 && std::islower(first)) break;
        if (!word.empty() && (word.back() == ',' || word.back() == '.')) word.pop_back();
        if (!IsSpeakerName(word)) break;
        if (!name.empty()) name += ' ';
        name += word;
        if (std::count(name.begin(), name.end(), ' ') >= 2) break;
    }
    return name;
}

bool ParseActionBeat(std::string_view line, const ManuscriptOptions& options,
                     std::string_view recent_speaker, RichDialogue* result) {
    std::string attributed_speaker, attributed_dialogue;
    std::string attributed_attributes;
    if (ParseAttributed(line, recent_speaker, &attributed_speaker,
                        &attributed_attributes, &attributed_dialogue)) {
        result->speaker = std::move(attributed_speaker);
        result->attributes = std::move(attributed_attributes);
        result->dialogue = std::move(attributed_dialogue);
        return true;
    }

    const auto quote = FindQuotedSpan(line);
    if (!quote) return false;
    const auto quote_text = [&] {
        return std::string(line.substr(quote->begin + quote->opening.size(),
            quote->end - quote->begin - quote->opening.size() - quote->closing.size()));
    };

    if (quote->begin == 0 && quote->end < line.size()) {
        std::string action = Trim(std::string(line.substr(quote->end)));
        const std::string speaker = SpeakerFromAction(action, options.existing_characters, recent_speaker);
        if (speaker.empty()) return false;
        result->speaker = speaker;
        result->attributes = InferExpression(action);
        result->dialogue = quote_text();
        result->narration_after = std::move(action);
        return true;
    }
    if (quote->begin > 0 && quote->end == line.size()) {
        std::string action = Trim(std::string(line.substr(0, quote->begin)));
        const std::string speaker = SpeakerFromAction(action, options.existing_characters, recent_speaker);
        if (speaker.empty()) return false;
        result->speaker = speaker;
        result->attributes = InferExpression(action);
        result->dialogue = quote_text();
        result->narration_before = std::move(action);
        return true;
    }
    // Eileen turned away. "Go," she said.
    if (quote->begin > 0 && quote->end < line.size()) {
        std::string suffix = Trim(std::string(line.substr(quote->end)));
        std::string speaker, attributes;
        if (!suffix.empty() && suffix.front() == ',') suffix = Trim(suffix.substr(1));
        if (ParseAttributionPhrase(suffix, recent_speaker, &speaker, &attributes)) {
            result->speaker = std::move(speaker);
            result->attributes = attributes.empty()
                ? InferExpression(std::string(line.substr(0, quote->begin))) : std::move(attributes);
            result->dialogue = quote_text();
            result->narration_before = Trim(std::string(line.substr(0, quote->begin)));
            return true;
        }
    }
    return false;
}

struct NamedDialogue {
    SpeakerSpec speaker;
    std::string text;
};

std::optional<SpeakerSpec> ParseDirectedSpeaker(std::string value) {
    if (const auto direct = ParseSpeakerSpec(value)) return direct;
    value = Trim(std::move(value));
    if (value.empty() || value.back() != ')') return std::nullopt;
    const auto open = value.find_last_of('(');
    if (open == std::string::npos || open == 0) return std::nullopt;
    auto speaker = ParseSpeakerSpec(Trim(value.substr(0, open)));
    if (!speaker) return std::nullopt;
    speaker->attributes = InferExpression(value.substr(open + 1, value.size() - open - 2));
    return speaker;
}

std::optional<NamedDialogue> ParseNamedDialogue(
    std::string_view line, std::string_view recent_speaker = {}) {
    const std::vector<std::string_view> separators{":", " \xe2\x80\x94 ", " \xe2\x80\x93 ", " - "};
    for (const auto separator : separators) {
        const auto position = line.find(separator);
        if (position == std::string_view::npos) continue;
        const std::string left = Trim(std::string(line.substr(0, position)));
        std::string right = Trim(std::string(line.substr(position + separator.size())));
        if (right.empty()) continue;
        if (separator == ":") {
            std::string speaker, attributes;
            if (ParseAttributionPhrase(left, recent_speaker, &speaker, &attributes))
                return NamedDialogue{{std::move(speaker), std::move(attributes)},
                                     Unquote(std::move(right))};
        }
        if (const auto spec = ParseDirectedSpeaker(left))
            return NamedDialogue{*spec, Unquote(std::move(right))};
    }
    return std::nullopt;
}

bool IsUppercaseSpeakerCue(std::string_view value) {
    bool has_letter = false;
    for (const unsigned char byte : value) {
        if (byte >= 0x80) continue;
        if (std::islower(byte)) return false;
        if (std::isalpha(byte)) has_letter = true;
    }
    return has_letter;
}

bool IsParenthetical(std::string_view value) {
    return value.size() >= 2 && value.front() == '(' && value.back() == ')';
}

struct ScreenplayBlock {
    SpeakerSpec speaker;
    std::string text;
    std::size_t lines = 0;
};

std::optional<ScreenplayBlock> ParseScreenplayBlock(
    const std::vector<std::string>& lines, std::size_t index) {
    if (index + 1 >= lines.size()) return std::nullopt;
    if (StrongRenpyLine(lines[index])) return std::nullopt;
    std::string cue = Trim(lines[index]);
    std::string direction;
    bool explicit_colon = !cue.empty() && cue.back() == ':';
    if (explicit_colon) cue = Trim(cue.substr(0, cue.size() - 1));
    const auto parenthetical = cue.find_last_of('(');
    if (parenthetical != std::string::npos && cue.back() == ')' && parenthetical > 0) {
        direction = cue.substr(parenthetical + 1, cue.size() - parenthetical - 2);
        cue = Trim(cue.substr(0, parenthetical));
    }
    auto speaker = ParseSpeakerSpec(cue);
    if (!speaker || (!explicit_colon && !IsUppercaseSpeakerCue(speaker->name)))
        return std::nullopt;

    std::size_t spoken_index = index + 1;
    std::string spoken = Trim(lines[spoken_index]);
    if (IsParenthetical(spoken) && spoken_index + 1 < lines.size()) {
        if (!direction.empty()) direction += ' ';
        direction += spoken.substr(1, spoken.size() - 2);
        spoken = Trim(lines[++spoken_index]);
    }
    if (spoken.empty() || StartsWith(spoken, "::") || StrongRenpyLine(lines[spoken_index]))
        return std::nullopt;
    if (StartsWith(spoken, "- ")) spoken = Trim(spoken.substr(2));
    else if (StartsWith(spoken, "\xe2\x80\x94 ") || StartsWith(spoken, "\xe2\x80\x93 "))
        spoken = Trim(spoken.substr(3));
    if (speaker->attributes.empty()) speaker->attributes = InferExpression(direction);
    return ScreenplayBlock{*speaker, Unquote(std::move(spoken)), spoken_index - index + 1};
}

std::vector<ManuscriptLineReview> ClassifyLines(std::string_view text) {
    const auto lines = Lines(text);
    std::vector<ManuscriptLineReview> result;
    result.reserve(lines.size());
    std::optional<std::size_t> protected_indent;
    std::string recent_speaker;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const std::string trimmed = Trim(lines[index]);
        ManuscriptLineKind kind = ManuscriptLineKind::Blank;
        if (!trimmed.empty()) {
            const auto token = tokenize_line(lines[index], index + 1);
            if (protected_indent && token.indentation > *protected_indent) {
                kind = ManuscriptLineKind::Renpy;
            } else {
                if (protected_indent && token.indentation <= *protected_indent) protected_indent.reset();
                if (ProtectsIndentedBlock(token)) {
                    protected_indent = token.indentation;
                    kind = ManuscriptLineKind::Renpy;
                } else {
                    if (const auto screenplay = ParseScreenplayBlock(lines, index)) {
                        recent_speaker = screenplay->speaker.name;
                        for (std::size_t offset = 0; offset < screenplay->lines; ++offset)
                            result.push_back({index + offset + 1, ManuscriptLineKind::Prose,
                                              lines[index + offset]});
                        index += screenplay->lines - 1;
                        continue;
                    }
                    const bool speaker_block = ParseSpeakerSpec(trimmed).has_value() && index + 1 < lines.size() &&
                        IsQuoted(Trim(lines[index + 1]));
                    if (speaker_block) {
                        kind = ManuscriptLineKind::Prose;
                        recent_speaker = ParseSpeakerSpec(trimmed)->name;
                    } else {
                        const bool unindented_renpy_narration = token.type == LineType::Narration &&
                            token.indentation == 0 && IsQuoted(trimmed) &&
                            !StartsWith(trimmed, "\xe2\x80\x9c");
                        std::string speaker, dialogue;
                        RichDialogue rich;
                        ManuscriptOptions options;
                        const auto named = ParseNamedDialogue(trimmed, recent_speaker);
                        const bool named_dialogue = named.has_value();
                        std::string attributes;
                        const bool direct_attribution = ParseAttributed(
                            trimmed, recent_speaker, &speaker, &attributes, &dialogue);
                        const bool action_beat = !direct_attribution &&
                            ParseActionBeat(trimmed, options, recent_speaker, &rich);
                        const bool attributed = direct_attribution || action_beat;
                        const bool natural_quotation = IsQuoted(trimmed) && !StartsWith(trimmed, "\"");
                        if (attributed || natural_quotation)
                            kind = ManuscriptLineKind::Prose;
                        else if (StrongRenpyLine(lines[index]) || unindented_renpy_narration)
                            kind = ManuscriptLineKind::Renpy;
                        else if (StartsWith(trimmed, "::") || named_dialogue)
                            kind = ManuscriptLineKind::Prose;
                        else
                            kind = ManuscriptLineKind::Uncertain;
                        if (direct_attribution && !speaker.empty()) recent_speaker = speaker;
                        else if (action_beat && !rich.speaker.empty()) recent_speaker = rich.speaker;
                        else if (named_dialogue) recent_speaker = named->speaker.name;
                    }
                }
            }
        }
        result.push_back({index + 1, kind, lines[index]});
        if (kind == ManuscriptLineKind::Prose && ParseSpeakerSpec(trimmed) && index + 1 < lines.size() &&
            IsQuoted(Trim(lines[index + 1]))) {
            result.push_back({index + 2, ManuscriptLineKind::Prose, lines[index + 1]});
            ++index;
        }
    }
    return result;
}

}  // namespace

std::vector<ManuscriptLineReview> review_manuscript_lines(std::string_view text) {
    return ClassifyLines(text);
}

bool manuscript_contains_renpy_code(std::string_view text) {
    std::istringstream input{std::string(text)};
    for (std::string line; std::getline(input, line);) {
        if (StrongRenpyLine(line)) return true;
    }
    return false;
}

ManuscriptConversion convert_mixed_manuscript_to_renpy(
    std::string_view text, bool include_character_definitions,
    const std::map<std::string, std::string>& existing_characters,
    bool include_uncertain_lines) {
    const auto lines = Lines(text);
    if (lines.empty()) return {};
    const std::string newline = text.find("\r\n") != std::string_view::npos ? "\r\n" : "\n";
    const bool trailing_newline = EndsWith(text, "\n");

    const auto reviews = ClassifyLines(text);
    std::vector<bool> prose(lines.size(), false);
    for (const auto& review : reviews) {
        prose[review.line_number - 1] = review.kind == ManuscriptLineKind::Prose ||
            (include_uncertain_lines && review.kind == ManuscriptLineKind::Uncertain);
    }

    ManuscriptConversion result;
    std::vector<std::string> output;
    const bool has_scene_label = std::any_of(lines.begin(), lines.end(), [](const auto& line) {
        const std::string trimmed = Trim(line);
        return StartsWith(trimmed, "::") || tokenize_line(line, 0).type == LineType::Label;
    });
    bool opening_label_added = false;
    for (std::size_t index = 0; index < lines.size();) {
        if (!prose[index]) {
            output.push_back(lines[index++]);
            continue;
        }
        const std::size_t start = index;
        if (include_character_definitions && !has_scene_label && !opening_label_added) {
            if (!output.empty() && !output.back().empty()) output.push_back({});
            output.push_back("label start:");
            opening_label_added = true;
        }
        std::size_t indentation = LeadingIndent(lines[index]);
        while (index < lines.size() && prose[index]) {
            const auto current = LeadingIndent(lines[index]);
            if (current > 0) indentation = indentation == 0 ? current : std::min(indentation, current);
            ++index;
        }
        std::string block;
        for (std::size_t line = start; line < index; ++line) {
            if (line > start) block += '\n';
            block += lines[line];
        }
        ManuscriptOptions options;
        options.label.clear();
        options.include_character_definitions = false;
        options.indentation = indentation > 0 ? indentation : 4;
        options.existing_characters = existing_characters;
        const auto converted = convert_manuscript_to_renpy(block, options);
        AppendUniqueCharacters(&result.characters, converted.characters);
        AppendUniqueStrings(&result.reused_aliases, converted.reused_aliases);
        result.dialogue_lines += converted.dialogue_lines;
        result.narration_lines += converted.narration_lines;
        auto converted_lines = Lines(converted.script);
        output.insert(output.end(), converted_lines.begin(), converted_lines.end());
    }

    if (include_character_definitions && !result.characters.empty()) {
        std::set<std::string> existing;
        for (const auto& line : lines) {
            const auto token = tokenize_line(line, 0);
            if (token.type != LineType::Define) continue;
            std::istringstream subject(token.subject);
            std::string alias;
            if (subject >> alias) existing.insert(alias);
        }
        std::vector<std::string> definitions;
        for (const auto& character : result.characters) {
            if (existing.count(character.alias)) continue;
            definitions.push_back("define " + character.alias + " = Character(\"" +
                Escape(character.name) + "\"" + (character.uses_image_attributes
                    ? ", image=\"" + Escape(character.alias) + "\"" : std::string{}) + ")");
        }
        if (!definitions.empty()) {
            definitions.push_back({});
            output.insert(output.begin(), definitions.begin(), definitions.end());
        }
    }

    std::ostringstream joined;
    for (std::size_t index = 0; index < output.size(); ++index) {
        if (index) joined << newline;
        joined << output[index];
    }
    if (trailing_newline) joined << newline;
    result.script = joined.str();
    return result;
}

ManuscriptConversion convert_manuscript_to_renpy(
    std::string_view manuscript, const ManuscriptOptions& options) {
    std::vector<std::string> lines;
    std::istringstream input{std::string(manuscript)};
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(Trim(std::move(line)));
    }

    struct Entry { std::string label; std::string speaker; std::string attributes; std::string text; };
    std::vector<Entry> entries;
    std::vector<ManuscriptCharacter> characters;
    std::map<std::string, std::string> alias_by_name;
    std::set<std::string> used_aliases;
    std::set<std::string> existing_aliases;
    std::set<std::string> reused_aliases;
    for (const auto& [alias, display_name] : options.existing_characters) {
        used_aliases.insert(UnicodeCaseFold(alias));
        existing_aliases.insert(UnicodeCaseFold(alias));
        alias_by_name.emplace(UnicodeCaseFold(alias), alias);
        alias_by_name.emplace(UnicodeCaseFold(display_name), alias);
    }
    auto alias_for = [&](const std::string& name) -> std::string {
        const std::string key = UnicodeCaseFold(Trim(name));
        const auto existing = alias_by_name.find(key);
        if (existing != alias_by_name.end()) {
            if (existing_aliases.count(UnicodeCaseFold(existing->second)))
                reused_aliases.insert(existing->second);
            return existing->second;
        }
        std::string base = Identifier(name, "character");
        std::string alias = base;
        for (std::size_t suffix = 2; used_aliases.count(alias); ++suffix)
            alias = base + "_" + std::to_string(suffix);
        used_aliases.insert(alias);
        alias_by_name.emplace(key, alias);
        alias_by_name.emplace(UnicodeCaseFold(alias), alias);
        characters.push_back({name, alias});
        return alias;
    };
    auto mark_image_attributes = [&](const std::string& alias) {
        const auto found = std::find_if(characters.begin(), characters.end(), [&](const auto& character) {
            return character.alias == alias;
        });
        if (found != characters.end()) found->uses_image_attributes = true;
    };

    std::string recent_speaker;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto& line = lines[index];
        if (line.empty()) continue;
        if (StartsWith(line, "::")) {
            const std::string label = Identifier(Trim(line.substr(2)), "scene");
            entries.push_back({label, {}, {}, {}});
            continue;
        }

        std::string speaker, attributes, dialogue;
        RichDialogue rich;
        if (const auto screenplay = ParseScreenplayBlock(lines, index)) {
            speaker = screenplay->speaker.name;
            attributes = screenplay->speaker.attributes;
            dialogue = screenplay->text;
            index += screenplay->lines - 1;
        } else if (IsQuoted(line)) {
            dialogue = Unquote(line);
        } else if (const auto named = ParseNamedDialogue(line, recent_speaker)) {
            speaker = named->speaker.name;
            attributes = named->speaker.attributes;
            dialogue = named->text;
        } else if (ParseActionBeat(line, options, recent_speaker, &rich)) {
            if (!rich.narration_before.empty())
                entries.push_back({{}, {}, {}, rich.narration_before});
            const std::string alias = alias_for(rich.speaker);
            if (!rich.attributes.empty()) mark_image_attributes(alias);
            entries.push_back({{}, alias, rich.attributes, rich.dialogue});
            recent_speaker = rich.speaker;
            if (!rich.narration_after.empty())
                entries.push_back({{}, {}, {}, rich.narration_after});
            continue;
        } else if (ParseSpeakerSpec(line) && index + 1 < lines.size() && IsQuoted(lines[index + 1])) {
            const auto spec = ParseSpeakerSpec(line);
            speaker = spec ? spec->name : line;
            attributes = spec ? spec->attributes : std::string{};
            dialogue = Unquote(lines[++index]);
        } else {
            dialogue = Unquote(line);
        }
        if (!speaker.empty()) {
            const std::string alias = alias_for(speaker);
            if (!attributes.empty()) mark_image_attributes(alias);
            entries.push_back({{}, alias, attributes, dialogue});
            recent_speaker = speaker;
        } else {
            entries.push_back({{}, {}, {}, dialogue});
        }
    }

    ManuscriptConversion result;
    result.characters = characters;
    result.reused_aliases.assign(reused_aliases.begin(), reused_aliases.end());
    std::ostringstream script;
    if (options.include_character_definitions && !characters.empty()) {
        for (const auto& character : characters) {
            script << "define " << character.alias << " = Character(\"" << Escape(character.name) << "\"";
            if (character.uses_image_attributes)
                script << ", image=\"" << Escape(character.alias) << "\"";
            script << ")\n";
        }
        script << '\n';
    }
    bool inside_label = false;
    if (!options.label.empty()) {
        script << "label " << Identifier(options.label, "start") << ":\n";
        inside_label = true;
    }
    for (const auto& entry : entries) {
        if (!entry.label.empty()) {
            if (script.tellp() > 0 && !EndsWith(script.str(), "\n\n")) script << '\n';
            script << "label " << entry.label << ":\n";
            inside_label = true;
            continue;
        }
        // A blank opening label means this is a snippet intended for an existing label.
        if (inside_label || options.label.empty()) script << std::string(options.indentation, ' ');
        if (!entry.speaker.empty()) {
            script << entry.speaker << ' ';
            if (!entry.attributes.empty()) script << entry.attributes << ' ';
            ++result.dialogue_lines;
        } else {
            ++result.narration_lines;
        }
        script << '"' << Escape(entry.text) << "\"\n";
    }
    result.script = script.str();
    return result;
}

}  // namespace say_count
