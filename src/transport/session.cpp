#include "session.h"
#include "core/logger.h"
#include "http_handler.h"
#include <iomanip>
#include <random>

using asio::use_awaitable;

namespace mcp::transport {

    namespace {
        /**
         * @brief Generate a random session ID using 128 bits of random data.
         * @return 32-character hexadecimal string
         */
        static std::string generate_session_id() noexcept {
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
    }// namespace


    /**
     * @brief Base session implementation shared by TCP and SSL sessions.
     * Note: Socket management is delegated to subclasses.
     */
    // Base class methods are implemented in subclasses since socket access is subclass-specific
}// namespace mcp::transport