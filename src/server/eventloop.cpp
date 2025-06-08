#include "eventloop.hpp"

#ifdef __linux__
#include <sys/eventfd.h>
#include <unistd.h>
#elif _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <spdlog/spdlog.h>

namespace lithium::app {

EventLoop::TaskPool EventLoop::task_pool_;

auto EventLoop::Task::operator<(const Task& other) const -> bool {
    if (priority != other.priority) {
        return priority < other.priority;
    }
    return execution_time > other.execution_time;
}

EventLoop::EventLoop(int thread_count) : stop_flag_(false), next_task_id_(0) {
    spdlog::info("**Initializing EventLoop** with {} threads", thread_count);

#ifdef __linux__
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        spdlog::critical("**Failed to create epoll file descriptor**");
        std::exit(EXIT_FAILURE);
    }
    epoll_events_.resize(10);
    signal_fd_ = -1;
    spdlog::debug("**Linux epoll initialized** successfully");
#elif _WIN32
    FD_ZERO(&read_fds_);
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        spdlog::critical("**Windows WSA initialization failed**");
        std::exit(EXIT_FAILURE);
    }
    spdlog::debug("**Windows socket system initialized** successfully");
#endif

#ifdef USE_ASIO
    spdlog::info("**Using Boost.Asio** for I/O operations");
    for (int i = 0; i < thread_count; ++i) {
        thread_pool_.emplace_back([this] {
            spdlog::debug("**Asio worker thread started**");
            io_context_.run();
        });
    }
#else
    spdlog::info("**Using custom thread pool** for task processing");
    for (int i = 0; i < thread_count; ++i) {
        thread_pool_.emplace_back(&EventLoop::workerThread, this);
    }
#endif

    spdlog::info(
        "**EventLoop initialization completed** with {} worker threads",
        thread_count);
}

EventLoop::~EventLoop() {
    spdlog::info("**Shutting down EventLoop**");
    stop();

    for (auto& thread : thread_pool_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

#ifdef __linux__
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        spdlog::debug("**Epoll file descriptor closed**");
    }
    if (signal_fd_ != -1) {
        close(signal_fd_);
        spdlog::debug("**Signal file descriptor closed**");
    }
#elif _WIN32
    WSACleanup();
    spdlog::debug("**Windows socket system cleaned up**");
#endif

    spdlog::info("**EventLoop shutdown completed**");
}

void EventLoop::run() {
    spdlog::info("**Starting EventLoop execution**");
    stop_flag_.store(false);
#ifndef USE_ASIO
    workerThread();
#endif
}

void EventLoop::workerThread() {
    thread_local std::vector<Task> local_tasks;
    thread_local int idle_count = 0;
    local_tasks.reserve(64);

    spdlog::debug("**Worker thread started** [Thread ID: {}]",
                  std::this_thread::get_id());

    while (!stop_flag_.load(std::memory_order_relaxed)) {
        Task task;
        bool has_task = false;

        // **Batch task processing for improved performance**
        {
            constexpr int batch_size = 16;
            for (int i = 0; i < batch_size && task_queue_.pop(task); ++i) {
                auto current_time = std::chrono::steady_clock::now();
                if (task.execution_time <= current_time && task.is_active) {
                    local_tasks.push_back(std::move(task));
                    has_task = true;
                } else {
                    task_queue_.push(std::move(task));
                }
            }
        }

        // **Execute local tasks with error handling**
        for (auto& current_task : local_tasks) {
            if (current_task.is_active) {
                try {
                    current_task.function();
                } catch (const std::exception& e) {
                    spdlog::error("**Task execution failed**: {}", e.what());
                } catch (...) {
                    spdlog::error("**Unknown error during task execution**");
                }
            }

            // **Re-queue periodic tasks**
            if (current_task.type == Task::Type::Periodic) {
                current_task.execution_time += std::chrono::milliseconds(100);
                task_queue_.push(std::move(current_task));
            }
        }
        local_tasks.clear();

        // **Platform-specific I/O multiplexing**
#ifdef __linux__
        constexpr int max_events = 16;
        epoll_event events[max_events];
        int event_count = epoll_wait(epoll_fd_, events, max_events, 1);
        if (event_count > 0) {
            spdlog::trace("**Processing {} epoll events**", event_count);
            for (int i = 0; i < event_count; ++i) {
                handleEpollEvent(events[i]);
            }
            has_task = true;
        } else if (event_count == -1) {
            spdlog::warn("**Epoll wait error**: errno {}", errno);
        }
#endif

        // **Exponential backoff for idle threads**
        if (!has_task) {
            if (++idle_count > 10) {
                auto sleep_duration =
                    std::chrono::microseconds(std::min(1000, idle_count * 100));
                std::this_thread::sleep_for(sleep_duration);
                spdlog::trace("**Thread sleeping** for {}μs due to inactivity",
                              sleep_duration.count());
            }
        } else {
            idle_count = 0;
        }
    }

    spdlog::debug("**Worker thread terminated** [Thread ID: {}]",
                  std::this_thread::get_id());
}

#ifdef __linux__
void EventLoop::handleEpollEvent(const epoll_event& event) {
    if (event.data.fd == signal_fd_) {
        uint64_t signal_value;
        if (read(signal_fd_, &signal_value, sizeof(uint64_t)) > 0) {
            auto handler_iter =
                signal_handlers_.find(static_cast<int>(signal_value));
            if (handler_iter != signal_handlers_.end()) {
                spdlog::debug("**Handling signal**: {}", signal_value);
                post(0, handler_iter->second);
            }
        }
    } else {
        spdlog::trace("**Processing epoll event** for fd: {}", event.data.fd);
    }
}
#endif

