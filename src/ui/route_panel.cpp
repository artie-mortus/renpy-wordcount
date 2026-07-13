#include "ui/route_panel.h"

#include <utility>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/treectrl.h>

#include "ui/flow_map_panel.h"

namespace say_count::ui {
namespace {

class RouteItemData final : public wxTreeItemData {
public:
    RouteItemData(std::string file_name, std::size_t line_number)
        : file(std::move(file_name)), line(line_number) {}

    std::string file;
    std::size_t line = 0;
};

std::string Minutes(std::size_t minutes) {
    return minutes == 0 ? "0 min" : "~" + std::to_string(minutes) + " min";
}

std::string PathName(std::size_t index, const RoutePath& path) {
    return "Path " + std::to_string(index + 1) + " — " + std::to_string(path.words) +
           " words · " + Minutes(path.reading_minutes) +
           (path.loop_target ? " · loop" : "");
}

}  // namespace

RoutePanel::RoutePanel(wxWindow* parent) : wxPanel(parent) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    summary_ = new wxStaticText(this, wxID_ANY, "No labels yet.");
    flow_map_ = new FlowMapPanel(this);
    details_ = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                              wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxTR_SINGLE | wxBORDER_NONE);
    layout->Add(summary_, 0, wxEXPAND | wxALL, 10);
    layout->Add(flow_map_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
    layout->Add(details_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
    SetSizer(layout);
    details_->Bind(wxEVT_TREE_ITEM_ACTIVATED, &RoutePanel::OnActivated, this);
    Rebuild();
}

void RoutePanel::SetReport(std::optional<RouteReport> report) {
    report_ = std::move(report);
    flow_map_->SetReport(report_);
    Rebuild();
}

void RoutePanel::SetJumpHandler(JumpHandler handler) {
    jump_handler_ = std::move(handler);
}

void RoutePanel::Rebuild() {
    Freeze();
    details_->DeleteAllItems();
    const wxTreeItemId root = details_->AddRoot("Routes");
    if (!report_ || !report_->graph.start) {
        summary_->SetLabel("No labels yet.");
        Layout();
        Thaw();
        return;
    }

    const auto& report = *report_;
    std::string summary;
    if (report.extremes.min_words == report.extremes.max_words) {
        summary = "Route length: " + std::to_string(report.extremes.min_words) + " words · " +
                  Minutes(report.shortest_reading_minutes);
    } else {
        summary = "Shortest: " + std::to_string(report.extremes.min_words) + " words · " +
                  Minutes(report.shortest_reading_minutes) + "\nLongest: " +
                  std::to_string(report.extremes.max_words) + " words · " +
                  Minutes(report.longest_reading_minutes);
    }
    summary += "\nEndings: " + std::to_string(report.endings) +
               " · Branch points: " + std::to_string(report.branches) +
               "\nMenu choices: " + std::to_string(report.menu_branches) +
               " · Conditional paths: " + std::to_string(report.condition_branches);
    summary_->SetLabel(wxString::FromUTF8(summary));

    const auto add_location = [&](const wxTreeItemId& parent, const std::string& text,
                                  const std::string& label, std::size_t line = 0) {
        const auto found = report.graph.nodes.find(label);
        if (found == report.graph.nodes.end()) return wxTreeItemId{};
        const std::size_t target_line = line ? line : found->second.line;
        return details_->AppendItem(parent, wxString::FromUTF8(text), -1, -1,
                                    new RouteItemData(found->second.file, target_line));
    };

    const wxTreeItemId paths = details_->AppendItem(
        root, wxString::FromUTF8("Paths (" + std::to_string(report.paths.size()) + ")"));
    for (std::size_t index = 0; index < report.paths.size(); ++index) {
        const auto& path = report.paths[index];
        const wxTreeItemId path_item = details_->AppendItem(paths, wxString::FromUTF8(PathName(index, path)));
        for (const auto& label : path.labels) add_location(path_item, label, label);
        if (path.loop_target) {
            const auto loop = add_location(path_item, "Loops back to " + *path.loop_target,
                                           *path.loop_target);
            if (loop.IsOk()) details_->SetItemTextColour(loop, wxColour("#b3261e"));
        }
        if (index == 0) details_->Expand(path_item);
    }
    if (report.paths_truncated)
        details_->AppendItem(paths, "More paths omitted to keep the panel responsive.");
    details_->Expand(paths);

    std::size_t branch_count = 0;
    for (const auto& [label, branches] : report.branch_totals) branch_count += branches.size();
    if (branch_count) {
        const wxTreeItemId branches = details_->AppendItem(
            root, wxString::FromUTF8("Inline branches (" + std::to_string(branch_count) + ")"));
        for (const auto& label : report.graph.node_order) {
            const auto found = report.branch_totals.find(label);
            if (found == report.branch_totals.end()) continue;
            for (const auto& branch : found->second) {
                const std::string kind = branch.kind == RouteBranchKind::choice
                    ? "choice \xe2\x80\x9c" + branch.text + "\xe2\x80\x9d" : branch.text;
                add_location(branches, label + " · " + kind + " — " +
                             std::to_string(branch.words) + " branch words", label, branch.line);
            }
        }
        details_->Expand(branches);
    }

    const bool has_issues = *report.graph.start != "start" || report.loops ||
                            report.menu_branches || report.condition_branches ||
                            !report.unreachable.empty();
    if (has_issues) {
        const wxTreeItemId issues = details_->AppendItem(root, "Route notes");
        if (*report.graph.start != "start") {
            details_->AppendItem(issues, wxString::FromUTF8(
                "No \"start\" label; walking from \"" + *report.graph.start + "\"."));
        }
        if (report.loops)
            details_->AppendItem(issues, "Loop detected; route totals are approximate.");
        if (report.menu_branches || report.condition_branches) {
            details_->AppendItem(issues,
                "Inline branches are itemized above; whole-route totals remain estimates.");
        }
        for (const auto& label : report.unreachable) {
            const auto item = add_location(issues, "Unreachable label \"" + label + "\"", label);
            if (item.IsOk()) details_->SetItemTextColour(item, wxColour("#b3261e"));
        }
        details_->Expand(issues);
    }

    Layout();
    Thaw();
}

void RoutePanel::OnActivated(wxTreeEvent& event) {
    const auto* data = dynamic_cast<const RouteItemData*>(details_->GetItemData(event.GetItem()));
    if (data && jump_handler_) jump_handler_(data->file, data->line);
}

}  // namespace say_count::ui
