// src/core/executable_path.cpp
#include "executable_path.h"
#include <string>

#ifdef _WIN32
#include <shlwapi.h>// PathRemoveFileSpec
#include <windows.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <libgen.h>
#include <limits.h>
#include <string>
#include <unistd.h>

#endif

namespace mcp::core {

    std::string getExecutablePath() {
#ifdef _WIN32
        char path[MAX_PATH];
        if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0) {
            return "";
        }
        return std::string(path);
#else
        char result[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
        return count != -1 ? std::string(result, count) : "";
#endif
    }

    std::string getExecutableDirectory() {
        std::string exec_path = getExecutablePath();
        if (exec_path.empty()) return "";

#ifdef _WIN32
        char dir[MAX_PATH];
        strcpy_s(dir, exec_path.c_str());
        PathRemoveFileSpecA(dir);
        return std::string(dir);
#else
        return std::string(dirname(const_cast<char *>(exec_path.c_str())));
#endif
    }

}// namespace mcp::core