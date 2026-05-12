#ifndef TRANSPORT_THREADPOOLWORKER_H
#define TRANSPORT_THREADPOOLWORKER_H

#include <string>
#include <thread>

namespace transport {

/**
 * Worker thread that processes incoming requests from the HTTP server.
 * Can be configured with a callback or derived for custom logic.
 */
class ThreadPoolWorker {
private:
    int workerId_;
    bool running_;
    std::thread thread_;

public:
    ThreadPoolWorker(int workerId = 0);
    ~ThreadPoolWorker();

    /**
     * Run the worker thread (blocking until stop() is called).
     */
    void run();

    /**
     * Signal the worker thread to stop.
     */
    void stop();

    /**
     * Process a request string (to be overridden in subclass or via callback).
     */
    virtual void process(const std::string& request);

    int getWorkerId() const { return workerId_; }
    bool isRunning() const { return running_; }
};

} // namespace transport

#endif // TRANSPORT_THREADPOOLWORKER_H
