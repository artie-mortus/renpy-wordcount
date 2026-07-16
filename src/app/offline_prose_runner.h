#pragma once

#include "core/offline_prose_ai.h"

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace say_count::app {

struct OfflineProseRunResult {
    OfflineProseRewrite rewrite;
    std::string error;
    bool cancelled = false;
};

// Runs llama.cpp's CLI inside a bubblewrap network namespace with no network
// interfaces. The callback returns false to cancel the local process group.
OfflineProseRunResult run_offline_prose_model(
    std::string_view manuscript,
    const std::map<std::string, std::string>& existing_characters,
    std::string_view runner_path, std::string_view model_path,
    const std::function<bool()>& continue_waiting = {});

}  // namespace say_count::app
