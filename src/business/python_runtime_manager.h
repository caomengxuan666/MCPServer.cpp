#ifndef PYTHON_RUNTIME_MANAGER_H
#define PYTHON_RUNTIME_MANAGER_H

#include "config/config.hpp"
#include "config/config_observer.hpp"
#include "python_plugin_instance.h"
#include <memory>
#include <mutex>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <string>


// Forward declaration for Python types
struct _ts;// PyThreadState
typedef struct _ts PyThreadState;

namespace mcp {
    namespace config {
        struct GlobalConfig;
    }
}// namespace mcp

namespace mcp::business {
    namespace py = pybind11;
    class PythonRuntimeManager;

    // Observer that listens for config changes and updates Python runtime
    class PythonConfigObserver : public mcp::config::ConfigObserver {
    private:
        PythonRuntimeManager &runtime_manager_;

    public:
        explicit PythonConfigObserver(PythonRuntimeManager &manager);
        void onConfigReloaded(const mcp::config::GlobalConfig &newConfig) override;
    };

    class PythonRuntimeManager {
    public:
        static PythonRuntimeManager &getInstance();

        bool initialize(const std::string &plugin_dir);
        bool isInitialized() const;
        py::module_ importModule(const std::string &module_name);
        void addPath(const std::string &path);

        // Set Python environment configuration
        void setEnvironmentConfig(std::unique_ptr<PythonEnvironmentConfig> config);

        // Create environment config based on type
        static std::unique_ptr<PythonEnvironmentConfig> createEnvironmentConfig(const std::string &type, const std::string &uv_venv_path = "");

    private:
        PythonRuntimeManager();
        ~PythonRuntimeManager();

        // Save the state of the main thread's GIL
        PyThreadState *main_thread_state_ = nullptr;
        bool initialized_ = false;
        mutable std::mutex runtime_mutex_;

        // Python environment configuration
        std::unique_ptr<PythonEnvironmentConfig> env_config_;
    };

}// namespace mcp::business

#endif// PYTHON_RUNTIME_MANAGER_H