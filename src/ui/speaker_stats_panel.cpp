#include "ui/speaker_stats_panel.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/gauge.h>
#include <wx/choice.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/weakref.h>
#include <wx/button.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

#include "core/count_expression.h"
#include "core/version.h"

namespace say_count::ui {
namespace {

std::optional<long> TargetValue(wxTextCtrl* input) {
    const wxString text = input->GetValue().Strip(wxString::both);
    if (text.empty()) return 0;
    return evaluate_count_expression(text.ToStdString(wxConvUTF8));
}

// Narrow literals with non-ASCII bytes convert through the locale; keep them UTF-8.
wxString TargetText(std::size_t current, long target, const char* unit) {
    if (!target) return wxString::Format("No %s target", unit);
    const long remaining = target - static_cast<long>(current);
    const long percent = std::min<long>(100, std::lround(current * 100.0 / target));
    std::string text;
    if (remaining > 0) {
        text = std::to_string(remaining) + " " + unit + " left \xc2\xb7 " +
               std::to_string(percent) + "%";
    } else if (remaining == 0) {
        text = "Target reached \xc2\xb7 100%";
    } else {
        text = std::to_string(-remaining) + " " + unit + " over \xc2\xb7 " +
               std::to_string(percent) + "%";
    }
    return wxString::FromUTF8(text);
}

std::pair<wxGauge*, wxStaticText*> AddProgress(wxWindow* parent, wxBoxSizer* sizer,
                                               std::size_t current, long target,
                                               const char* unit) {
    const int percent = target ? static_cast<int>(std::min<long>(100, std::lround(current * 100.0 / target))) : 0;
    auto* gauge = new wxGauge(parent, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 8));
    gauge->SetValue(percent);
    sizer->Add(gauge, 0, wxEXPAND | wxTOP, 3);
    auto* label = new wxStaticText(parent, wxID_ANY, TargetText(current, target, unit));
    sizer->Add(label, 0, wxTOP, 2);
    return {gauge, label};
}

wxTextCtrl* TargetInput(wxWindow* parent, long value) {
    auto* input = new wxTextCtrl(parent, wxID_ANY,
                                 value > 0 ? wxString::Format("%ld", value) : wxString{},
                                 wxDefaultPosition, wxSize(96, -1), wxTE_PROCESS_ENTER);
    input->SetHint("Target or calculation");
    input->SetToolTip("Enter a target or calculation, such as 3000 / 2 or 750 * 2. "
                      "Fractional results round to the nearest whole number.");
    return input;
}

std::vector<std::string> RowStructure(const ScriptAnalysis& analysis) {
    std::vector<std::string> result;
    for (const auto& speaker : analysis.speakers) {
        const auto color = analysis.speaker_colors.find(speaker.name);
        result.push_back("speaker:" + speaker.name + ":" +
                         (color == analysis.speaker_colors.end() ? "" : color->second));
    }
    for (const auto& scene : analysis.scenes)
        if (scene.words || scene.lines) result.push_back("scene:" + scene.name);
    std::sort(result.begin(), result.end());
    return result;
}

wxString CountText(const AggregateStats& stats, int share) {
    std::string count = std::to_string(stats.words) + " words \xc2\xb7 " +
                        std::to_string(stats.lines) + " lines";
    if (share >= 0) count += " \xc2\xb7 " + std::to_string(share) + "%";
    return wxString::FromUTF8(count);
}

}  // namespace

SpeakerStatsPanel::SpeakerStatsPanel(wxWindow* parent, wxString targets_path)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxBORDER_NONE),
      targets_path_(std::move(targets_path)) {
    SetScrollRate(0, 12);
    content_ = new wxBoxSizer(wxVERTICAL);
    SetSizer(content_);
    LoadTargets();
    Rebuild();
}

void SpeakerStatsPanel::SetAnalysis(const ScriptAnalysis& analysis) {
    ScriptAnalysis updated = analysis;
    std::stable_sort(updated.speakers.begin(), updated.speakers.end(),
                     [](const auto& left, const auto& right) { return left.words > right.words; });
    const bool structure_changed = RowStructure(updated) != row_structure_;
    analysis_ = std::move(updated);
    if (structure_changed) Rebuild();
    else RefreshValues();
}

void SpeakerStatsPanel::SetLineJumpHandler(std::function<void(std::size_t)> handler) {
    line_jump_handler_ = std::move(handler);
}

