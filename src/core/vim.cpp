#include "core/vim.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>
#include <utility>
#include <vector>

namespace say_count {
namespace {

constexpr std::size_t kIndentWidth = 4;
constexpr std::size_t kHalfPageLines = 15;
constexpr std::size_t kFullPageLines = 30;
constexpr std::size_t kMaxCount = 100000;

bool IsContinuation(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

std::size_t NextCp(std::string_view text, std::size_t offset) {
    if (offset >= text.size()) return text.size();
    ++offset;
    while (offset < text.size() && IsContinuation(text[offset])) ++offset;
    return offset;
}

std::size_t PrevCp(std::string_view text, std::size_t offset) {
    if (offset == 0) return 0;
    --offset;
    while (offset > 0 && IsContinuation(text[offset])) --offset;
    return offset;
}

std::size_t LineStartAt(std::string_view text, std::size_t offset) {
    offset = std::min(offset, text.size());
    while (offset > 0 && text[offset - 1] != '\n') --offset;
    return offset;
}

std::size_t LineEndAt(std::string_view text, std::size_t offset) {
    offset = std::min(offset, text.size());
    while (offset < text.size() && text[offset] != '\n') ++offset;
    return offset;
}

std::vector<std::size_t> LineStarts(std::string_view text) {
    std::vector<std::size_t> starts{0};
    for (std::size_t index = 0; index < text.size(); ++index)
        if (text[index] == '\n') starts.push_back(index + 1);
    return starts;
}

std::size_t LineOf(const std::vector<std::size_t>& starts, std::size_t offset) {
    const auto found = std::upper_bound(starts.begin(), starts.end(), offset);
    return static_cast<std::size_t>(found - starts.begin()) - 1;
}

std::size_t FirstNonBlank(std::string_view text, std::size_t line_start) {
    const std::size_t end = LineEndAt(text, line_start);
    std::size_t offset = line_start;
    while (offset < end && (text[offset] == ' ' || text[offset] == '\t')) ++offset;
    return offset;
}

std::size_t ClampToLine(std::string_view text, std::size_t offset) {
    offset = std::min(offset, text.size());
    while (offset > 0 && offset < text.size() && IsContinuation(text[offset])) --offset;
    const std::size_t start = LineStartAt(text, offset);
    const std::size_t end = LineEndAt(text, offset);
    if (offset >= end && end > start) offset = PrevCp(text, end);
    return offset;
}

bool IsLineBlank(char c) { return c == ' ' || c == '\t'; }

int ClassOf(std::string_view text, std::size_t offset, bool big) {
    const char c = text[offset];
    if (c == ' ' || c == '\t' || c == '\n') return 0;
    if (big) return 1;
    const auto u = static_cast<unsigned char>(c);
    if (std::isalnum(u) || c == '_' || u >= 0x80) return 2;
    return 1;
}

std::size_t WordForward(std::string_view text, std::size_t offset, bool big) {
    if (offset >= text.size()) return text.size();
    const int klass = ClassOf(text, offset, big);
    if (klass != 0) {
        while (offset < text.size() && text[offset] != '\n' &&
               ClassOf(text, offset, big) == klass) {
            offset = NextCp(text, offset);
        }
    }
    while (offset < text.size()) {
        if (text[offset] == '\n') {
            const std::size_t next = offset + 1;
            if (next < text.size() && text[next] == '\n') return next;
            offset = next;
            continue;
        }
        if (ClassOf(text, offset, big) != 0) break;
        offset = NextCp(text, offset);
    }
    return offset;
}

std::size_t WordEnd(std::string_view text, std::size_t offset, bool big) {
    if (text.empty()) return 0;
    offset = NextCp(text, offset);
    while (offset < text.size() &&
           (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\n')) {
        ++offset;
    }
    if (offset >= text.size()) return PrevCp(text, text.size());
    const int klass = ClassOf(text, offset, big);
    std::size_t last = offset;
    while (offset < text.size() && text[offset] != '\n' &&
           ClassOf(text, offset, big) == klass) {
        last = offset;
        offset = NextCp(text, offset);
    }
    return last;
}

std::size_t WordBack(std::string_view text, std::size_t offset, bool big) {
    if (offset == 0 || text.empty()) return 0;
    offset = PrevCp(text, std::min(offset, text.size()));
    while (offset > 0 &&
           (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\n')) {
        if (text[offset] == '\n' && text[offset - 1] == '\n') return offset;
        offset = PrevCp(text, offset);
    }
    if (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\n') return offset;
    const int klass = ClassOf(text, offset, big);
    while (offset > 0) {
        const std::size_t previous = PrevCp(text, offset);
        if (text[previous] == '\n' || ClassOf(text, previous, big) != klass) break;
        offset = previous;
    }
    return offset;
}

std::size_t ParagraphForward(std::string_view text, std::size_t offset) {
    std::size_t line_end = LineEndAt(text, offset);
    bool seen_content = LineStartAt(text, offset) != line_end;
    while (line_end < text.size()) {
        const std::size_t start = line_end + 1;
        const std::size_t end = LineEndAt(text, start);
        if (start == end && seen_content) return start;
        if (start != end) seen_content = true;
        line_end = end;
    }
    return text.size();
}

std::size_t ParagraphBack(std::string_view text, std::size_t offset) {
    std::size_t start = LineStartAt(text, offset);
    bool seen_content = start != LineEndAt(text, start);
    while (start > 0) {
        const std::size_t previous_start = LineStartAt(text, start - 1);
        const std::size_t previous_end = LineEndAt(text, previous_start);
        if (previous_start == previous_end && seen_content) return previous_start;
        if (previous_start != previous_end) seen_content = true;
        start = previous_start;
    }
    return 0;
}

std::size_t MatchBracket(std::string_view text, std::size_t offset, bool* found) {
    *found = false;
    const std::string_view open = "([{";
    const std::string_view close = ")]}";
    const std::size_t line_end = LineEndAt(text, offset);
    std::size_t probe = offset;
    char bracket = 0;
    while (probe < line_end) {
        const char c = text[probe];
        if (open.find(c) != std::string_view::npos ||
            close.find(c) != std::string_view::npos) {
            bracket = c;
            break;
        }
        ++probe;
    }
    if (!bracket) return offset;
    const bool forward = open.find(bracket) != std::string_view::npos;
    const char other = forward ? close[open.find(bracket)] : open[close.find(bracket)];
    int depth = 0;
    if (forward) {
        for (std::size_t index = probe; index < text.size(); ++index) {
            if (text[index] == bracket) ++depth;
            else if (text[index] == other && --depth == 0) {
                *found = true;
                return index;
            }
        }
    } else {
        for (std::size_t index = probe + 1; index-- > 0;) {
            if (text[index] == bracket) ++depth;
            else if (text[index] == other && --depth == 0) {
                *found = true;
                return index;
            }
        }
    }
    return offset;
}

std::size_t ParseCount(const std::string& digits) {
    if (digits.empty()) return 1;
    const unsigned long value = std::strtoul(digits.c_str(), nullptr, 10);
    return std::clamp<std::size_t>(static_cast<std::size_t>(value), 1, kMaxCount);
}

std::string VimPatternToRegex(std::string_view pattern) {
    std::string converted;
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        if (pattern[index] == '\\' && index + 1 < pattern.size() &&
            (pattern[index + 1] == '<' || pattern[index + 1] == '>')) {
            converted += "\\b";
            ++index;
        } else {
            converted += pattern[index];
        }
    }
    return converted;
}

std::string VimReplacementToFormat(std::string_view replacement) {
    std::string converted;
    for (std::size_t index = 0; index < replacement.size(); ++index) {
        const char c = replacement[index];
        if (c == '\\' && index + 1 < replacement.size()) {
            const char next = replacement[++index];
            if (next >= '0' && next <= '9') {
                converted.push_back('$');
                converted.push_back(next);
            } else if (next == '&') {
                converted.push_back('&');
            } else {
                converted.push_back(next);
            }
        } else if (c == '&') {
            converted += "$&";
        } else if (c == '$') {
            converted += "$$";
        } else {
            converted.push_back(c);
        }
    }
    return converted;
}

std::vector<std::string> SplitLines(std::string_view source) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= source.size()) {
        const auto newline = source.find('\n', start);
        if (newline == std::string_view::npos) {
            lines.emplace_back(source.substr(start));
            break;
        }
        lines.emplace_back(source.substr(start, newline - start));
        start = newline + 1;
    }
    if (lines.empty()) lines.emplace_back();
    return lines;
}

std::string JoinLines(const std::vector<std::string>& lines) {
    std::string result;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index) result.push_back('\n');
        result += lines[index];
    }
    return result;
}

bool IsEscapeKey(std::string_view key) {
    return key == "<Esc>" || key == "<C-[>" || key == "<C-c>";
}

}  // namespace

