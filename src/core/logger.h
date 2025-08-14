#pragma once

#include <memory>
#include <string>

// Include format library for format string support
#include <spdlog/fmt/fmt.h>
#include <spdlog/logger.h>

#define MCP_TRACE(...) ::mcp::core::MCPLogger::instance().trace(__VA_ARGS__)
#define MCP_DEBUG(...) ::mcp::core::MCPLogger::instance().debug(__VA_ARGS__)
#define MCP_INFO(...) ::mcp::core::MCPLogger::instance().info(__VA_ARGS__)
#define MCP_WARN(...) ::mcp::core::MCPLogger::instance().warn(__VA_ARGS__)
#define MCP_ERROR(...) ::mcp::core::MCPLogger::instance().error(__VA_ARGS__)
#define MCP_CRITICAL(...) ::mcp::core::MCPLogger::instance().critical(__VA_ARGS__)

namespace mcp {
    namespace core {

        /**
        * @brief Log levels for the logger
        */
        enum class LogLevel {
            TRACE = 0,
            DEBUG = 1,
            INFO = 2,
            WARN = 3,
            ERR = 4,
            CRITICAL = 5,
            OFF = 6
        };

        // global logger instance
        extern std::shared_ptr<spdlog::logger> g_logger;
        extern LogLevel g_current_level;


        /**
    * @brief Logger class that encapsulates spdlog functionality
    */
        class MCPLogger {
        public:
            static MCPLogger &instance();

            std::shared_ptr<spdlog::logger> operator->();

            template<typename... Args>
            void trace(fmt::format_string<Args...> fmt, Args &&...args) {
                log(LogLevel::TRACE, fmt, std::forward<Args>(args)...);
            }

            template<typename... Args>
            void debug(fmt::format_string<Args...> fmt, Args &&...args) {
                log(LogLevel::DEBUG, fmt, std::forward<Args>(args)...);
            }

            template<typename... Args>
            void info(fmt::format_string<Args...> fmt, Args &&...args) {
                log(LogLevel::INFO, fmt, std::forward<Args>(args)...);
            }

            template<typename... Args>
            void warn(fmt::format_string<Args...> fmt, Args &&...args) {
                log(LogLevel::WARN, fmt, std::forward<Args>(args)...);
            }

            template<typename... Args>
            void error(fmt::format_string<Args...> fmt, Args &&...args) {
                log(LogLevel::ERR, fmt, std::forward<Args>(args)...);
            }

            template<typename... Args>
            void critical(fmt::format_string<Args...> fmt, Args &&...args) {
                log(LogLevel::CRITICAL, fmt, std::forward<Args>(args)...);
            }

            // Overloads for string literals (without format arguments)
            void trace(const char *msg);
            void debug(const char *msg);
            void info(const char *msg);
            void warn(const char *msg);
            void error(const char *msg);
            void critical(const char *msg);

            void set_level(LogLevel level);
            static void enable_file_sink(){enable_file_logging_ = true;}
            static void disable_file_sink(){enable_file_logging_ = false;}
            static bool is_file_sink_enabled() {return enable_file_logging_;}

            LogLevel get_level() const;

        private:
            MCPLogger() = default;

            static bool enable_file_logging_;

            template<typename... Args>
            void log(LogLevel level, fmt::format_string<Args...> fmt, Args &&...args) {
                if (!g_logger || static_cast<int>(level) < static_cast<int>(g_current_level)) {
                    return;
                }

                try {
                    std::string formatted_msg = fmt::format(fmt, std::forward<Args>(args)...);
                    switch (level) {
                        case LogLevel::TRACE:
                            g_logger->trace(formatted_msg);
                            break;
                        case LogLevel::DEBUG:
                            g_logger->debug(formatted_msg);
                            break;
                        case LogLevel::INFO:
                            g_logger->info(formatted_msg);
                            break;
                        case LogLevel::WARN:
                            g_logger->warn(formatted_msg);
                            break;
                        case LogLevel::ERR:
                            g_logger->error(formatted_msg);
                            break;
                        case LogLevel::CRITICAL:
                            g_logger->critical(formatted_msg);
                            break;
                        default:
                            g_logger->info(formatted_msg);
                            break;
                    }
                } catch (const std::exception &e) {
                    // Fallback in case of formatting error
                    g_logger->error("Log formatting error: {}", e.what());
                }
            }
        };

        /**
    * @brief Initialize global spdlog in asynchronous mode
    * @param log_path Log file path
    * @param log_level Log level (trace, debug, info, warn, error, critical, off)
    * @param max_file_size Maximum size of each log file (bytes)
    * @param max_files Maximum number of log files
    */
        void initializeAsyncLogger(
                const std::string &log_path,
                const std::string &log_level = "info",
                size_t max_file_size = 1048576 * 5,// 5MB
                size_t max_files = 3);

        // Convenience functions for the default logger
        template<typename... Args>
        void trace(fmt::format_string<Args...> fmt, Args &&...args) {
            MCPLogger::instance().trace(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void debug(fmt::format_string<Args...> fmt, Args &&...args) {
            MCPLogger::instance().debug(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void info(fmt::format_string<Args...> fmt, Args &&...args) {
            MCPLogger::instance().info(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void warn(fmt::format_string<Args...> fmt, Args &&...args) {
            MCPLogger::instance().warn(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void error(fmt::format_string<Args...> fmt, Args &&...args) {
            MCPLogger::instance().error(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void critical(fmt::format_string<Args...> fmt, Args &&...args) {
            MCPLogger::instance().critical(fmt, std::forward<Args>(args)...);
        }
    }// namespace core
}// namespace mcp