std::pair<std::map<std::string, ProjectTarget>, std::map<std::string, ProjectTarget>>
SpeakerStatsPanel::ExportTargets() const {
    std::map<std::string, ProjectTarget> speakers, scenes;
    for (const auto& [name, target] : speaker_targets_) speakers[name] = {target.words, target.lines};
    for (const auto& [name, target] : scene_targets_) scenes[name] = {target.words, target.lines};
    return {std::move(speakers), std::move(scenes)};
}

ProjectTarget SpeakerStatsPanel::ExportProjectTarget() const {
    return {project_words_, project_lines_};
}

void SpeakerStatsPanel::ImportTargets(long project_words, long project_lines,
                                      const std::map<std::string, ProjectTarget>& speakers,
                                      const std::map<std::string, ProjectTarget>& scenes) {
    project_words_ = std::max(0L, project_words);
    project_lines_ = std::max(0L, project_lines);
    speaker_targets_.clear();
    scene_targets_.clear();
    for (const auto& [name, target] : speakers)
        if (target.words > 0 || target.lines > 0)
            speaker_targets_[name] = {std::max(0L, target.words), std::max(0L, target.lines)};
    for (const auto& [name, target] : scenes)
        if (target.words > 0 || target.lines > 0)
            scene_targets_[name] = {std::max(0L, target.words), std::max(0L, target.lines)};
    SaveTargets();
    Rebuild();
}

void SpeakerStatsPanel::RefreshCountedLines() {
    if (!counted_lines_) return;
    counted_lines_->Freeze();
    counted_lines_->DeleteAllItems();
    const wxString speaker = speaker_filter_->GetStringSelection();
    const wxString text = text_filter_->GetValue().Strip(wxString::both).Lower();
    const wxString label = label_filter_->GetValue().Strip(wxString::both).Lower();
    long row = 0;
    for (std::size_t index = 0; index < analysis_.counted.size(); ++index) {
        const auto& item = analysis_.counted[index];
        if (speaker_filter_->GetSelection() > 0 && wxString::FromUTF8(item.speaker) != speaker) continue;
        if (!text.empty() && !wxString::FromUTF8(item.text).Lower().Contains(text) &&
            !wxString::FromUTF8(item.raw).Lower().Contains(text)) continue;
        if (!label.empty() && !wxString::FromUTF8(item.scene).Lower().Contains(label)) continue;
        const long inserted = counted_lines_->InsertItem(
            row++, wxString::FromUTF8(std::to_string(item.line_number)));
        counted_lines_->SetItem(inserted, 1, wxString::FromUTF8(item.speaker));
        counted_lines_->SetItem(inserted, 2, wxString::FromUTF8(item.scene));
        counted_lines_->SetItem(inserted, 3, wxString::FromUTF8(item.text));
        counted_lines_->SetItemData(inserted, static_cast<long>(index));
    }
    counted_lines_->Thaw();
}

void SpeakerStatsPanel::RefreshVersionComparison() {
    if (!version_input_ || !version_result_) return;
    version_source_ = version_input_->GetValue();
    if (wxString(version_source_).Strip(wxString::both).empty()) {
        version_result_->SetValue("Paste or load an old version to compare.");
        return;
    }
    const auto comparison = compare_versions(analyze_script(version_source_.ToStdString(wxConvUTF8)), analysis_);
    auto signed_value = [](long value) { return value >= 0 ? "+" + std::to_string(value) : std::to_string(value); };
    std::string text = "Total: " + std::to_string(comparison.before_words) + " -> " +
        std::to_string(comparison.after_words) + " (" + signed_value(comparison.net_words) + ")\n" +
        "Added: " + std::to_string(comparison.added_words) + "   Removed: " +
        std::to_string(comparison.removed_words) + "   Net: " + signed_value(comparison.net_words);
    for (const auto& speaker : comparison.speakers) {
        text += "\n" + speaker.name + ": ";
        if (!speaker.delta) text += std::to_string(speaker.after) + " words (unchanged)";
        else text += std::to_string(speaker.before) + " -> " + std::to_string(speaker.after) +
                     " (" + signed_value(speaker.delta) + ")";
    }
    version_result_->SetValue(wxString::FromUTF8(text));
}