void VimEmulator::Reset() {
    mode_ = Mode::Normal;
    cmdline_.clear();
    ClearPending();
}

void VimEmulator::ClearPending() {
    op_ = 0;
    count_.clear();
    op_count_.clear();
    char_wait_ = CharWait::None;
    g_pending_ = false;
}

std::size_t VimEmulator::EffectiveCount() const {
    return std::min(kMaxCount, ParseCount(count_) * ParseCount(op_count_));
}

bool VimEmulator::HasExplicitCount() const {
    return !count_.empty() || !op_count_.empty();
}

std::size_t VimEmulator::ExplicitCount() const {
    return ParseCount(count_.empty() ? op_count_ : count_);
}

VimState VimEmulator::ApplyKey(std::string_view source, std::size_t caret,
                               std::string_view key) {
    text_.assign(source);
    caret_ = std::min(caret, text_.size());
    anchor_ = std::min(anchor_, text_.size());
    host_undo_ = host_redo_ = host_save_ = false;
    search_match_ = false;
    message_.clear();
    goal_column_used_ = false;

    if (mode_ == Mode::Insert) {
        if (IsEscapeKey(key)) {
            mode_ = Mode::Normal;
            const std::size_t start = LineStartAt(text_, caret_);
            if (caret_ > start) caret_ = PrevCp(text_, caret_);
            caret_ = ClampToLine(text_, caret_);
            ClearPending();
        }
        return MakeState();
    }
    if (mode_ == Mode::Command) {
        HandleCommandKey(key);
        return MakeState();
    }
    if (char_wait_ != CharWait::None) {
        HandleCharWait(key);
    } else {
        HandleKey(key);
    }
    if (!goal_column_used_) goal_column_active_ = false;
    if (mode_ != Mode::Insert && mode_ != Mode::Command)
        caret_ = ClampToLine(text_, caret_);
    return MakeState();
}

VimState VimEmulator::MakeState() const {
    VimState state;
    state.text = text_;
    state.caret = std::min(caret_, text_.size());
    state.selection_start = state.selection_end = state.caret;
    state.visual_anchor = std::min(anchor_, text_.size());
    state.host_undo = host_undo_;
    state.host_redo = host_redo_;
    state.host_save = host_save_;
    state.search_match = search_match_;
    state.search_match_start = search_match_start_;
    state.search_match_end = search_match_end_;
    switch (mode_) {
        case Mode::Normal:
            state.mode = op_ ? "no" : "n";
            break;
        case Mode::Insert:
            state.mode = "i";
            break;
        case Mode::Command:
            state.mode = "c";
            state.command_line = cmdline_;
            break;
        case Mode::VisualChar: {
            state.mode = "v";
            state.visual = true;
            const std::size_t low = std::min(state.visual_anchor, state.caret);
            const std::size_t high = std::max(state.visual_anchor, state.caret);
            state.selection_start = low;
            state.selection_end = NextCp(text_, high);
            break;
        }
        case Mode::VisualLine: {
            state.mode = "V";
            state.visual = true;
            state.visual_line = true;
            const std::size_t low = std::min(state.visual_anchor, state.caret);
            const std::size_t high = std::max(state.visual_anchor, state.caret);
            state.selection_start = LineStartAt(text_, low);
            const std::size_t end = LineEndAt(text_, high);
            state.selection_end = end < text_.size() ? end + 1 : text_.size();
            break;
        }
        case Mode::VisualBlock:
            state.mode = std::string(1, '\x16');
            state.visual = true;
            state.visual_block = true;
            state.selection_start = std::min(state.visual_anchor, state.caret);
            state.selection_end = NextCp(text_, std::max(state.visual_anchor, state.caret));
            break;
    }
    if (mode_ != Mode::Command && !message_.empty()) state.command_line = message_;
    return state;
}

