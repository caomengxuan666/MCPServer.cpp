#pragma once

#include <string>
#include <vector>

namespace mcp {
    namespace utils {

        /**
        * @brief Load authentication keys from a file
        * 
        * @param env_file_path Path to the environment file containing auth keys
        * @return std::vector<std::string> Vector of loaded auth keys
        */
        std::vector<std::string> load_auth_keys_from_file(const std::string &env_file_path);

    }// namespace utils
}// namespace mcp