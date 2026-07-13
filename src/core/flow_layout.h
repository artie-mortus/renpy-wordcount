#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/routes.h"

namespace say_count {

inline constexpr double kFlowNodeWidth = 104.0;
inline constexpr double kFlowNodeHeight = 32.0;
inline constexpr double kFlowHorizontalGap = 14.0;
inline constexpr double kFlowVerticalGap = 30.0;
inline constexpr std::size_t kFlowMaxNodes = 80;

struct FlowRect {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;

    bool Contains(double point_x, double point_y) const noexcept {
        return point_x >= x && point_x <= x + width &&
               point_y >= y && point_y <= y + height;
    }
};

struct FlowNodeLayout {
    std::string name;
    std::string file;
    std::size_t line = 0;
    std::size_t words = 0;
    FlowRect bounds;
    bool start = false;
    bool ending = false;
    bool unreachable = false;
};

struct FlowEdgeLayout {
    std::string from;
    std::string to;
    bool back_edge = false;
};

struct FlowLayout {
    double width = 0;
    double height = 0;
    std::vector<FlowNodeLayout> nodes;
    std::vector<FlowEdgeLayout> edges;
    bool too_many_nodes = false;
    std::size_t requested_nodes = 0;
};

FlowLayout build_flow_layout(const RouteReport& report,
                             std::size_t maximum_nodes = kFlowMaxNodes);

}  // namespace say_count