void VimEmulator::HandleCommandKey(std::string_view key) {
    if (IsEscapeKey(key)) {
        cmdline_.clear();
        mode_ = Mode::Normal;
        return;
    }
    if (key == "<CR>") {
        ExecuteCommandLine();
        return;
    }
    if (key == "<BS>") {
        if (cmdline_.size() <= 1) {
            cmdline_.clear();
            mode_ = Mode::Normal;
        } else {
            cmdline_.erase(PrevCp(cmdline_, cmdline_.size()));
        }
        return;
    }
    if (key == "<Tab>" || key == "<S-Tab>") return;
    if (!key.empty() && (key.size() == 1 || key[0] != '<')) cmdline_.append(key);
}

void VimEmulator::ExecuteCommandLine() {
    const std::string line = cmdline_;
    cmdline_.clear();
    mode_ = Mode::Normal;
    if (line.size() < 1) return;
    const char type = line[0];
    std::string_view body(line);
    body.remove_prefix(1);
    if (type == '/') {
        if (!body.empty()) last_search_ = std::string(body);
        SearchMove(1);
        return;
    }
    if (type != ':') return;

    const auto starts = LineStarts(text_);
    const std::size_t last_line = starts.size() - 1;
    const std::size_t cursor_line = LineOf(starts, caret_);
    std::size_t first = cursor_line;
    std::size_t last = cursor_line;
    bool have_range = false;
    const auto parse_address = [&](std::string_view& rest, std::size_t* out) {
        if (rest.empty()) return false;
        if (std::isdigit(static_cast<unsigned char>(rest[0]))) {
            std::size_t value = 0;
            std::size_t consumed = 0;
            while (consumed < rest.size() &&
                   std::isdigit(static_cast<unsigned char>(rest[consumed]))) {
                value = std::min<std::size_t>(value * 10 + (rest[consumed] - '0'), kMaxCount);
                ++consumed;
            }
            rest.remove_prefix(consumed);
            *out = value ? value - 1 : 0;
            return true;
        }
        if (rest[0] == '.') {
            rest.remove_prefix(1);
            *out = cursor_line;
            return true;
        }
        if (rest[0] == '$') {
            rest.remove_prefix(1);
            *out = last_line;
            return true;
        }
        return false;
    };
    if (body.rfind("'<,'>", 0) == 0) {
        first = visual_first_line_;
        last = visual_last_line_;
        have_range = true;
        body.remove_prefix(5);
    } else if (!body.empty() && body[0] == '%') {
        first = 0;
        last = last_line;
        have_range = true;
        body.remove_prefix(1);
    } else if (parse_address(body, &first)) {
        last = first;
        have_range = true;
        if (!body.empty() && body[0] == ',') {
            body.remove_prefix(1);
            parse_address(body, &last);
        }
    }
    first = std::min(first, last_line);
    last = std::min(last, last_line);
    if (first > last) std::swap(first, last);

    if (body.empty()) {
        if (have_range) caret_ = ClampToLine(text_, FirstNonBlank(text_, starts[last]));
        return;
    }
    if (body == "w" || body == "write" || body == "wq" || body == "x") {
        host_save_ = true;
        return;
    }
    if (body[0] == 'q') return;
    if (body.rfind("noh", 0) == 0) return;
    if (body[0] == 's' && body.size() >= 2 &&
        !std::isalnum(static_cast<unsigned char>(body[1]))) {
        ExecuteSubstitute(first, last, body);
    }
}

void VimEmulator::ExecuteSubstitute(std::size_t first_line, std::size_t last_line,
                                    std::string_view body) {
    const char separator = body[1];
    std::vector<std::string> parts{""};
    for (std::size_t index = 2; index < body.size(); ++index) {
        const char c = body[index];
        if (c == '\\' && index + 1 < body.size()) {
            if (body[index + 1] == separator) {
                parts.back().push_back(separator);
            } else {
                parts.back().push_back('\\');
                parts.back().push_back(body[index + 1]);
            }
            ++index;
        } else if (c == separator) {
            parts.emplace_back();
        } else {
            parts.back().push_back(c);
        }
    }
    std::string pattern = parts[0];
    const std::string replacement = parts.size() > 1 ? parts[1] : "";
    const std::string flags = parts.size() > 2 ? parts[2] : "";
    if (pattern.empty()) pattern = last_search_;
    if (pattern.empty()) return;
    last_search_ = pattern;
    std::regex expression;
    try {
        expression.assign(VimPatternToRegex(pattern), std::regex::ECMAScript);
    } catch (const std::regex_error&) {
        return;
    }
    const bool global = flags.find('g') != std::string::npos;
    const std::string format = VimReplacementToFormat(replacement);
    auto lines = SplitLines(text_);
    last_line = std::min(last_line, lines.size() - 1);
    first_line = std::min(first_line, last_line);
    const auto match_flags = global
        ? std::regex_constants::format_default
        : std::regex_constants::format_first_only;
    for (std::size_t index = first_line; index <= last_line; ++index) {
        try {
            lines[index] = std::regex_replace(lines[index], expression, format, match_flags);
        } catch (const std::regex_error&) {
            return;
        }
    }
    text_ = JoinLines(lines);
    const auto starts = LineStarts(text_);
    const std::size_t target = std::min(last_line, starts.size() - 1);
    caret_ = ClampToLine(text_, FirstNonBlank(text_, starts[target]));
}

