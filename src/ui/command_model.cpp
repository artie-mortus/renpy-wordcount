#include "ui/command_model.h"

#include <algorithm>

namespace say_count::ui {

const std::vector<CommandSpec>& CommandCatalog() {
    static const std::vector<CommandSpec> commands{
        {wxID_NEW, "New script", "Ctrl+N · File", "Create a new script tab"},
        {wxID_OPEN, "Open scripts", "Ctrl+O · File", "Open one or more Ren'Py scripts"},
        {kShowHome, "Home", "File", "Start a story, open a game, or return to recent work"},
        {kQuickOpen, "Quick Open", "Ctrl+P · Navigation", "Find a project file, label, or character"},
        {wxID_SAVE, "Save", "Ctrl+S · File", "Save the current script"},
        {wxID_SAVEAS, "Save As", "Ctrl+Shift+S · File", "Save the current script under a new name"},
        {wxID_CLOSE, "Close tab", "Ctrl+W · File", "Close the current tab"},
        {kConnectProject, "Open a game...", "Game", "Open an existing Ren'Py game folder"},
        {kSnapshotNow, "Save a version now", "Local history · Game", "Store a version of every open script"},
        {kManageSnapshots, "Version history...", "Local history · Game", "Preview and restore earlier local versions"},
        {kReviewConflicts, "Review external conflicts", "Project", "Review files changed outside Say Count"},
        {kGitRepository, "Git repository", "Clone, sync, commit, and push · Project", "Work with the game's Git repository"},
        {kImportProject, "Import Say Count project", "File", "Import a browser-app project bundle"},
        {kExportProject, "Export complete project", "File", "Export a complete Say Count project"},
        {kExportCsv, "Export speaker statistics", "CSV · File", "Export speaker statistics as CSV"},
        {kExportJson, "Export full statistics", "JSON · File", "Export all statistics as JSON"},
        {kExportHtml, "Export standalone report", "HTML · File", "Export a standalone HTML report"},
        {wxID_FIND, "Find and replace", "Ctrl+F · Edit", "Find and replace text"},
        {kGoToLine, "Go to line", "Ctrl+G · Navigation", "Move the cursor to a line"},
        {kToggleComment, "Toggle comment", "Ctrl+/ · Edit", "Comment or uncomment the selected lines"},
        {kInsertStoryElement, "Insert into story", "Character, dialogue, choice, scene, audio, or jump · Writing", "Build and preview a story element"},
        {kShowStoryLibrary, "Story Library", "Cast, places, images, music, and sounds · Writing", "Open the project's cast and media library"},
        {kWriterDraft, "Writing draft...", "Persistent natural-writing source · Writing", "Write naturally with a generated-script preview"},
        {kToggleNvimMotions, "Toggle built-in Vim motions", "Normal/insert modes · Edit", "Use built-in modal editing"},
        {kWriteManuscript, "Make game script from writing...", "Writing", "Review and turn natural writing into Ren'Py script"},
        {kConvertToChat, "Turn writing into a chat scene", "Edit", "Convert writing into a messenger scene"},
        {kConvertChatToDialogue, "Turn chat scene back into dialogue", "Edit", "Convert a messenger scene back into dialogue"},
        {kInstallChatRuntime, "Install/update chat app files", "Edit", "Install the chat runtime into the game"},
        {kConfigureOfflineProseAi, "Configure offline prose AI", "Edit", "Configure an optional network-blocked local prose model"},
        {kFixIndents, "Fix indents", "Edit", "Preview and repair indentation"},
        {kRenameSymbol, "Rename Ren'Py symbol", "Project", "Safely rename an alias or label project-wide"},
        {kToggleWrap, "Toggle word wrap", "View", "Soft-wrap long lines"},
        {kFocusMode, "Toggle focus mode", "Ctrl+Shift+F · View", "Hide nonessential panels"},
        {kShowOutline, "Toggle outline", "View", "Show or hide the script outline"},
        {kShowProjectNavigator, "Toggle script index", "View", "Show or hide the project's script index"},
        {kShowSpeakerStats, "Toggle speaker statistics", "View", "Show or hide speaker statistics"},
        {kShowDiagnostics, "Problems", "View", "Show problems found in the writing"},
        {kShowRoutes, "Route details", "View", "Show route summaries and paths"},
        {kShowProduction, "Production Desk", "View", "Show prose and production tools"},
        {kFontIncrease, "Increase editor font", "Ctrl+= · View", "Increase the editor font size"},
        {kFontDecrease, "Decrease editor font", "Ctrl+- · View", "Decrease the editor font size"},
        {kFontReset, "Reset editor font", "Ctrl+0 · View", "Reset the editor font size"},
        {kThemeSystem, "Use system theme", "View", "Follow the system theme"},
        {kThemeLight, "Use light theme", "View", "Use the light editor theme"},
        {kThemeDark, "Use dark theme", "View", "Use the dark editor theme"},
        {kRunRenpy, "Run project", "F6 · Ren'Py", "Preview the game"},
        {kWarpRenpy, "Run from caret", "F7 · Ren'Py", "Preview the game from the current line"},
        {kDirectorRenpy, "Interactive Director", "Ren'Py", "Launch Ren'Py's Interactive Director"},
        {kRuntimePresets, "Runtime state presets", "Ren'Py", "Choose variables for the next preview"},
        {kRunRenpyLint, "Check game for problems", "Ren'Py", "Run Ren'Py's complete project check"},
        {kGenerateTranslations, "Generate translations", "Ren'Py", "Generate Ren'Py translation files"},
        {kExportDialogue, "Export dialogue", "Ren'Py", "Export dialogue through Ren'Py"},
        {kShowAssets, "Story Library", "Ren'Py", "Browse cast, images, music, and sounds"},
        {kShowCoverage, "Label coverage", "Ren'Py", "Review playthrough label coverage"},
        {kStopRenpy, "Stop running project", "Shift+F6 · Ren'Py", "Stop the running preview"},
        {kShowRenpyLog, "Show launch log", "Ren'Py", "Show the Ren'Py launch log"},
        {kConfigureRenpy, "Configure Ren'Py SDK", "Ren'Py", "Choose the Ren'Py SDK executable"},
        {kManuscriptGuide, "Prose writing guide", "Help", "Learn the supported manuscript formats"},
        {kChatGuide, "Chat format guide", "Help", "Learn how to write messenger scenes"},
        {kShortcutSheet, "Keyboard shortcuts", "Ctrl+K · Help", "Show keyboard shortcuts"},
        {wxID_ABOUT, "About Say Count", "Help", "About Say Count"},
    };
    return commands;
}

const CommandSpec& CommandFor(int id) {
    const auto& commands = CommandCatalog();
    const auto found = std::find_if(commands.begin(), commands.end(), [id](const auto& command) {
        return command.id == id;
    });
    static const CommandSpec missing{};
    return found == commands.end() ? missing : *found;
}

CommandBarState DeriveCommandBarState(const ShellContext& context) {
    CommandBarState result;
    result.show_open_game = !context.has_game;
    result.show_history = context.has_game || context.document_has_path;
    result.show_write = true;
    result.show_problems = context.problem_count > 0;
    result.show_preview = context.has_game;
    result.enable_save = context.document_dirty || !context.document_has_path;
    result.enable_preview = context.has_game && context.renpy_available && !context.renpy_running;
    if (context.fixable_problem_count > 0) {
        result.problem_label = "Fix " + std::to_string(context.fixable_problem_count);
    } else if (context.problem_count == 1) {
        result.problem_label = "1 problem";
    } else if (context.problem_count > 1) {
        result.problem_label = std::to_string(context.problem_count) + " problems";
    }
    return result;
}

}  // namespace say_count::ui
