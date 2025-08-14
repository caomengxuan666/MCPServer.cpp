#pragma once
#include <random>
#include <string>
#include <sstream>
#include <iomanip>

namespace mcp::utils {

    inline std::string generate_session_id() noexcept {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint64_t> dist;

        uint64_t part1 = dist(gen);
        uint64_t part2 = dist(gen);

        std::stringstream ss;
        ss << std::hex << std::setw(16) << std::setfill('0') << part1
           << std::hex << std::setw(16) << std::setfill('0') << part2;
        return ss.str();
    }
}// namespace mcp::utils