bool VimEmulator::SearchMove(int direction) {
    if (last_search_.empty()) {
        message_ = "No previous search pattern";
        return false;
    }
    std::regex expression;
    try {
        expression.assign(VimPatternToRegex(last_search_), std::regex::ECMAScript);
    } catch (const std::regex_error&) {
        message_ = "Invalid pattern: " + last_search_;
        return false;
    }
    std::vector<std::pair<std::size_t, std::size_t>> matches;
    const auto begin = std::sregex_iterator(text_.begin(), text_.end(), expression);
    for (auto found = begin; found != std::sregex_iterator(); ++found) {
        if (found->length() == 0) break;
        matches.emplace_back(static_cast<std::size_t>(found->position()),
                             static_cast<std::size_t>(found->length()));
    }
    if (matches.empty()) {
        message_ = "Pattern not found: " + last_search_;
        return false;
    }
    auto selected = matches.begin();
    if (direction > 0) {
        const auto found = std::upper_bound(
            matches.begin(), matches.end(), caret_,
            [](std::size_t position, const auto& match) { return position < match.first; });
        selected = found == matches.end() ? matches.begin() : found;
    } else {
        const auto found = std::lower_bound(
            matches.begin(), matches.end(), caret_,
            [](const auto& match, std::size_t position) { return match.first < position; });
        selected = found == matches.begin() ? matches.end() - 1 : found - 1;
    }
    caret_ = ClampToLine(text_, selected->first);
    search_match_ = true;
    search_match_start_ = selected->first;
    search_match_end_ = selected->first + selected->second;
    message_ = "/" + last_search_;
    return true;
}

void VimEmulator::EnterInsert(std::size_t caret) {
    mode_ = Mode::Insert;
    caret_ = std::min(caret, text_.size());
    op_ = 0;
}

void VimEmulator::ApplyCharwise(char op, std::size_t begin, std::size_t end) {
    begin = std::min(begin, text_.size());
    end = std::min(end, text_.size());
    if (begin > end) std::swap(begin, end);
    switch (op) {
        case 'y':
            register_ = text_.substr(begin, end - begin);
            register_linewise_ = false;
            caret_ = ClampToLine(text_, begin);
            break;
        case 'd':
            register_ = text_.substr(begin, end - begin);
            register_linewise_ = false;
            text_.erase(begin, end - begin);
            caret_ = ClampToLine(text_, begin);
            break;
        case 'c':
            register_ = text_.substr(begin, end - begin);
            register_linewise_ = false;
            text_.erase(begin, end - begin);
            EnterInsert(begin);
            break;
        case '~': {
            for (std::size_t index = begin; index < end; ++index) {
                const auto u = static_cast<unsigned char>(text_[index]);
                if (std::isupper(u)) text_[index] = static_cast<char>(std::tolower(u));
                else if (std::islower(u)) text_[index] = static_cast<char>(std::toupper(u));
            }
            caret_ = ClampToLine(text_, begin);
            break;
        }
        case 'u':
        case 'U': {
            for (std::size_t index = begin; index < end; ++index) {
                const auto u = static_cast<unsigned char>(text_[index]);
                text_[index] = static_cast<char>(op == 'u' ? std::tolower(u) : std::toupper(u));
            }
            caret_ = ClampToLine(text_, begin);
            break;
        }
        case '>':
        case '<': {
            const auto starts = LineStarts(text_);
            const std::size_t last = end > begin ? PrevCp(text_, end) : begin;
            ApplyLinewise(op, LineOf(starts, begin), LineOf(starts, last));
            break;
        }
        default:
            break;
    }
}

void VimEmulator::ApplyLinewise(char op, std::size_t first_line, std::size_t last_line) {
    auto starts = LineStarts(text_);
    last_line = std::min(last_line, starts.size() - 1);
    first_line = std::min(first_line, last_line);
    const std::size_t begin = starts[first_line];
    const std::size_t end = last_line + 1 < starts.size() ? starts[last_line + 1]
                                                          : text_.size();
    switch (op) {
        case 'y':
            register_ = text_.substr(begin, end - begin);
            if (register_.empty() || register_.back() != '\n') register_.push_back('\n');
            register_linewise_ = true;
            caret_ = ClampToLine(text_, FirstNonBlank(text_, begin));
            break;
        case 'd': {
            register_ = text_.substr(begin, end - begin);
            if (register_.empty() || register_.back() != '\n') register_.push_back('\n');
            register_linewise_ = true;
            if (end >= text_.size() && begin > 0) text_.erase(begin - 1, end - begin + 1);
            else text_.erase(begin, end - begin);
            const auto remaining = LineStarts(text_);
            const std::size_t target = std::min(first_line, remaining.size() - 1);
            caret_ = ClampToLine(text_, FirstNonBlank(text_, remaining[target]));
            break;
        }
        case 'c': {
            register_ = text_.substr(begin, end - begin);
            if (register_.empty() || register_.back() != '\n') register_.push_back('\n');
            register_linewise_ = true;
            const std::size_t indent_end = FirstNonBlank(text_, begin);
            std::string replacement = text_.substr(begin, indent_end - begin);
            const bool had_newline = end > begin && text_[end - 1] == '\n';
            if (had_newline) replacement.push_back('\n');
            text_.replace(begin, end - begin, replacement);
            EnterInsert(begin + replacement.size() - (had_newline ? 1 : 0));
            break;
        }
        case '>':
        case '<': {
            for (std::size_t line = last_line + 1; line-- > first_line;) {
                const std::size_t start = starts[line];
                const std::size_t line_end = LineEndAt(text_, start);
                if (op == '>') {
                    if (line_end > start) text_.insert(start, std::string(kIndentWidth, ' '));
                } else {
                    std::size_t remove = 0;
                    while (remove < kIndentWidth && start + remove < line_end &&
                           text_[start + remove] == ' ') {
                        ++remove;
                    }
                    if (remove == 0 && start < line_end && text_[start] == '\t') remove = 1;
                    text_.erase(start, remove);
                }
            }
            caret_ = ClampToLine(text_, FirstNonBlank(text_, LineStartAt(text_, starts[first_line])));
            break;
        }
        default:
            break;
    }
}