void SpeakerStatsPanel::RefreshValues() {
    auto stored = [](const std::map<std::string, Targets>& store, const std::string& name) {
        const auto found = store.find(name);
        return found == store.end() ? Targets{} : found->second;
    };
    auto refresh = [&](const std::string& key, const AggregateStats& stats,
                       Targets targets, int share) {
        const auto found = rows_.find(key);
        if (found == rows_.end()) return;
        auto& row = found->second;
        if (row.count) row.count->SetLabel(CountText(stats, share));
        if (row.balance) row.balance->SetValue(std::clamp(share, 0, 100));
        const auto update_progress = [&](wxGauge* gauge, wxStaticText* label,
                                         std::size_t current, long target, const char* unit) {
            const int percent = target ? static_cast<int>(std::min<long>(
                100, std::lround(current * 100.0 / target))) : 0;
            if (gauge) gauge->SetValue(percent);
            if (label) label->SetLabel(TargetText(current, target, unit));
        };
        update_progress(row.words_gauge, row.words_progress, stats.words, targets.words, "words");
        update_progress(row.lines_gauge, row.lines_progress, stats.lines, targets.lines, "lines");
    };

    refresh("project", {"Project", analysis_.total_words, analysis_.dialogue_lines},
            {project_words_, project_lines_}, -1);
    for (const auto& speaker : analysis_.speakers) {
        const int share = analysis_.total_words == 0 ? 0 : static_cast<int>(
            std::lround(static_cast<double>(speaker.words) * 100.0 / analysis_.total_words));
        refresh("speaker:" + speaker.name, speaker, stored(speaker_targets_, speaker.name), share);
    }
    for (const auto& scene : analysis_.scenes) {
        if (!scene.words && !scene.lines) continue;
        refresh("scene:" + scene.name, scene, stored(scene_targets_, scene.name), -1);
    }
    RefreshCountedLines();
    RefreshVersionComparison();
    Layout();
}

