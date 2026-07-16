#include "core/autocomplete.h"
#include "core/diagnostics.h"
#include "core/find_replace.h"
#include "core/navigator.h"
#include "core/parser.h"
#include "core/routes.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

struct Measurement { const char* name; double milliseconds; double limit; };

template <typename Work>
Measurement measure(const char* name, double limit, Work work) {
    std::cerr << "Running " << name << "...\n";
    const auto start = Clock::now();
    work();
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    std::cerr << "Completed " << name << " in " << elapsed << " ms\n";
    return {name, elapsed, limit};
}

std::vector<say_count::NamedScript> large_project() {
    std::vector<say_count::NamedScript> scripts;
    scripts.reserve(500);
    for (int file = 0; file < 500; ++file) {
        std::string source = "define speaker_" + std::to_string(file) +
            " = Character(\"Speaker " + std::to_string(file) + "\")\n";
        for (int scene = 0; scene < 10; ++scene) {
            source += "label scene_" + std::to_string(file) + "_" + std::to_string(scene) + ":\n";
            source += "    speaker_" + std::to_string(file) +
                " \"A representative dialogue line for repeatable project performance measurement.\"\n";
            source += scene == 9 ? "    return\n" :
                "    jump scene_" + std::to_string(file) + "_" + std::to_string(scene + 1) + "\n";
        }
        scripts.push_back({"chapter_" + std::to_string(file) + ".rpy", std::move(source)});
    }
    return scripts;
}
}  // namespace

int main(int argc, char** argv) {
    const bool check = argc > 1 && std::string(argv[1]) == "--check";
    const auto scripts = large_project();
    std::vector<say_count::CompletionIndex> cached_completion_indexes;
    cached_completion_indexes.reserve(scripts.size());
    for (const auto& script : scripts)
        cached_completion_indexes.push_back(say_count::build_completion_index(script));
    std::string large_document;
    while (large_document.size() < 2 * 1024 * 1024)
        large_document += "label repeated_scene:\n    narrator \"A searchable line of dialogue for the editor benchmark.\"\n";

    std::vector<Measurement> results;
    say_count::ScriptAnalysis analysis;
    results.push_back(measure("analyze-500-file-project", 3000, [&] { analysis = say_count::analyze_project(scripts); }));
    results.push_back(measure("diagnose-500-file-project", 3000, [&] {
        const auto value = say_count::diagnose_project(scripts); (void)value;
    }));
    results.push_back(measure("completion-index-500-files", 4000, [&] {
        const auto value = say_count::build_completion_index(scripts); (void)value;
    }));
    results.push_back(measure("incremental-completion-update", 100, [&] {
        cached_completion_indexes[250] = say_count::build_completion_index(scripts[250]);
        const auto value = say_count::merge_completion_indexes(cached_completion_indexes); (void)value;
    }));
    results.push_back(measure("navigation-index-50-files", 3000, [&] {
        const std::vector<say_count::NamedScript> navigation_scripts(scripts.begin(), scripts.begin() + 50);
        const auto value = say_count::build_navigation_index(navigation_scripts); (void)value;
    }));
    results.push_back(measure("routes-25-files", 1500, [&] {
        const std::vector<say_count::NamedScript> route_scripts(scripts.begin(), scripts.begin() + 25);
        const auto route_analysis = say_count::analyze_project(route_scripts);
        const auto value = say_count::compute_routes(route_analysis, route_scripts); (void)value;
    }));
    results.push_back(measure("find-in-2mb-document", 500, [&] {
        const auto value = say_count::find_matches(large_document, "searchable"); (void)value;
    }));

    bool passed = true;
    std::cout << std::fixed << std::setprecision(1);
    for (const auto& result : results) {
        const bool within = result.milliseconds <= result.limit;
        passed = passed && within;
        std::cout << result.name << ": " << result.milliseconds << " ms"
                  << " (limit " << result.limit << " ms)" << (within ? "" : " EXCEEDED") << '\n';
    }
    return check && !passed ? EXIT_FAILURE : EXIT_SUCCESS;
}