void VimEmulator::ApplyVisualOperator(char op) {
    const auto starts = LineStarts(text_);
    const std::size_t low = std::min(anchor_, caret_);
    const std::size_t high = std::max(anchor_, caret_);
    if (mode_ == Mode::VisualLine) {
        mode_ = Mode::Normal;
        ApplyLinewise(op, LineOf(starts, low), LineOf(starts, high));
    } else if (mode_ == Mode::VisualBlock) {
        mode_ = Mode::Normal;
        const std::size_t first_line = LineOf(starts, low);
        const std::size_t last_line = LineOf(starts, high);
        const std::size_t column_a = low - starts[first_line];
        const std::size_t column_b = high - starts[last_line];
        const std::size_t left = std::min(column_a, column_b);
        const std::size_t right = std::max(column_a, column_b);
        if (op == 'y' || op == 'd' || op == 'c') {
            std::string collected;
            for (std::size_t line = first_line; line <= last_line; ++line) {
                const std::size_t start = starts[line];
                const std::size_t line_end = LineEndAt(text_, start);
                const std::size_t from = std::min(start + left, line_end);
                std::size_t to = std::min(start + right, line_end);
                if (to < line_end) to = NextCp(text_, to);
                if (line > first_line) collected.push_back('\n');
                collected += text_.substr(from, to - from);
            }
            register_ = collected;
            register_linewise_ = false;
            if (op == 'd' || op == 'c') {
                for (std::size_t line = last_line + 1; line-- > first_line;) {
                    const std::size_t start = starts[line];
                    const std::size_t line_end = LineEndAt(text_, start);
                    const std::size_t from = std::min(start + left, line_end);
                    std::size_t to = std::min(start + right, line_end);
                    if (to < line_end) to = NextCp(text_, to);
                    text_.erase(from, to - from);
                }
            }
            const std::size_t landing = std::min(starts[first_line] + left, text_.size());
            if (op == 'c') EnterInsert(std::min(landing, LineEndAt(text_, starts[first_line])));
            else caret_ = ClampToLine(text_, landing);
        } else {
            ApplyCharwise(op, low, NextCp(text_, high));
        }
    } else {
        mode_ = Mode::Normal;
        ApplyCharwise(op, low, NextCp(text_, high));
    }
    ClearPending();
}

void VimEmulator::Paste(bool after) {
    if (register_.empty()) return;
    const std::size_t count = EffectiveCount();
    std::string payload;
    for (std::size_t repeat = 0; repeat < count; ++repeat) payload += register_;
    if (register_linewise_) {
        const std::size_t line_start = LineStartAt(text_, caret_);
        const std::size_t line_end = LineEndAt(text_, caret_);
        if (after) {
            if (line_end >= text_.size()) {
                if (!payload.empty() && payload.back() == '\n') payload.pop_back();
                const std::size_t insert_at = text_.size();
                text_ += "\n" + payload;
                caret_ = ClampToLine(text_, FirstNonBlank(text_, insert_at + 1));
            } else {
                text_.insert(line_end + 1, payload);
                caret_ = ClampToLine(text_, FirstNonBlank(text_, line_end + 1));
            }
        } else {
            text_.insert(line_start, payload);
            caret_ = ClampToLine(text_, FirstNonBlank(text_, line_start));
        }
    } else {
        const std::size_t line_end = LineEndAt(text_, caret_);
        std::size_t position = caret_;
        if (after && caret_ < line_end) position = NextCp(text_, caret_);
        text_.insert(position, payload);
        caret_ = ClampToLine(text_, PrevCp(text_, position + payload.size()));
    }
}

void VimEmulator::FindChar(CharWait kind, std::string_view character, std::size_t count) {
    if (character.empty()) return;
    const bool forward = kind == CharWait::FindF || kind == CharWait::FindT;
    const bool till = kind == CharWait::FindT || kind == CharWait::FindTillBack;
    const std::size_t line_start = LineStartAt(text_, caret_);
    const std::size_t line_end = LineEndAt(text_, caret_);
    std::size_t target = caret_;
    bool found = false;
    if (forward) {
        std::size_t probe = NextCp(text_, caret_);
        while (probe < line_end && count) {
            if (text_.compare(probe, character.size(), character) == 0) {
                if (--count == 0) {
                    target = probe;
                    found = true;
                    break;
                }
            }
            probe = NextCp(text_, probe);
        }
        if (found && till) target = PrevCp(text_, target);
    } else {
        std::size_t probe = caret_;
        while (probe > line_start && count) {
            probe = PrevCp(text_, probe);
            if (text_.compare(probe, character.size(), character) == 0) {
                if (--count == 0) {
                    target = probe;
                    found = true;
                    break;
                }
            }
        }
        if (found && till) target = NextCp(text_, target);
    }
    if (!found) {
        op_ = 0;
        return;
    }
    if (op_) {
        if (forward) ApplyCharwise(op_, caret_, NextCp(text_, target));
        else ApplyCharwise(op_, target, caret_);
        op_ = 0;
    } else {
        caret_ = target;
    }
}