void SpeakerStatsPanel::Rebuild() {
    int scroll_x = 0, scroll_y = 0;
    GetViewStart(&scroll_x, &scroll_y);
    Freeze();
    const wxString selected_speaker = speaker_filter_ ? speaker_filter_->GetStringSelection() : wxString{};
    const wxString text_query = text_filter_ ? text_filter_->GetValue() : wxString{};
    const wxString label_query = label_filter_ ? label_filter_->GetValue() : wxString{};
    content_->Clear(true);
    speaker_filter_ = nullptr;
    text_filter_ = nullptr;
    label_filter_ = nullptr;
    counted_lines_ = nullptr;
    version_input_ = nullptr;
    version_result_ = nullptr;
    rows_.clear();
    auto add_heading = [&](const wxString& text) {
        auto* label = new wxStaticText(this, wxID_ANY, text);
        label->SetFont(label->GetFont().Bold());
        content_->Add(label, 0, wxLEFT | wxRIGHT | wxTOP, 12);
    };
    // color null = no swatch; share < 0 = no dialogue-balance bar (speakers only).
    auto add_row = [&](const std::string& key, const std::string& name,
                       const AggregateStats& stats, Targets targets,
                       std::map<std::string, Targets>* store, const wxColour* color, int share) {
        RowWidgets widgets;
        auto* box = new wxBoxSizer(wxVERTICAL);
        auto* name_row = new wxBoxSizer(wxHORIZONTAL);
        if (color) {
            auto* swatch = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(10, 10));
            swatch->SetBackgroundColour(*color);
            name_row->Add(swatch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        }
        name_row->Add(new wxStaticText(this, wxID_ANY, wxString::FromUTF8(name)), 0,
                      wxALIGN_CENTER_VERTICAL);
        box->Add(name_row, 0, wxBOTTOM, 2);
        widgets.count = new wxStaticText(this, wxID_ANY, CountText(stats, share));
        box->Add(widgets.count, 0, wxBOTTOM, 4);
        if (share >= 0) {
            widgets.balance = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 8));
            widgets.balance->SetValue(share);
            box->Add(widgets.balance, 0, wxEXPAND | wxBOTTOM, 4);
        }
        auto* inputs = new wxBoxSizer(wxHORIZONTAL);
        inputs->Add(new wxStaticText(this, wxID_ANY, "Words"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        auto* words = TargetInput(this, targets.words);
        inputs->Add(words, 0, wxRIGHT, 10);
        inputs->Add(new wxStaticText(this, wxID_ANY, "Lines"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        auto* lines = TargetInput(this, targets.lines);
        inputs->Add(lines);
        box->Add(inputs, 0, wxBOTTOM, 2);
        const auto words_progress = AddProgress(this, box, stats.words, targets.words, "words");
        widgets.words_gauge = words_progress.first;
        widgets.words_progress = words_progress.second;
        const auto lines_progress = AddProgress(this, box, stats.lines, targets.lines, "lines");
        widgets.lines_gauge = lines_progress.first;
        widgets.lines_progress = lines_progress.second;
        // Weak refs: Rebuild() destroys the inputs, which can fire KILL_FOCUS mid-destruction.
        wxWeakRef<wxTextCtrl> words_ref(words);
        wxWeakRef<wxTextCtrl> lines_ref(lines);
        auto save = [this, name, store, words_ref, lines_ref](wxCommandEvent&) {
            if (!words_ref || !lines_ref) return;
            Targets previous{project_words_, project_lines_};
            if (store) {
                const auto found = store->find(name);
                previous = found == store->end() ? Targets{} : found->second;
            }
            const auto words_value = TargetValue(words_ref);
            const auto lines_value = TargetValue(lines_ref);
            if (!words_value && !lines_value) return;
            const Targets value{words_value.value_or(previous.words),
                                lines_value.value_or(previous.lines)};
            if (words_value) {
                words_ref->ChangeValue(*words_value > 0 ? wxString::Format("%ld", *words_value)
                                                        : wxString{});
            }
            if (lines_value) {
                lines_ref->ChangeValue(*lines_value > 0 ? wxString::Format("%ld", *lines_value)
                                                        : wxString{});
            }
            if (value.words == previous.words && value.lines == previous.lines) return;
            if (!store) {
                project_words_ = value.words;
                project_lines_ = value.lines;
            } else if (value.words || value.lines) (*store)[name] = value;
            else store->erase(name);
            SaveTargets();
            CallAfter([this] { Rebuild(); });
        };
        words->Bind(wxEVT_TEXT_ENTER, save);
        lines->Bind(wxEVT_TEXT_ENTER, save);
        words->Bind(wxEVT_KILL_FOCUS, [save](wxFocusEvent& event) mutable {
            wxCommandEvent command; save(command); event.Skip();
        });
        lines->Bind(wxEVT_KILL_FOCUS, [save](wxFocusEvent& event) mutable {
            wxCommandEvent command; save(command); event.Skip();
        });
        content_->Add(box, 0, wxEXPAND | wxALL, 12);
        rows_[key] = widgets;
    };
    auto stored = [](const std::map<std::string, Targets>& store, const std::string& name) {
        const auto found = store.find(name);
        return found == store.end() ? Targets{} : found->second;
    };

    add_heading("Project targets");
    add_row("project", "Project", {"Project", analysis_.total_words, analysis_.dialogue_lines},
            {project_words_, project_lines_}, nullptr, nullptr, -1);

    add_heading("Characters");
    if (analysis_.speakers.empty()) content_->Add(new wxStaticText(this, wxID_ANY, "No dialogue yet."), 0, wxALL, 12);
    for (const auto& speaker : analysis_.speakers) {
        wxColour color("#77d6c3");
        const auto found = analysis_.speaker_colors.find(speaker.name);
        if (found != analysis_.speaker_colors.end()) {
            const wxColour parsed(wxString::FromUTF8(found->second));
            if (parsed.IsOk()) color = parsed;
        }
        const int share = analysis_.total_words == 0 ? 0 : static_cast<int>(
            std::lround(static_cast<double>(speaker.words) * 100.0 / analysis_.total_words));
        add_row("speaker:" + speaker.name, speaker.name, speaker,
                stored(speaker_targets_, speaker.name),
                &speaker_targets_, &color, share);
    }

    add_heading("Scenes / labels");
    bool any_scene = false;
    for (const auto& scene : analysis_.scenes) {
        if (!scene.words && !scene.lines) continue;
        any_scene = true;
        add_row("scene:" + scene.name, scene.name, scene,
                stored(scene_targets_, scene.name), &scene_targets_,
                nullptr, -1);
    }
    if (!any_scene) content_->Add(new wxStaticText(this, wxID_ANY, "No labels with dialogue yet."), 0, wxALL, 12);

    add_heading("Counted lines");
    auto* filters = new wxBoxSizer(wxVERTICAL);
    wxArrayString speakers;
    speakers.Add("All speakers");
    std::vector<std::string> names;
    for (const auto& speaker : analysis_.speakers) names.push_back(speaker.name);
    std::sort(names.begin(), names.end());
    for (const auto& name : names) speakers.Add(wxString::FromUTF8(name));
    speaker_filter_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, speakers);
    speaker_filter_->SetSelection(0);
    if (!selected_speaker.empty()) {
        const int selection = speaker_filter_->FindString(selected_speaker);
        if (selection != wxNOT_FOUND) speaker_filter_->SetSelection(selection);
    }
    text_filter_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize);
    text_filter_->SetHint("Filter text");
    text_filter_->SetValue(text_query);
    label_filter_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize);
    label_filter_->SetHint("Filter label");
    label_filter_->SetValue(label_query);
    filters->Add(speaker_filter_, 0, wxEXPAND | wxBOTTOM, 4);
    filters->Add(text_filter_, 0, wxEXPAND | wxBOTTOM, 4);
    filters->Add(label_filter_, 0, wxEXPAND);
    content_->Add(filters, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);
    counted_lines_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 240),
                                    wxLC_REPORT | wxLC_SINGLE_SEL);
    counted_lines_->AppendColumn("Line", wxLIST_FORMAT_LEFT, 52);
    counted_lines_->AppendColumn("Speaker", wxLIST_FORMAT_LEFT, 90);
    counted_lines_->AppendColumn("Label", wxLIST_FORMAT_LEFT, 100);
    counted_lines_->AppendColumn("Text", wxLIST_FORMAT_LEFT, 260);
    content_->Add(counted_lines_, 0, wxEXPAND | wxALL, 12);
    speaker_filter_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { RefreshCountedLines(); });
    text_filter_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshCountedLines(); });
    label_filter_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshCountedLines(); });
    counted_lines_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event) {
        const auto index = static_cast<std::size_t>(event.GetData());
        if (index < analysis_.counted.size() && line_jump_handler_)
            line_jump_handler_(analysis_.counted[index].line_number);
    });
    RefreshCountedLines();
    add_heading("Compare versions");
    version_input_ = new wxTextCtrl(this, wxID_ANY, version_source_, wxDefaultPosition,
                                    wxSize(-1, 120), wxTE_MULTILINE);
    version_input_->SetHint("Paste an older script here");
    auto* load_version = new wxButton(this, wxID_ANY, "Load old script...");
    version_result_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxSize(-1, 150),
                                    wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    content_->Add(version_input_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);
    content_->Add(load_version, 0, wxLEFT | wxRIGHT | wxTOP, 12);
    content_->Add(version_result_, 0, wxEXPAND | wxALL, 12);
    version_input_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshVersionComparison(); });
    load_version->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxFileDialog dialog(this, "Load older script", wxEmptyString, wxEmptyString,
                            "Ren'Py scripts (*.rpy)|*.rpy", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() != wxID_OK) return;
        wxFile file(dialog.GetPath()); wxString contents;
        if (file.IsOpened() && file.ReadAll(&contents, wxConvUTF8)) version_input_->SetValue(contents);
        else wxMessageBox("Could not read the selected script.", "Version comparison",
                          wxOK | wxICON_ERROR, this);
    });
    RefreshVersionComparison();
    Layout();
    FitInside();
    row_structure_ = RowStructure(analysis_);
    Scroll(scroll_x, scroll_y);
    Thaw();
}

