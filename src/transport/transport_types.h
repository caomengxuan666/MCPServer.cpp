#pragma once

#include <asio.hpp>
#include <functional>
#include <memory>
#include <string>

namespace mcp::transport {
    // global callback type for handling messages
    using MessageCallback = std::function<void(const std::string &, std::shared_ptr<class Session>, const std::string &)>;

}// namespace mcp::transport