void VimEmulator::HandleCharWait(std::string_view key) {
    const CharWait wait = char_wait_;
    char_wait_ = CharWait::None;
    if (key.empty() || (key.size() > 1 && key[0] == '<')) {
        ClearPending();
        return;
    }
    const std::size_t count = EffectiveCount();
    if (wait == CharWait::Replace) {
        if (mode_ == Mode::VisualChar || mode_ == Mode::VisualLine ||
            mode_ == Mode::VisualBlock) {
            const std::size_t low = std::min(anchor_, caret_);
            const std::size_t high = NextCp(text_, std::max(anchor_, caret_));
            std::string rebuilt;
            for (std::size_t index = low; index < high;) {
                if (text_[index] == '\n') {
                    rebuilt.push_back('\n');
                    ++index;
                } else {
                    rebuilt.append(key);
                    index = NextCp(text_, index);
                }
            }
            text_.replace(low, high - low, rebuilt);
            mode_ = Mode::Normal;
            caret_ = ClampToLine(text_, low);
            ClearPending();
            return;
        }
        const std::size_t line_end = LineEndAt(text_, caret_);
        std::size_t probe = caret_;
        for (std::size_t step = 0; step < count; ++step) {
            if (probe >= line_end) {
                ClearPending();
                return;
            }
            probe = NextCp(text_, probe);
        }
        std::string replacement;
        for (std::size_t step = 0; step < count; ++step) replacement.append(key);
        text_.replace(caret_, probe - caret_, replacement);
        caret_ = ClampToLine(text_, caret_ + replacement.size() - key.size());
        ClearPending();
        return;
    }
    FindChar(wait, key, count);
    find_char_ = std::string(key);
    find_kind_ = wait;
    ClearPending();
}

void VimEmulator::ApplyMotion(std::size_t target, bool inclusive) {
    if (op_) {
        const std::size_t low = std::min(caret_, target);
        std::size_t high = std::max(caret_, target);
        if (inclusive) high = NextCp(text_, high);
        ApplyCharwise(op_, low, high);
        op_ = 0;
    } else {
        caret_ = target;
    }
    count_.clear();
    op_count_.clear();
}