void SpeakerStatsPanel::LoadTargets() {
    wxFileConfig config("say-count", wxEmptyString, targets_path_);
    config.Read("project/words", &project_words_, 0L);
    config.Read("project/lines", &project_lines_, 0L);
    auto load = [&](const wxString& root, std::map<std::string, Targets>& target) {
        config.SetPath(root);
        long index = 0;
        wxString key;
        bool more = config.GetFirstGroup(key, index);
        while (more) {
            Targets values;
            config.Read(key + "/words", &values.words, 0L);
            config.Read(key + "/lines", &values.lines, 0L);
            if (values.words || values.lines) target[key.ToStdString(wxConvUTF8)] = values;
            more = config.GetNextGroup(key, index);
        }
        config.SetPath("/");
    };
    load("/speakers", speaker_targets_);
    load("/scenes", scene_targets_);
}

void SpeakerStatsPanel::SaveTargets() const {
    const wxString directory = wxFileName(targets_path_).GetPath();
    if (!wxFileName::DirExists(directory)) {
        wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    }
    wxFileConfig config("say-count", wxEmptyString, targets_path_);
    config.DeleteAll();
    config.Write("project/words", project_words_);
    config.Write("project/lines", project_lines_);
    auto save = [&](const wxString& root, const std::map<std::string, Targets>& values) {
        for (const auto& [key, target] : values) {
            config.Write(root + "/" + wxString::FromUTF8(key) + "/words", target.words);
            config.Write(root + "/" + wxString::FromUTF8(key) + "/lines", target.lines);
        }
    };
    save("speakers", speaker_targets_);
    save("scenes", scene_targets_);
    config.Flush();
}

}  // namespace say_count::ui
