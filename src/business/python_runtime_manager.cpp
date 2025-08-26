#include "python_runtime_manager.h"
#include "core/logger.h"
#include <iostream>
#include <sstream>

// Include Python.h after pybind11 headers to avoid conflicts
#include <Python.h>

namespace mcp::business {

    PythonRuntimeManager &PythonRuntimeManager::getInstance() {
        static PythonRuntimeManager instance;
        return instance;
    }

    PythonRuntimeManager::PythonRuntimeManager() : initialized_(false) {
    }


    PythonRuntimeManager::~PythonRuntimeManager() {
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        if (!initialized_) return;

        std::ostringstream oss;
        oss << std::this_thread::get_id();
        MCP_DEBUG("[PYTHON] Finalizing Python interpreter (thread: {})", oss.str());

        // 1. Restore main thread state (must be done before Py_Finalize())
        if (main_thread_state_ != nullptr) {
            PyEval_RestoreThread(main_thread_state_);// Restore main thread GIL holding state
            main_thread_state_ = nullptr;
            MCP_DEBUG("[PYTHON] Main thread state restored");
        }

        // 2. Destroy Python interpreter
        if (Py_IsInitialized()) {
            Py_Finalize();
            MCP_DEBUG("[PYTHON] Py_Finalize() called");
        }

        initialized_ = false;
        MCP_DEBUG("[PYTHON] Runtime finalized");
    }

    void PythonRuntimeManager::setEnvironmentConfig(std::unique_ptr<PythonEnvironmentConfig> config) {
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        env_config_ = std::move(config);
    }

    std::unique_ptr<PythonEnvironmentConfig> PythonRuntimeManager::createEnvironmentConfig(const std::string &type, const std::string &uv_venv_path) {
        if (type == "system") {
            return std::make_unique<SystemEnvConfig>();
        } else if (type == "conda") {
            return std::make_unique<CondaEnvConfig>();
        } else if (type == "uv") {
            return std::make_unique<UvEnvConfig>(uv_venv_path);
        }
        // Default to system environment
        return std::make_unique<SystemEnvConfig>();
    }

    bool PythonRuntimeManager::initialize(const std::string &plugin_dir) {
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        if (initialized_) {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            MCP_DEBUG("[PYTHON] Runtime already initialized (thread: {})", oss.str());
            return true;
        }

        try {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            MCP_DEBUG("[PYTHON] Initializing Python interpreter (thread: {})", oss.str());

            // Set Python home and path if environment config is provided
            if (env_config_) {
                std::string python_home = env_config_->get_python_interpreter_path();
                if (!python_home.empty()) {
// Use PyConfig for Python 3.8+ (more modern approach)
// This is a safer way to set Python home
#if PY_VERSION_HEX >= 0x03080000
                    PyConfig config;
                    PyConfig_InitPythonConfig(&config);
                    PyConfig_SetString(&config, &config.home, Py_DecodeLocale(python_home.c_str(), nullptr));
                    Py_InitializeFromConfig(&config);
                    PyConfig_Clear(&config);
#else
                    Py_SetPythonHome(Py_DecodeLocale(python_home.c_str(), nullptr));
#endif
                    MCP_DEBUG("[PYTHON] Set Python home to: {}", python_home);
                } else {
                    // Initialize Python interpreter (void type, no need to check return value)
                    Py_Initialize();
                }
            } else {
                // Initialize Python interpreter (void type, no need to check return value)
                Py_Initialize();
            }

            MCP_DEBUG("[PYTHON] Py_Initialize() called (interpreter initialized)");

            // 2. Enable multi-threading support (Python 3.9+ compatible, alternative to PyEval_InitThreads())
            PyGILState_STATE gil_state = PyGILState_Ensure();
            PyGILState_Release(gil_state);
            MCP_DEBUG("[PYTHON] Multi-thread support enabled (via PyGILState_Ensure)");

            // 3. Release main thread GIL and save thread state (child threads can acquire GIL)
            main_thread_state_ = PyEval_SaveThread();
            if (main_thread_state_ == nullptr) {
                MCP_WARN("[PYTHON] Warning: PyEval_SaveThread() returned null");
            } else {
                std::ostringstream oss2;
                oss2 << std::this_thread::get_id();
                MCP_DEBUG("[PYTHON] Main thread GIL released (thread: {})", oss2.str());
            }

            // 4. Re-acquire GIL and add plugin directory to sys.path
            py::gil_scoped_acquire acquire;
            py::module_ sys = py::module_::import("sys");
            sys.attr("path").attr("append")(plugin_dir);
            MCP_DEBUG("[PYTHON] Added plugin dir to sys.path: {}", plugin_dir);

            // Add additional paths from environment config if provided
            if (env_config_) {
                std::string python_path = env_config_->get_python_path();
                if (!python_path.empty()) {
                    sys.attr("path").attr("append")(python_path);
                    MCP_DEBUG("[PYTHON] Added Python path to sys.path: {}", python_path);
                }
            }

            // Fix: Convert py::str to std::string before output
            std::string sys_path_str = py::str(sys.attr("path")).cast<std::string>();
            MCP_DEBUG("[PYTHON] sys.path: {}", sys_path_str);

            initialized_ = true;
            MCP_INFO("[PYTHON] Runtime initialized successfully");
            return true;
        } catch (const py::error_already_set &e) {
            MCP_ERROR("[PYTHON] Init Python error: {}", e.what());
            //MCP_ERROR("[PYTHON] Traceback: {}", e.trace());
            if (Py_IsInitialized()) {
                Py_Finalize();// Only call when already initialized
            }
            return false;
        } catch (const std::exception &e) {
            MCP_ERROR("[PYTHON] Init C++ error: {}", e.what());
            if (Py_IsInitialized()) {
                Py_Finalize();
            }
            return false;
        }
    }

    bool PythonRuntimeManager::isInitialized() const {
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        return initialized_;
    }

    py::module_ PythonRuntimeManager::importModule(const std::string &module_name) {
        std::lock_guard<std::mutex> lock(runtime_mutex_);

        if (!initialized_) {
            throw std::runtime_error("Python runtime not initialized");
        }

        return py::module_::import(module_name.c_str());
    }

    void PythonRuntimeManager::addPath(const std::string &path) {
        std::lock_guard<std::mutex> lock(runtime_mutex_);

        if (!initialized_) {
            throw std::runtime_error("Python runtime not initialized");
        }

        py::module_ sys = py::module_::import("sys");
        sys.attr("path").attr("append")(path);
    }

}// namespace mcp::business

// Implementation of PythonConfigObserver
namespace mcp::business {

    PythonConfigObserver::PythonConfigObserver(PythonRuntimeManager &manager)
        : runtime_manager_(manager) {}

    void PythonConfigObserver::onConfigReloaded(const mcp::config::GlobalConfig &newConfig) {
        MCP_INFO("PythonConfigObserver: Applying new Python environment configuration...");

        try {
            auto new_env_config = PythonRuntimeManager::createEnvironmentConfig(
                    newConfig.python_env.default_env,
                    newConfig.python_env.uv_venv_path);
            runtime_manager_.setEnvironmentConfig(std::move(new_env_config));
            MCP_DEBUG("Python environment updated: default='{}', uv_venv='{}'",
                      newConfig.python_env.default_env,
                      newConfig.python_env.uv_venv_path);
        } catch (const std::exception &e) {
            MCP_ERROR("Failed to update Python environment: {}", e.what());
        }
    }

}// namespace mcp::business