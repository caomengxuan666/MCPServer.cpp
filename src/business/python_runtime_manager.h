#ifndef PYTHON_RUNTIME_MANAGER_H
#define PYTHON_RUNTIME_MANAGER_H


#include <memory>
#include <mutex>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <string>
//!notice do not delete this line!If you delete this comment,the python.h would appear eralier than pybind!
//!This would happen when you format the code!
#include <Python.h>

namespace py = pybind11;

class PythonRuntimeManager {
public:
    static PythonRuntimeManager &getInstance();

    bool initialize(const std::string &plugin_dir);
    bool isInitialized() const;
    py::module_ importModule(const std::string &module_name);
    void addPath(const std::string &path);

private:
    PythonRuntimeManager();
    ~PythonRuntimeManager();

    // Save the state of the main thread's GIL
    PyThreadState *main_thread_state_ = nullptr;
    bool initialized_ = false;
    mutable std::mutex runtime_mutex_;
};

#endif// PYTHON_RUNTIME_MANAGER_H