#include "python_runtime_manager.h"
#include <iostream>
#include "core/logger.h"

PythonRuntimeManager& PythonRuntimeManager::getInstance() {
    static PythonRuntimeManager instance;
    return instance;
}

PythonRuntimeManager::PythonRuntimeManager() : initialized_(false) {
}


PythonRuntimeManager::~PythonRuntimeManager() {
    std::lock_guard<std::mutex> lock(runtime_mutex_);
    if (!initialized_) return;

    MCP_DEBUG("[PYTHON] Finalizing Python runtime (thread: {})", std::this_thread::get_id());

    // 1. Restore main thread state (must be done before Py_Finalize())
    if (main_thread_state_ != nullptr) {
        PyEval_RestoreThread(main_thread_state_); // Restore main thread GIL holding state
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

bool PythonRuntimeManager::initialize(const std::string& plugin_dir) {
    std::lock_guard<std::mutex> lock(runtime_mutex_);
    if (initialized_) {
        MCP_DEBUG("[PYTHON] Runtime already initialized (thread: {})", std::this_thread::get_id());
        return true;
    }

    try {
        MCP_DEBUG("[PYTHON] Initializing Python interpreter (thread: {})", std::this_thread::get_id());
        
        // 1. Initialize Python interpreter (void type, no need to check return value)
        Py_Initialize();
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
            MCP_DEBUG("[PYTHON] Main thread GIL released (thread: {})", std::this_thread::get_id());
        }

        // 4. Re-acquire GIL and add plugin directory to sys.path
        py::gil_scoped_acquire acquire;
        py::module_ sys = py::module_::import("sys");
        sys.attr("path").attr("append")(plugin_dir);
        MCP_DEBUG("[PYTHON] Added plugin dir to sys.path: {}", plugin_dir);
        
        // Fix: Convert py::str to std::string before output
        std::string sys_path_str = py::str(sys.attr("path")).cast<std::string>();
        MCP_DEBUG("[PYTHON] sys.path: {}", sys_path_str);

        initialized_ = true;
        MCP_INFO("[PYTHON] Runtime initialized successfully");
        return true;
    } catch (const py::error_already_set& e) {
        MCP_ERROR("[PYTHON] Init Python error: {}", e.what());
        //MCP_ERROR("[PYTHON] Traceback: {}", e.trace());
        if (Py_IsInitialized()) {
            Py_Finalize(); // Only call when already initialized
        }
        return false;
    } catch (const std::exception& e) {
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

py::module_ PythonRuntimeManager::importModule(const std::string& module_name) {
    std::lock_guard<std::mutex> lock(runtime_mutex_);
    
    if (!initialized_) {
        throw std::runtime_error("Python runtime not initialized");
    }
    
    return py::module_::import(module_name.c_str());
}

void PythonRuntimeManager::addPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(runtime_mutex_);
    
    if (!initialized_) {
        throw std::runtime_error("Python runtime not initialized");
    }
    
    py::module_ sys = py::module_::import("sys");
    sys.attr("path").attr("append")(path);
}