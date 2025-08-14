#include "auth_utils.h"
#include "core/executable_path.h"
#include "core/logger.h"
#include <filesystem>
#include <fstream>


namespace mcp {
    namespace utils {

        std::vector<std::string> load_auth_keys_from_file(const std::string &env_file_path) {
            std::vector<std::string> keys;

            // Construct full path relative to executable directory
            std::filesystem::path executable_dir(mcp::core::getExecutableDirectory());
            std::filesystem::path full_path = executable_dir / env_file_path;

            // Check if file exists
            if (!std::filesystem::exists(full_path)) {
                MCP_WARN("Auth environment file not found: {}", full_path.string());
                return keys;
            }

            // Open file
            std::ifstream file(full_path);
            if (!file.is_open()) {
                MCP_ERROR("Failed to open auth environment file: {}", full_path.string());
                return keys;
            }

            // Read file line by line
            std::string line;
            while (std::getline(file, line)) {
                // Skip empty lines and comments (lines starting with #)
                if (line.empty() || line[0] == '#') {
                    continue;
                }

                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);

                // Skip if line is empty after trimming
                if (line.empty()) {
                    continue;
                }

                // Add key to vector
                keys.push_back(line);
            }

            MCP_DEBUG("Loaded {} auth keys from {}", keys.size(), full_path.string());
            return keys;
        }

    }// namespace utils
}// namespace mcp