void EventLoop::stop() {
    spdlog::info("**Stopping EventLoop**");
    stop_flag_.store(true);
    wakeup();
}

auto EventLoop::adjustTaskPriority(int task_id, int new_priority) -> bool {
    std::unique_lock lock(queue_mutex_);
    std::priority_queue<Task> new_queue;

    bool found = false;
    while (!legacy_tasks_.empty()) {
        Task task = std::move(const_cast<Task&>(legacy_tasks_.top()));
        legacy_tasks_.pop();
        if (task.task_id == task_id) {
            task.priority = new_priority;
            found = true;
            spdlog::debug("**Task priority adjusted**: ID {} to priority {}",
                          task_id, new_priority);
        }
        new_queue.push(std::move(task));
    }

    legacy_tasks_ = std::move(new_queue);

    if (!found) {
        spdlog::warn("**Task not found** for priority adjustment: ID {}",
                     task_id);
    }

    return found;
}

void EventLoop::schedulePeriodic(std::chrono::milliseconds interval,
                                 int priority, std::function<void()> function) {
    spdlog::info("**Scheduling periodic task** with {}ms interval, priority {}",
                 interval.count(), priority);

    std::function<void()> periodic_wrapper;
    periodic_wrapper = [this, interval, priority, func = std::move(function),
                        &periodic_wrapper]() mutable {
        try {
            func();
        } catch (const std::exception& e) {
            spdlog::error("**Periodic task execution failed**: {}", e.what());
        }

        postDelayed(interval, priority,
                    [this, interval, priority, func,
                     &periodic_wrapper]() mutable { periodic_wrapper(); });
    };

    post(priority, std::move(periodic_wrapper));
}

void EventLoop::subscribeEvent(const std::string& event_name,
                               const EventCallback& callback) {
    std::unique_lock lock(queue_mutex_);
    event_subscribers_[event_name].push_back(callback);
    spdlog::debug("**Event subscription added** for event: '{}'", event_name);
}

void EventLoop::emitEvent(const std::string& event_name) {
    std::unique_lock lock(queue_mutex_);
    if (event_subscribers_.count(event_name)) {
        spdlog::debug("**Emitting event**: '{}' to {} subscribers", event_name,
                      event_subscribers_[event_name].size());
        for (const auto& callback : event_subscribers_[event_name]) {
            post(callback);
        }
    } else {
        spdlog::trace("**No subscribers found** for event: '{}'", event_name);
    }
}

void EventLoop::setTimeout(std::function<void()> function,
                           std::chrono::milliseconds delay) {
    spdlog::debug("**Setting timeout** for {}ms", delay.count());
    postDelayed(delay, 0, std::move(function));
}

void EventLoop::setInterval(std::function<void()> function,
                            std::chrono::milliseconds interval) {
    spdlog::debug("**Setting interval** for {}ms", interval.count());
    schedulePeriodic(interval, 0, std::move(function));
}

#ifdef __linux__
void EventLoop::addSignalHandler(int signal_number,
                                 std::function<void()> handler) {
    std::unique_lock lock(queue_mutex_);
    signal_handlers_[signal_number] = std::move(handler);

    spdlog::info("**Adding signal handler** for signal {}", signal_number);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signal_number);
    signal_fd_ = signalfd(-1, &mask, SFD_NONBLOCK);

    if (signal_fd_ == -1) {
        spdlog::error("**Failed to create signalfd** for signal {}",
                      signal_number);
        return;
    }

    epoll_event event_config;
    event_config.events = EPOLLIN;
    event_config.data.fd = signal_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, signal_fd_, &event_config) == -1) {
        spdlog::error("**Failed to add signalfd to epoll** for signal {}",
                      signal_number);
    }
}

void EventLoop::addEpollFileDescriptor(int file_descriptor) const {
    epoll_event event_config;
    event_config.events = EPOLLIN;
    event_config.data.fd = file_descriptor;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, file_descriptor, &event_config) ==
        -1) {
        spdlog::error("**Failed to add file descriptor {} to epoll**",
                      file_descriptor);
    } else {
        spdlog::debug("**File descriptor {} added to epoll**", file_descriptor);
    }
}
#elif _WIN32
void EventLoop::addSocketFileDescriptor(SOCKET socket_fd) {
    FD_SET(socket_fd, &read_fds_);
    spdlog::debug("**Socket file descriptor {} added**", socket_fd);
}
#endif

void EventLoop::wakeup() {
    spdlog::trace("**Waking up event loop**");

#ifdef __linux__
    int event_fd = eventfd(0, EFD_NONBLOCK);
    if (event_fd == -1) {
        spdlog::warn("**Failed to create eventfd for wakeup**");
        return;
    }

    epoll_event event_config = {0};
    event_config.events = EPOLLIN;
    event_config.data.fd = event_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event_fd, &event_config);

    uint64_t one = 1;
    write(event_fd, &one, sizeof(uint64_t));
    close(event_fd);
#elif _WIN32
    SOCKET wakeup_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (wakeup_socket != INVALID_SOCKET) {
        FD_SET(wakeup_socket, &read_fds_);
        closesocket(wakeup_socket);
    }
#endif

    condition_.notify_all();
}

}  // namespace lithium::app
