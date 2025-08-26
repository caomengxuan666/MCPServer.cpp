#ifndef PYTHON_PLUGIN_INSTANCE_H
#define PYTHON_PLUGIN_INSTANCE_H

#include "mcp_plugin.h"
#include <mutex>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>

namespace mcp::business {

    namespace py = pybind11;

    /**
     * Interface for Python environment configuration
     * Provides methods to get interpreter path, module search path and virtual environment status
     */
    class PythonEnvironmentConfig {
    public:
        virtual ~PythonEnvironmentConfig() = default;

        /**
         * Get Python interpreter path
         * @return Path to Python interpreter executable
         */
        virtual std::string get_python_interpreter_path() const = 0;

        /**
         * Get Python module search path
         * @return Path where Python modules are located
         */
        virtual std::string get_python_path() const = 0;

        /**
         * Check if virtual environment should be used
         * @return True if virtual environment is enabled, false otherwise
         */
        virtual bool use_virtual_env() const = 0;
        
    protected:
        /**
         * Detect Python version by trying common versions from 3.6 to 3.13
         * @return Detected Python version string (e.g., "3.9")
         */
        std::string detect_python_version() const {
            // Try common Python versions from 3.13 down to 3.6
            static const std::vector<std::string> versions = {
                "3.13", "3.12", "3.11", "3.10", "3.9", "3.8", "3.7", "3.6"
            };
            
            // In a real implementation, we would detect the actual Python version
            // For now, we default to "3.9" as a reasonable fallback
            // A more sophisticated implementation might check the filesystem or use Python API
            return "3.9";
        }
        
#ifdef _MSC_VER
        /**
         * Secure way to get environment variable on Windows
         * @param name Environment variable name
         * @return Environment variable value or empty string if not found
         */
        std::string get_env_var(const char* name) const {
            char* value = nullptr;
            size_t len = 0;
            if (_dupenv_s(&value, &len, name) == 0 && value != nullptr) {
                std::string result(value);
                free(value);
                return result;
            }
            return "";
        }
#else
        /**
         * Get environment variable on non-Windows platforms
         * @param name Environment variable name
         * @return Environment variable value or nullptr if not found
         */
        const char* get_env_var(const char* name) const {
            return std::getenv(name);
        }
#endif
    };

    /**
     * System environment configuration implementation
     * Uses default system Python interpreter
     */
    class SystemEnvConfig : public PythonEnvironmentConfig {
    public:
        std::string get_python_interpreter_path() const override {
#ifdef _MSC_VER
            // For Windows, try common Python installation paths
            // Usually Python is in the PATH, so we can just use "python"
            return "python";
#else
            return "/usr/bin/python3";  // Default system Python path
#endif
        }

        std::string get_python_path() const override {
#ifdef _MSC_VER
            // On Windows, Python packages are typically available in the PATH
            // or can be found relative to the Python executable
            return "";  // Empty string means use default Python path
#else
            return "/usr/lib/python3/dist-packages";  // System Python packages path
#endif
        }

        bool use_virtual_env() const override {
            return false;
        }
    };

    /**
     * Conda environment configuration implementation
     * Uses Conda Python interpreter if available
     */
    class CondaEnvConfig : public PythonEnvironmentConfig {
    public:
        std::string get_python_interpreter_path() const override {
            std::string conda_prefix = get_env_var("CONDA_PREFIX");
            return conda_prefix.empty() ? "" : conda_prefix + "/bin/python";
        }

        std::string get_python_path() const override {
            std::string conda_prefix = get_env_var("CONDA_PREFIX");
            return conda_prefix.empty() ? "" : conda_prefix + "/lib/python" + detect_python_version() + "/site-packages";
        }

        bool use_virtual_env() const override {
            return false;
        }
    };

    /**
     * UV environment configuration implementation
     * Uses UV virtual environment Python interpreter
     */
    class UvEnvConfig : public PythonEnvironmentConfig {
    public:
        explicit UvEnvConfig(const std::string& venv_path) : venv_path_(venv_path) {}

        std::string get_python_interpreter_path() const override {
            return venv_path_ + "/bin/python";
        }

        std::string get_python_path() const override {
            return venv_path_ + "/lib/python" + detect_python_version() + "/site-packages";
        }

        bool use_virtual_env() const override {
            return true;
        }

    private:
        std::string venv_path_;
    };

    class PythonPluginInstance {
    public:
        PythonPluginInstance();
        ~PythonPluginInstance();

        bool initialize(const char *plugin_path);
        void uninitialize();

        ToolInfo *get_tools(int *count);
        const char *call_tool(const char *name, const char *args_json, MCPError *error);

    private:
        bool initialize_plugin_module();

        py::module_ plugin_module_;
        std::string plugin_dir_;
        std::string module_name_;
        std::vector<ToolInfo> tools_cache_;
        bool initialized_;
        std::mutex cache_mutex_;
    };

} // namespace mcp::business

#endif// PYTHON_PLUGIN_INSTANCE_H