void VimEmulator::HandleKey(std::string_view key) {
    const bool visual = mode_ == Mode::VisualChar || mode_ == Mode::VisualLine ||
                        mode_ == Mode::VisualBlock;
    if (IsEscapeKey(key)) {
        if (visual) mode_ = Mode::Normal;
        ClearPending();
        return;
    }
    if (g_pending_) {
        g_pending_ = false;
        if (key == "g") {
            const auto starts = LineStarts(text_);
            const std::size_t line = HasExplicitCount()
                ? std::min(ExplicitCount() - 1, starts.size() - 1) : 0;
            if (op_) {
                const std::size_t current = LineOf(starts, caret_);
                ApplyLinewise(op_, std::min(current, line), std::max(current, line));
                op_ = 0;
            } else {
                caret_ = ClampToLine(text_, FirstNonBlank(text_, starts[line]));
            }
        }
        count_.clear();
        op_count_.clear();
        return;
    }
    if (key.size() == 1 && std::isdigit(static_cast<unsigned char>(key[0])) &&
        (key[0] != '0' || !count_.empty())) {
        count_ += key;
        return;
    }
    if (key == "d" || key == "c" || key == "y" || key == ">" || key == "<") {
        const char op = key[0];
        if (visual) {
            ApplyVisualOperator(op);
            return;
        }
        if (op_ == op) {
            const auto starts = LineStarts(text_);
            const std::size_t line = LineOf(starts, caret_);
            ApplyLinewise(op, line, line + EffectiveCount() - 1);
            ClearPending();
            return;
        }
        if (op_) {
            ClearPending();
            return;
        }
        op_ = op;
        op_count_.swap(count_);
        count_.clear();
        return;
    }

    std::string k(key);
    if (k == "<Left>") k = "h";
    else if (k == "<Right>") k = "l";
    else if (k == "<Up>") k = "k";
    else if (k == "<Down>") k = "j";
    else if (k == "<Home>") k = "0";
    else if (k == "<End>") k = "$";
    else if (k == "<Del>") k = "x";
    else if (k == "<S-Left>") k = "b";
    else if (k == "<S-Right>") k = "w";
    else if (k == "<CR>") k = "+";

    const std::size_t count = EffectiveCount();
    const auto starts = LineStarts(text_);
    const std::size_t current_line = LineOf(starts, caret_);
    const std::size_t last_line = starts.size() - 1;

    const auto clear_counts = [this] {
        count_.clear();
        op_count_.clear();
    };
    const auto line_motion = [&](std::size_t target_line, bool to_first_nonblank) {
        target_line = std::min(target_line, last_line);
        if (op_) {
            ApplyLinewise(op_, std::min(current_line, target_line),
                          std::max(current_line, target_line));
            op_ = 0;
        } else {
            const std::size_t start = starts[target_line];
            if (to_first_nonblank) {
                caret_ = ClampToLine(text_, FirstNonBlank(text_, start));
            } else {
                const std::size_t column = goal_column_active_
                    ? goal_column_ : caret_ - starts[current_line];
                if (!goal_column_active_) {
                    goal_column_ = column;
                    goal_column_active_ = true;
                }
                goal_column_used_ = true;
                const std::size_t line_end = LineEndAt(text_, start);
                caret_ = ClampToLine(text_, std::min(start + column, line_end));
            }
        }
        clear_counts();
    };

    if (k == "h" || k == "<BS>" || k == "l" || k == " ") {
        const bool back = k == "h" || k == "<BS>";
        const bool cross = k == "<BS>" || k == " ";
        std::size_t target = caret_;
        for (std::size_t step = 0; step < count; ++step) {
            if (back) {
                if (target == 0) break;
                if (!cross && target == LineStartAt(text_, target)) break;
                target = PrevCp(text_, target);
                if (cross) target = ClampToLine(text_, target);
            } else if (cross) {
                if (target >= text_.size()) break;
                target = NextCp(text_, target);
                if (target < text_.size() && text_[target] == '\n') ++target;
            } else {
                const std::size_t line_end = LineEndAt(text_, target);
                const std::size_t next = NextCp(text_, target);
                if (next > line_end) break;
                if (!op_ && next >= line_end) break;
                target = next;
            }
        }
        ApplyMotion(target, false);
        return;
    }
    if (k == "0") {
        ApplyMotion(LineStartAt(text_, caret_), false);
        return;
    }
    if (k == "^") {
        ApplyMotion(std::min(FirstNonBlank(text_, LineStartAt(text_, caret_)),
                             text_.size()), false);
        return;
    }
    if (k == "$") {
        const std::size_t line_end = LineEndAt(text_, caret_);
        if (op_) {
            ApplyCharwise(op_, caret_, line_end);
            op_ = 0;
            clear_counts();
        } else {
            ApplyMotion(line_end > LineStartAt(text_, caret_)
                        ? PrevCp(text_, line_end) : line_end, false);
        }
        return;
    }
    if (k == "j" || k == "k") {
        const std::size_t distance = std::min<std::size_t>(count, kMaxCount);
        const std::size_t target_line = k == "j"
            ? std::min(current_line + distance, last_line)
            : current_line - std::min(distance, current_line);
        line_motion(target_line, false);
        return;
    }
    if (k == "+" || k == "-") {
        const std::size_t target_line = k == "+"
            ? std::min(current_line + count, last_line)
            : current_line - std::min(count, current_line);
        line_motion(target_line, true);
        return;
    }
    if (k == "<C-d>" || k == "<C-u>" || k == "<C-b>" || k == "<PageUp>" ||
        k == "<PageDown>") {
        const std::size_t distance =
            (k == "<C-d>" || k == "<C-u>") ? kHalfPageLines : kFullPageLines;
        const bool down = k == "<C-d>" || k == "<PageDown>";
        const std::size_t target_line = down
            ? std::min(current_line + distance, last_line)
            : current_line - std::min(distance, current_line);
        line_motion(target_line, false);
        return;
    }
    if (k == "G") {
        line_motion(HasExplicitCount() ? std::min(ExplicitCount() - 1, last_line)
                                       : last_line, true);
        return;
    }
    if (k == "g") {
        g_pending_ = true;
        return;
    }
    if (k == "w" || k == "W") {
        const bool big = k == "W";
        if (op_ == 'c' && caret_ < text_.size() && ClassOf(text_, caret_, big) != 0) {
            std::size_t target = caret_;
            for (std::size_t step = 0; step < count; ++step)
                target = WordEnd(text_, target, big);
            ApplyMotion(target, true);
            return;
        }
        std::size_t target = caret_;
        for (std::size_t step = 0; step < count; ++step)
            target = WordForward(text_, target, big);
        if (op_) {
            const std::size_t line_end = LineEndAt(text_, caret_);
            const bool empty_line = line_end == LineStartAt(text_, caret_);
            if (target > line_end && !empty_line) target = line_end;
        }
        ApplyMotion(target, false);
        return;
    }
    if (k == "e" || k == "E") {
        std::size_t target = caret_;
        for (std::size_t step = 0; step < count; ++step)
            target = WordEnd(text_, target, k == "E");
        ApplyMotion(target, true);
        return;
    }
    if (k == "b" || k == "B") {
        std::size_t target = caret_;
        for (std::size_t step = 0; step < count; ++step)
            target = WordBack(text_, target, k == "B");
        ApplyMotion(target, false);
        return;
    }
    if (k == "{" || k == "}") {
        std::size_t target = caret_;
        for (std::size_t step = 0; step < count; ++step) {
            target = k == "}" ? ParagraphForward(text_, target)
                              : ParagraphBack(text_, target);
        }
        ApplyMotion(target, false);
        return;
    }
    if (k == "%") {
        bool found = false;
        const std::size_t target = MatchBracket(text_, caret_, &found);
        if (found) ApplyMotion(target, true);
        else ClearPending();
        return;
    }
    if (k == "f" || k == "F" || k == "t" || k == "T") {
        char_wait_ = k == "f" ? CharWait::FindF
                   : k == "F" ? CharWait::FindBackF
                   : k == "t" ? CharWait::FindT
                              : CharWait::FindTillBack;
        return;
    }
    if (k == ";" || k == ",") {
        if (find_kind_ == CharWait::None || find_char_.empty()) {
            ClearPending();
            return;
        }
        CharWait kind = find_kind_;
        if (k == ",") {
            switch (kind) {
                case CharWait::FindF: kind = CharWait::FindBackF; break;
                case CharWait::FindBackF: kind = CharWait::FindF; break;
                case CharWait::FindT: kind = CharWait::FindTillBack; break;
                case CharWait::FindTillBack: kind = CharWait::FindT; break;
                default: break;
            }
        }
        FindChar(kind, find_char_, count);
        clear_counts();
        return;
    }
    if (k == "n" || k == "N") {
        for (std::size_t step = 0; step < count; ++step)
            if (!SearchMove(k == "n" ? 1 : -1)) break;
        ClearPending();
        return;
    }

    if (visual) {
        if (k == "o") {
            std::swap(anchor_, caret_);
            clear_counts();
            return;
        }
        if (k == "x" || k == "s") {
            ApplyVisualOperator(k == "x" ? 'd' : 'c');
            return;
        }
        if (k == "~" || k == "u" || k == "U") {
            ApplyVisualOperator(k[0]);
            return;
        }
        if (k == "J") {
            const auto lines = LineStarts(text_);
            const std::size_t first = LineOf(lines, std::min(anchor_, caret_));
            const std::size_t last = LineOf(lines, std::max(anchor_, caret_));
            mode_ = Mode::Normal;
            caret_ = lines[first];
            const std::size_t joins = std::max<std::size_t>(last - first, 1);
            for (std::size_t step = 0; step < joins; ++step) {
                const std::size_t line_end = LineEndAt(text_, caret_);
                if (line_end >= text_.size()) break;
                std::size_t ws_end = line_end + 1;
                while (ws_end < text_.size() && IsLineBlank(text_[ws_end])) ++ws_end;
                const bool spacer = line_end > LineStartAt(text_, caret_) &&
                    ws_end < text_.size() && text_[ws_end] != '\n';
                text_.replace(line_end, ws_end - line_end, spacer ? " " : "");
                caret_ = line_end;
            }
            caret_ = ClampToLine(text_, caret_);
            ClearPending();
            return;
        }
        if (k == "p" || k == "P") {
            const std::string paste_register = register_;
            const bool paste_linewise = register_linewise_;
            ApplyVisualOperator('d');
            register_ = paste_register;
            register_linewise_ = paste_linewise;
            if (register_linewise_) Paste(false);
            else {
                text_.insert(caret_, register_);
                caret_ = ClampToLine(text_, PrevCp(text_, caret_ + register_.size()));
            }
            clear_counts();
            return;
        }
        if (k == "r") {
            char_wait_ = CharWait::Replace;
            return;
        }
    }

    if (k == "v" || k == "V" || k == "<C-v>") {
        const Mode target = k == "v" ? Mode::VisualChar
                          : k == "V" ? Mode::VisualLine : Mode::VisualBlock;
        if (mode_ == target) {
            mode_ = Mode::Normal;
        } else {
            if (!visual) anchor_ = caret_;
            mode_ = target;
        }
        ClearPending();
        return;
    }
    if (visual) {
        if (k == ":") {
            const auto lines = LineStarts(text_);
            visual_first_line_ = LineOf(lines, std::min(anchor_, caret_));
            visual_last_line_ = LineOf(lines, std::max(anchor_, caret_));
            mode_ = Mode::Command;
            cmdline_ = ":'<,'>";
            ClearPending();
            return;
        }
        if (k == "/") {
            mode_ = Mode::Command;
            cmdline_ = "/";
            ClearPending();
            return;
        }
        ClearPending();
        return;
    }

    if (k == "x") {
        const std::size_t line_end = LineEndAt(text_, caret_);
        std::size_t end = caret_;
        for (std::size_t step = 0; step < count && end < line_end; ++step)
            end = NextCp(text_, end);
        if (end > caret_) ApplyCharwise('d', caret_, end);
        ClearPending();
        return;
    }
    if (k == "X") {
        const std::size_t line_start = LineStartAt(text_, caret_);
        std::size_t begin = caret_;
        for (std::size_t step = 0; step < count && begin > line_start; ++step)
            begin = PrevCp(text_, begin);
        if (begin < caret_) ApplyCharwise('d', begin, caret_);
        ClearPending();
        return;
    }
    if (k == "D" || k == "C") {
        ApplyCharwise(k == "D" ? 'd' : 'c', caret_, LineEndAt(text_, caret_));
        ClearPending();
        return;
    }
    if (k == "Y") {
        ApplyLinewise('y', current_line, current_line + count - 1);
        ClearPending();
        return;
    }
    if (k == "S") {
        ApplyLinewise('c', current_line, current_line + count - 1);
        ClearPending();
        return;
    }
    if (k == "s") {
        const std::size_t line_end = LineEndAt(text_, caret_);
        std::size_t end = caret_;
        for (std::size_t step = 0; step < count && end < line_end; ++step)
            end = NextCp(text_, end);
        ApplyCharwise('c', caret_, end);
        ClearPending();
        return;
    }
    if (k == "J") {
        const std::size_t joins = std::max<std::size_t>(count, 2) - 1;
        for (std::size_t step = 0; step < joins; ++step) {
            const std::size_t line_end = LineEndAt(text_, caret_);
            if (line_end >= text_.size()) break;
            std::size_t ws_end = line_end + 1;
            while (ws_end < text_.size() && IsLineBlank(text_[ws_end])) ++ws_end;
            const bool spacer = line_end > LineStartAt(text_, caret_) &&
                ws_end < text_.size() && text_[ws_end] != '\n';
            text_.replace(line_end, ws_end - line_end, spacer ? " " : "");
            caret_ = line_end;
        }
        caret_ = ClampToLine(text_, caret_);
        ClearPending();
        return;
    }
    if (k == "~") {
        const std::size_t line_end = LineEndAt(text_, caret_);
        std::size_t end = caret_;
        for (std::size_t step = 0; step < count && end < line_end; ++step)
            end = NextCp(text_, end);
        ApplyCharwise('~', caret_, end);
        caret_ = ClampToLine(text_, std::min(end, text_.size()));
        ClearPending();
        return;
    }
    if (k == "r") {
        char_wait_ = CharWait::Replace;
        return;
    }
    if (k == "p" || k == "P") {
        Paste(k == "p");
        ClearPending();
        return;
    }
    if (k == "i") {
        EnterInsert(caret_);
        ClearPending();
        return;
    }
    if (k == "I") {
        EnterInsert(FirstNonBlank(text_, LineStartAt(text_, caret_)));
        ClearPending();
        return;
    }
    if (k == "a") {
        const std::size_t line_end = LineEndAt(text_, caret_);
        EnterInsert(caret_ < line_end ? NextCp(text_, caret_) : caret_);
        ClearPending();
        return;
    }
    if (k == "A") {
        EnterInsert(LineEndAt(text_, caret_));
        ClearPending();
        return;
    }
    if (k == "o" || k == "O") {
        const std::size_t line_start = LineStartAt(text_, caret_);
        const std::string indent =
            text_.substr(line_start, FirstNonBlank(text_, line_start) - line_start);
        if (k == "o") {
            const std::size_t line_end = LineEndAt(text_, caret_);
            text_.insert(line_end, "\n" + indent);
            EnterInsert(line_end + 1 + indent.size());
        } else {
            text_.insert(line_start, indent + "\n");
            EnterInsert(line_start + indent.size());
        }
        ClearPending();
        return;
    }
    if (k == "u") {
        host_undo_ = true;
        ClearPending();
        return;
    }
    if (k == "<C-r>") {
        host_redo_ = true;
        ClearPending();
        return;
    }
    if (k == ":") {
        mode_ = Mode::Command;
        cmdline_ = ":";
        ClearPending();
        return;
    }
    if (k == "/") {
        mode_ = Mode::Command;
        cmdline_ = "/";
        ClearPending();
        return;
    }
    ClearPending();
}

}  // namespace say_count
