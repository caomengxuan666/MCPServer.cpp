#ifndef PYTHON_RUNTIME_MANAGER_H
#define PYTHON_RUNTIME_MANAGER_H

#include <string>
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <mutex>
#include <Python.h>

namespace py = pybind11;

class PythonRuntimeManager {
public:
    static PythonRuntimeManager& getInstance();
    
    bool initialize(const std::string& plugin_dir);
    bool isInitialized() const;
    py::module_ importModule(const std::string& module_name);
    void addPath(const std::string& path);
    
private:
    PythonRuntimeManager();
    ~PythonRuntimeManager();
    
    // Save the state of the main thread's GIL
    PyThreadState* main_thread_state_ = nullptr; 
    bool initialized_ = false;
    mutable std::mutex runtime_mutex_; 
};

#endif // PYTHON_RUNTIME_MANAGER_H