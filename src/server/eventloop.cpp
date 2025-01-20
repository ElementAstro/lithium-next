#include "eventloop.hpp"

#ifdef __linux__
#include <sys/eventfd.h>
#include <unistd.h>
#elif _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "atom/log/loguru.hpp"

namespace lithium::app {
EventLoop::EventLoop(int num_threads) : stop_flag_(false) {
#ifdef __linux__
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        ABORT_F("Failed to create epoll file descriptor");
        exit(EXIT_FAILURE);
    }
    epoll_events_.resize(10);
#elif _WIN32
    FD_ZERO(&read_fds);
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

#ifdef USE_ASIO
    // Boost.Asio 初始化
    for (int i = 0; i < num_threads; ++i) {
        thread_pool_.emplace_back([this] { io_context_.run(); });
    }
#else
    // 初始化线程池
    for (int i = 0; i < num_threads; ++i) {
        thread_pool_.emplace_back(&EventLoop::workerThread, this);
    }
#endif
}

EventLoop::~EventLoop() {
    stop();
    for (auto& thread : thread_pool_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
#ifdef __linux__
    close(epoll_fd_);
    if (signal_fd_ != -1) {
        close(signal_fd_);
    }
#elif _WIN32
    WSACleanup();
#endif
}

void EventLoop::run() {
    stop_flag_.store(false);
#ifndef USE_ASIO
    workerThread();
#endif
}

void EventLoop::workerThread() {
    // 使用线程本地存储来缓存任务
    thread_local std::vector<Task> local_tasks;
    thread_local int idle_count = 0;
    local_tasks.reserve(64);  // 预分配空间

    while (!stop_flag_.load(std::memory_order_relaxed)) {
        Task task;
        bool has_task = false;

        // 批量获取任务到本地队列
        {
            const int batch_size = 16;
            for (int i = 0; i < batch_size && task_queue_.pop(task); ++i) {
                auto current_time = std::chrono::steady_clock::now();
                if (task.execTime <= current_time && task.is_active) {
                    local_tasks.push_back(std::move(task));
                } else {
                    task_queue_.push(std::move(task));
                }
            }
        }

        // 执行本地任务
        for (auto& t : local_tasks) {
            if (t.is_active) {
                t.func();
            }
            // 如果是周期性任务，重新入队
            if (t.type == Task::Type::Periodic) {
                t.execTime +=
                    std::chrono::milliseconds(100);  // 假设100ms为周期
                task_queue_.push(std::move(t));
            }
        }
        local_tasks.clear();

        // 进行 IO 多路复用
#ifdef __linux__
        epoll_event events[16];
        int nfds = epoll_wait(epoll_fd_, events, 16, 1);
        if (nfds > 0) {
            for (int i = 0; i < nfds; ++i) {
                // 处理 IO 事件
                handleEpollEvent(events[i]);
            }
        }
#endif

        // 使用指数退避算法进行休眠
        if (!has_task) {
            static thread_local int idle_count = 0;
            if (++idle_count > 10) {
                std::this_thread::sleep_for(std::chrono::microseconds(
                    std::min(1000, idle_count * 100)));
            }
        } else {
            idle_count = 0;
        }
    }
}

// 添加事件处理辅助函数
#ifdef __linux__
void EventLoop::handleEpollEvent(const epoll_event& event) {
    if (event.data.fd == signal_fd_) {
        uint64_t sigVal;
        if (read(signal_fd_, &sigVal, sizeof(uint64_t)) > 0) {
            auto it = signal_handlers_.find(sigVal);
            if (it != signal_handlers_.end()) {
                post(0, it->second);
            }
        }
    }
}
#endif

void EventLoop::stop() {
    stop_flag_.store(true);
    wakeup();
}

auto EventLoop::adjustTaskPriority(int task_id, int new_priority) -> bool {
    std::unique_lock lock(queue_mutex_);
    std::priority_queue<Task> newQueue;

    bool found = false;
    while (!tasks_.empty()) {
        Task task = std::move(const_cast<Task&>(tasks_.top()));
        tasks_.pop();
        if (task.taskId == task_id) {
            task.priority = new_priority;
            found = true;
        }
        newQueue.push(std::move(task));
    }

    tasks_ = std::move(newQueue);
    return found;
}

void EventLoop::subscribeEvent(const std::string& event_name,
                               const EventCallback& callback) {
    std::unique_lock lock(queue_mutex_);
    event_subscribers_[event_name].push_back(callback);
}

void EventLoop::emitEvent(const std::string& event_name) {
    std::unique_lock lock(queue_mutex_);
    if (event_subscribers_.count(event_name)) {
        for (const auto& callback : event_subscribers_[event_name]) {
            post(callback);
        }
    }
}

#ifdef __linux__
void EventLoop::addSignalHandler(int signal, std::function<void()> handler) {
    std::unique_lock lock(queue_mutex_);
    signal_handlers_[signal] = std::move(handler);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signal);
    signal_fd_ = signalfd(-1, &mask, SFD_NONBLOCK);

    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = signal_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, signal_fd_, &ev);
}
#endif

void EventLoop::wakeup() {
#ifdef __linux__
    // Linux: 使用 eventfd 唤醒 epoll
    int eventFd = eventfd(0, EFD_NONBLOCK);
    epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = eventFd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, eventFd, &ev);
    uint64_t one = 1;
    write(eventFd, &one, sizeof(uint64_t));  // 触发事件
    close(eventFd);
#elif _WIN32
    // Windows: 利用 select 中的 socket 模拟唤醒机制
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    FD_SET(sock, &read_fds);
#endif
}

#ifdef __linux__
void EventLoop::addEpollFd(int fd) const {
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        ABORT_F("Failed to add fd to epoll");
    }
}
#elif _WIN32
void EventLoop::add_socket_fd(SOCKET fd) { FD_SET(fd, &read_fds); }
#endif

}  // namespace lithium::app
