#include "ThreadPoolWorker.h"
#include <iostream>
#include <chrono>

namespace transport {

ThreadPoolWorker::ThreadPoolWorker(int workerId)
    : workerId_(workerId), running_(false)
{
}

ThreadPoolWorker::~ThreadPoolWorker()
{
    stop();
}

void ThreadPoolWorker::run()
{
    running_ = true;
    std::cout << "[ThreadPoolWorker::run] Worker " << workerId_ << " started" << std::endl;

    // Placeholder: simple loop
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[ThreadPoolWorker::run] Worker " << workerId_ << " stopped" << std::endl;
}

void ThreadPoolWorker::stop()
{
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ThreadPoolWorker::process(const std::string& request)
{
    std::cout << "[ThreadPoolWorker::process] Worker " << workerId_ << " processing: " << request << std::endl;
}

} // namespace transport
