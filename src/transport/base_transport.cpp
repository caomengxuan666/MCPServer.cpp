#include "base_transport.h"
#include "core/logger.h"

namespace mcp::transport {

    /**
     * @brief Construct base transport with specified address and port.
     * @param address IP address to bind to
     * @param port Port number to listen on
     */
    BaseTransport::BaseTransport(const std::string &address, unsigned short port)
        : io_context_(),
          acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::make_address(address), port)),
          work_guard_(asio::make_work_guard(io_context_)) {
    }

}// namespace mcp::transport