#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <wx/defs.h>

namespace say_count::ui {

enum MenuId {
    kToggleWrap = wxID_HIGHEST + 1,
    kToggleNvimMotions,
    kFontIncrease,
    kFontDecrease,
    kFontReset,
    kThemeSystem,
    kThemeLight,
    kThemeDark,
    kExportCsv,
    kExportJson,
    kExportHtml,
    kFindNext,
    kFindPrevious,
    kFindReplace,
    kFindReplaceAll,
    kFindClose,
    kGoToLine,
    kToggleComment,
    kInsertStoryElement,
    kWriterDraft,
    kWriteManuscript,
    kConvertToChat,
    kConvertChatToDialogue,
    kInstallChatRuntime,
    kConfigureOfflineProseAi,
    kShortcutSheet,
    kManuscriptGuide,
    kChatGuide,
    kFocusMode,
    kConnectProject,
    kShowHome,
    kRecentProjectFirst,
    kRecentProjectLast = kRecentProjectFirst + 7,
    kReviewConflicts = kRecentProjectLast + 1,
    kSnapshotNow,
    kManageSnapshots,
    kImportProject,
    kExportProject,
    kGitRepository,
    kRenameSymbol,
    kConfigureRenpy,
    kRenpyStatus,
    kRunRenpy,
    kStopRenpy,
    kShowRenpyLog,
    kWarpRenpy,
    kDirectorRenpy,
    kRuntimePresets,
    kRunRenpyLint,
    kGenerateTranslations,
    kExportDialogue,
    kShowAssets,
    kShowCoverage,
    kShowRoutes,
    kShowProduction,
    kFixIndents,
    kShowOutline,
    kShowProjectNavigator,
    kShowSpeakerStats,
    kShowStoryLibrary,
    kShowDiagnostics,
    kQuickOpen,
    kCommandPalette,
};

struct CommandSpec {
    int id = wxID_NONE;
    const char* label = "";
    const char* context = "";
    const char* help = "";
};

const std::vector<CommandSpec>& CommandCatalog();
const CommandSpec& CommandFor(int id);

struct ShellContext {
    bool has_game = false;
    bool document_dirty = false;
    bool document_has_path = false;
    std::size_t problem_count = 0;
    std::size_t fixable_problem_count = 0;
    bool renpy_available = false;
    bool renpy_running = false;
};

struct CommandBarState {
    bool show_open_game = true;
    bool show_history = false;
    bool show_write = true;
    bool show_problems = false;
    bool show_preview = false;
    bool enable_save = true;
    bool enable_preview = false;
    std::string problem_label;
};

CommandBarState DeriveCommandBarState(const ShellContext& context);

}  // namespace say_count::ui
