#include "core/flow_layout.h"

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace say_count {
namespace {

std::vector<std::string> Targets(const RouteNode& node, const RouteGraph& graph) {
    std::vector<std::string> targets;
    std::unordered_set<std::string> seen;
    for (const auto& edge : node.edges) {
        if (graph.nodes.count(edge.target) && seen.insert(edge.target).second)
            targets.push_back(edge.target);
    }
    return targets;
}

}  // namespace

FlowLayout build_flow_layout(const RouteReport& report, std::size_t maximum_nodes) {
    FlowLayout layout;
    layout.requested_nodes = report.graph.nodes.size();
    if (!report.graph.start || report.graph.nodes.empty()) return layout;
    if (report.graph.nodes.size() > maximum_nodes) {
        layout.too_many_nodes = true;
        return layout;
    }

    std::unordered_map<std::string, std::size_t> layer_of;
    std::vector<std::vector<std::string>> layers;
    std::deque<std::string> queue;
    layer_of[*report.graph.start] = 0;
    queue.push_back(*report.graph.start);
    while (!queue.empty()) {
        const std::string name = std::move(queue.front());
        queue.pop_front();
        const std::size_t layer = layer_of.at(name);
        if (layers.size() <= layer) layers.resize(layer + 1);
        layers[layer].push_back(name);
        for (const auto& target : Targets(report.graph.nodes.at(name), report.graph)) {
            if (layer_of.emplace(target, layer + 1).second) queue.push_back(target);
        }
    }

    const std::size_t unreachable_base = layers.size();
    std::size_t unreachable_index = 0;
    for (const auto& name : report.graph.node_order) {
        if (layer_of.count(name)) continue;
        const std::size_t layer = unreachable_base + unreachable_index / 4;
        if (layers.size() <= layer) layers.resize(layer + 1);
        layers[layer].push_back(name);
        layer_of[name] = layer;
        ++unreachable_index;
    }

    std::size_t widest = 0;
    for (const auto& layer : layers) widest = std::max(widest, layer.size());
    layout.width = std::max(widest * (kFlowNodeWidth + kFlowHorizontalGap) +
                                kFlowHorizontalGap, 240.0);
    layout.height = layers.size() * (kFlowNodeHeight + kFlowVerticalGap) +
                    kFlowVerticalGap / 2.0;

    std::unordered_set<std::string> unreachable(report.unreachable.begin(), report.unreachable.end());
    std::unordered_map<std::string, std::pair<double, double>> centers;
    for (std::size_t layer_index = 0; layer_index < layers.size(); ++layer_index) {
        const auto& layer = layers[layer_index];
        const double row_width = layer.size() * kFlowNodeWidth +
                                 (layer.size() - 1) * kFlowHorizontalGap;
        for (std::size_t index = 0; index < layer.size(); ++index) {
            const std::string& name = layer[index];
            const double center_x = (layout.width - row_width) / 2.0 +
                                    index * (kFlowNodeWidth + kFlowHorizontalGap) +
                                    kFlowNodeWidth / 2.0;
            const double center_y = kFlowVerticalGap / 2.0 +
                                    layer_index * (kFlowNodeHeight + kFlowVerticalGap) +
                                    kFlowNodeHeight / 2.0;
            centers[name] = {center_x, center_y};
            const auto& node = report.graph.nodes.at(name);
            const auto word_count = report.label_words.find(name);
            layout.nodes.push_back({
                name, node.file, node.line,
                word_count == report.label_words.end() ? 0 : word_count->second,
                {center_x - kFlowNodeWidth / 2.0, center_y - kFlowNodeHeight / 2.0,
                 kFlowNodeWidth, kFlowNodeHeight},
                name == *report.graph.start, Targets(node, report.graph).empty(),
                unreachable.count(name) != 0,
            });
        }
    }

    for (const auto& name : report.graph.node_order) {
        const auto from = centers.find(name);
        if (from == centers.end()) continue;
        for (const auto& target : Targets(report.graph.nodes.at(name), report.graph)) {
            const auto to = centers.find(target);
            if (to == centers.end()) continue;
            layout.edges.push_back({name, target, to->second.second <= from->second.second});
        }
    }
    return layout;
}

}  // namespace say_count
