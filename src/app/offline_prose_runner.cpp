#include "app/offline_prose_runner.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#ifdef __linux__
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace say_count::app {
namespace {

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

}  // namespace

OfflineProseRunResult run_offline_prose_model(
    std::string_view manuscript,
    const std::map<std::string, std::string>& existing_characters,
    std::string_view runner_path, std::string_view model_path,
    const std::function<bool()>& continue_waiting) {
#ifndef __linux__
    (void)manuscript; (void)existing_characters; (void)runner_path; (void)model_path;
    (void)continue_waiting;
    return {{}, "Offline AI network isolation is currently available on Linux only.", false};
#else
    namespace fs = std::filesystem;
    const fs::path runner(runner_path);
    const fs::path model(model_path);
    if (!fs::is_regular_file(runner) || access(runner.c_str(), X_OK) != 0)
        return {{}, "The configured local model runner is not executable.", false};
    if (!fs::is_regular_file(model))
        return {{}, "The configured local GGUF model file was not found.", false};
    if (access("/usr/bin/bwrap", X_OK) != 0)
        return {{}, "Network isolation requires /usr/bin/bwrap, which was not found.", false};

    char directory_template[] = "/tmp/say-count-offline-ai-XXXXXX";
    const char* made = mkdtemp(directory_template);
    if (!made) return {{}, "Could not create private temporary storage for the local model.", false};
    const fs::path directory(made);
    const fs::path prompt_path = directory / "prompt.txt";
    const fs::path output_path = directory / "response.txt";
    const fs::path error_path = directory / "runner.log";
    {
        std::ofstream prompt(prompt_path, std::ios::binary | std::ios::trunc);
        prompt << build_offline_prose_prompt(manuscript, existing_characters);
        if (!prompt) { fs::remove_all(directory); return {{}, "Could not prepare the local prompt.", false}; }
    }
    chmod(prompt_path.c_str(), S_IRUSR);

    const pid_t pid = fork();
    if (pid < 0) { fs::remove_all(directory); return {{}, "Could not start the local model.", false}; }
    if (pid == 0) {
        setpgid(0, 0);
        const int output = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        const int errors = open(error_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        const int input = open("/dev/null", O_RDONLY);
        if (output < 0 || errors < 0 || input < 0) _exit(126);
        dup2(input, STDIN_FILENO); dup2(output, STDOUT_FILENO); dup2(errors, STDERR_FILENO);
        close(input); close(output); close(errors);

        std::vector<std::string> arguments{
            "/usr/bin/bwrap", "--unshare-net", "--die-with-parent", "--new-session",
            "--ro-bind", "/", "/", "--bind", directory.string(), directory.string(),
            "--dev-bind", "/dev", "/dev", "--proc", "/proc", "--chdir", directory.string(),
            "--setenv", "HOME", directory.string(), "--", runner.string(),
            "-m", model.string(), "-f", prompt_path.string(), "-n", "2048", "--temp", "0",
            "--no-display-prompt"
        };
        std::vector<char*> argv;
        for (auto& argument : arguments) argv.push_back(argument.data());
        argv.push_back(nullptr);
        execv(argv.front(), argv.data());
        _exit(127);
    }

    setpgid(pid, pid);
    int status = 0;
    bool cancelled = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);
    while (waitpid(pid, &status, WNOHANG) == 0) {
        if ((continue_waiting && !continue_waiting()) || std::chrono::steady_clock::now() >= deadline) {
            cancelled = true;
            kill(-pid, SIGTERM);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            kill(-pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(75));
    }

    const std::string response = ReadFile(output_path);
    const std::string runner_log = ReadFile(error_path);
    fs::remove_all(directory);
    if (cancelled) return {{}, "Local AI conversion was cancelled or timed out.", true};
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::string detail = runner_log.substr(0, 500);
        return {{}, "The local model runner failed." + (detail.empty() ? std::string{} : "\n" + detail), false};
    }
    auto rewrite = apply_offline_prose_response(manuscript, response);
    if (!rewrite) {
        const std::string error = rewrite.error;
        return {std::move(rewrite), error, false};
    }
    return {std::move(rewrite), {}, false};
#endif
}

}  // namespace say_count::app
