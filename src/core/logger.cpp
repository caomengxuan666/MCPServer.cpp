#include "logger.h"
#include <memory>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
namespace mcp {
    namespace core {

        // Static logger instance
        std::shared_ptr<spdlog::logger> g_logger = nullptr;
        LogLevel g_current_level = LogLevel::INFO;

        MCPLogger &MCPLogger::instance() {
            static MCPLogger instance;
            return instance;
        }

        // allow access to the underlying spdlog::logger
        std::shared_ptr<spdlog::logger> MCPLogger::operator->() {
            return g_logger;
        }

        void MCPLogger::set_level(LogLevel level) {
            g_current_level = level;
            if (g_logger) {
                g_logger->set_level(static_cast<spdlog::level::level_enum>(static_cast<int>(level)));
            }
        }

        LogLevel MCPLogger::get_level() const {
            return g_current_level;
        }

        // Overloads for string literals (without format arguments)
        void MCPLogger::trace(const char *msg) {
            if (g_logger && static_cast<int>(LogLevel::TRACE) >= static_cast<int>(g_current_level)) {
                g_logger->trace(msg);
            }
        }

        void MCPLogger::debug(const char *msg) {
            if (g_logger && static_cast<int>(LogLevel::DEBUG) >= static_cast<int>(g_current_level)) {
                g_logger->debug(msg);
            }
        }

        void MCPLogger::info(const char *msg) {
            if (g_logger && static_cast<int>(LogLevel::INFO) >= static_cast<int>(g_current_level)) {
                g_logger->info(msg);
            }
        }

        void MCPLogger::warn(const char *msg) {
            if (g_logger && static_cast<int>(LogLevel::WARN) >= static_cast<int>(g_current_level)) {
                g_logger->warn(msg);
            }
        }

        void MCPLogger::error(const char *msg) {
            if (g_logger && static_cast<int>(LogLevel::ERR) >= static_cast<int>(g_current_level)) {
                g_logger->error(msg);
            }
        }

        void MCPLogger::critical(const char *msg) {
            if (g_logger && static_cast<int>(LogLevel::CRITICAL) >= static_cast<int>(g_current_level)) {
                g_logger->critical(msg);
            }
        }

        void initializeAsyncLogger(const std::string &log_path, const std::string &log_level, size_t max_file_size,
                                   size_t max_files) {
            // Initialize thread pool
            spdlog::init_thread_pool(8192, 1);

            // Create console and file sinks
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_color_mode(spdlog::color_mode::always);

            // Set custom colors for different log levels
#ifdef _WIN32

            console_sink->set_color(spdlog::level::trace, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            console_sink->set_color(spdlog::level::debug, FOREGROUND_BLUE | FOREGROUND_GREEN);
            console_sink->set_color(spdlog::level::info, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            console_sink->set_color(spdlog::level::warn, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            console_sink->set_color(spdlog::level::err, FOREGROUND_RED | FOREGROUND_INTENSITY);
            console_sink->set_color(spdlog::level::critical, BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
#else
            console_sink->set_color(spdlog::level::trace, "\033[36m");           // Cyan
            console_sink->set_color(spdlog::level::debug, "\033[34m");           // Blue
            console_sink->set_color(spdlog::level::info, "\033[32m");            // Green
            console_sink->set_color(spdlog::level::warn, "\033[33m");            // Yellow
            console_sink->set_color(spdlog::level::err, "\033[31m");             // Red
            console_sink->set_color(spdlog::level::critical, "\033[41m\033[37m");// White on red background
#endif

            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_path, max_file_size, max_files);

            // Set log level
            spdlog::level::level_enum level_val = spdlog::level::info;
            if (log_level == "trace") {
                level_val = spdlog::level::trace;
                g_current_level = LogLevel::TRACE;
            } else if (log_level == "debug") {
                level_val = spdlog::level::debug;
                g_current_level = LogLevel::DEBUG;
            } else if (log_level == "warn") {
                level_val = spdlog::level::warn;
                g_current_level = LogLevel::WARN;
            } else if (log_level == "error") {
                level_val = spdlog::level::err;
                g_current_level = LogLevel::ERR;
            } else if (log_level == "critical") {
                level_val = spdlog::level::critical;
                g_current_level = LogLevel::CRITICAL;
            } else if (log_level == "off") {
                level_val = spdlog::level::off;
                g_current_level = LogLevel::OFF;
            }

            // Create async logger
            g_logger = std::make_shared<spdlog::logger>("mcp_logger", spdlog::sinks_init_list{console_sink, file_sink});
            g_logger->set_level(level_val);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");

            spdlog::register_logger(g_logger);
            spdlog::set_default_logger(g_logger);

            // Start periodic flushing (every 3 seconds)
            spdlog::flush_every(std::chrono::seconds(3));
        }

    }// namespace core
}// namespace mcp