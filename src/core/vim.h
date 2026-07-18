#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace say_count {

struct VimState {
    std::string text;
    std::size_t caret = 0;
    std::size_t selection_start = 0;
    std::size_t selection_end = 0;
    std::size_t visual_anchor = 0;
    std::string mode = "n";
    std::string command_line;
    bool visual = false;
    bool visual_line = false;
    bool visual_block = false;
    bool host_undo = false;
    bool host_redo = false;
    bool host_save = false;
};

// Simulated Vim editing state machine. The host editor owns the text: every
// ApplyKey receives the authoritative source and caret, and the returned state
// carries the edited text back. Insert-mode typing happens in the host; only
// normal/visual/command keys flow through here. Undo and redo are delegated to
// the host via host_undo/host_redo so one history covers both kinds of edits.
class VimEmulator {
public:
    VimState ApplyKey(std::string_view source, std::size_t caret, std::string_view key);
    void Reset();

private:
    enum class Mode { Normal, Insert, VisualChar, VisualLine, VisualBlock, Command };
    enum class CharWait { None, FindF, FindT, FindBackF, FindTillBack, Replace };

    VimState MakeState() const;
    void ClearPending();
    std::size_t EffectiveCount() const;
    bool HasExplicitCount() const;
    std::size_t ExplicitCount() const;

    void HandleCommandKey(std::string_view key);
    void HandleCharWait(std::string_view key);
    void HandleKey(std::string_view key);
    void ExecuteCommandLine();
    void ExecuteSubstitute(std::size_t first_line, std::size_t last_line,
                           std::string_view body);
    bool SearchMove(int direction);

    void ApplyMotion(std::size_t target, bool inclusive);
    void ApplyLinewise(char op, std::size_t first_line, std::size_t last_line);
    void ApplyCharwise(char op, std::size_t begin, std::size_t end);
    void ApplyVisualOperator(char op);
    void EnterInsert(std::size_t caret);
    void Paste(bool after);
    void FindChar(CharWait kind, std::string_view character, std::size_t count);

    std::string text_;
    std::size_t caret_ = 0;
    Mode mode_ = Mode::Normal;
    char op_ = 0;
    std::string count_;
    std::string op_count_;
    CharWait char_wait_ = CharWait::None;
    bool g_pending_ = false;
    std::size_t anchor_ = 0;
    std::string cmdline_;
    std::string register_;
    bool register_linewise_ = false;
    std::string find_char_;
    CharWait find_kind_ = CharWait::None;
    std::string last_search_;
    std::size_t goal_column_ = 0;
    bool goal_column_active_ = false;
    bool goal_column_used_ = false;
    std::size_t visual_first_line_ = 0;
    std::size_t visual_last_line_ = 0;
    bool host_undo_ = false;
    bool host_redo_ = false;
    bool host_save_ = false;
};

}  // namespace say_count
