#pragma once
#include "Singleton.h"
#include "asio.hpp"
#include <thread>
#include <vector>

class AsioIOServicePool : public Singleton<AsioIOServicePool> {
    friend Singleton<AsioIOServicePool>;

public:
    using IOService = asio::io_context;
    using Work = asio::executor_work_guard<asio::io_context::executor_type>;
    using WorkPtr = std::unique_ptr<Work>;
    ~AsioIOServicePool();
    AsioIOServicePool(const AsioIOServicePool &) = delete;
    AsioIOServicePool &operator=(const AsioIOServicePool &) = delete;

    asio::io_context &GetIOService();
    void Stop();

    static std::shared_ptr<AsioIOServicePool> GetInstance() {
        return Singleton<AsioIOServicePool>::GetInstance();
    }

private:
    AsioIOServicePool(std::size_t size = 2 /*std::thread::hardware_concurrency()*/);

    std::vector<IOService> _ioServices;
    std::vector<WorkPtr> _works;
    std::vector<std::thread> _threads;
    std::size_t _nextIOService;
};

inline AsioIOServicePool::AsioIOServicePool(std::size_t size) : _ioServices(size),
                                                                _works(size), _nextIOService(0) {
    for (std::size_t i = 0; i < size; ++i) {
        _works[i] = std::make_unique<Work>(asio::make_work_guard(_ioServices[i]));
    }

    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        _threads.emplace_back([this, i]() {
            _ioServices[i].run();
        });
    }
}

inline AsioIOServicePool::~AsioIOServicePool() {
    Stop();
}

inline asio::io_context &AsioIOServicePool::GetIOService() {
    auto &service = _ioServices[_nextIOService++];
    if (_nextIOService == _ioServices.size()) {
        _nextIOService = 0;
    }
    return service;
}

inline void AsioIOServicePool::Stop() {
    for (auto &work: _works) {
        work.reset();
    }

    for (auto &io: _ioServices) {
        io.stop();
    }

    for (auto &t: _threads) {
        if (t.joinable() && t.get_id() != std::this_thread::get_id()) {
            try {
                t.join();
            } catch (const std::system_error &e) {
                std::cerr << "Error joining thread: " << e.what() << std::endl;
            }
        }
    }
}