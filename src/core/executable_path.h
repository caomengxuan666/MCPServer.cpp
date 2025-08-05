// src/core/executable_path.h
#pragma once
#include <string>

namespace mcp::core {

    // get the full path of the currently running executable
    std::string getExecutablePath();

    // get the directory of the currently running executable
    std::string getExecutableDirectory();

}// namespace mcp::core