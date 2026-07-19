#pragma once

#include <string>
#include <vector>

namespace say_count {

enum class ChatRuntimeFileState { NewFile, Identical, Modified };

struct ChatRuntimeFile {
    std::string name;
    std::string source_path;
    std::string destination_path;
    ChatRuntimeFileState state = ChatRuntimeFileState::NewFile;
    bool project_owned = false;
};

struct ChatRuntimeInspection {
    std::vector<ChatRuntimeFile> files;
    std::string destination_directory;
    std::string error;

    bool has_modified_files() const noexcept;
    bool has_new_files() const noexcept;
};

struct ChatRuntimeInstallResult {
    std::size_t installed = 0;
    std::size_t identical = 0;
    std::string error;
};

ChatRuntimeInspection inspect_chat_runtime(std::string resource_directory,
                                           std::string scripts_directory,
                                           std::string vendor_directory = "chat_program");
ChatRuntimeInstallResult install_chat_runtime(const ChatRuntimeInspection& inspection);

}  // namespace say_count
