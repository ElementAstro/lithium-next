#ifndef EVENT_LOOP_HPP
#define EVENT_LOOP_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef USE_ASIO
#include <boost/asio.hpp>
#endif

#ifdef _WIN32
#include <winsock2.h>
#elif __linux__
#include <sys/epoll.h>
#include <sys/signalfd.h>
#endif

namespace lithium::app {

/**
 * @brief Event loop class for managing asynchronous tasks and events.
 */
class EventLoop {
public:
    using EventCallback = std::function<void()>;
    /**
     * @brief Constructs an EventLoop instance.
     *
     * @param num_threads Number of threads to use in the thread pool.
     */
    explicit EventLoop(int num_threads = 1);

    /**
     * @brief Destructs the EventLoop instance.
     */
    ~EventLoop();

    /**
     * @brief Starts the event loop.
     */
    void run();

    /**
     * @brief Stops the event loop.
     */
    void stop();

    /**
     * @brief Posts a task to the event loop with a specified priority.
     *
     * @tparam F The type of the function to post.
     * @tparam Args The types of the arguments to pass to the function.
     * @param priority The priority of the task.
     * @param f The function to post.
     * @param args The arguments to pass to the function.
     * @return A future representing the result of the task.
     */
    template <typename F, typename... Args>
    auto post(int priority, F&& f,
              Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

    /**
     * @brief Posts a task to the event loop with default priority.
     *
     * @tparam F The type of the function to post.
     * @tparam Args The types of the arguments to pass to the function.
     * @param f The function to post.
     * @param args The arguments to pass to the function.
     * @return A future representing the result of the task.
     */
    template <typename F, typename... Args>
    auto post(F&& f,
              Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

    /**
     * @brief Posts a delayed task to the event loop with a specified priority.
     *
     * @tparam F The type of the function to post.
     * @tparam Args The types of the arguments to pass to the function.
     * @param delay The delay before the task is executed.
     * @param priority The priority of the task.
     * @param f The function to post.
     * @param args The arguments to pass to the function.
     * @return A future representing the result of the task.
     */
    template <typename F, typename... Args>
    auto postDelayed(std::chrono::milliseconds delay, int priority, F&& f,
                     Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    /**
     * @brief Posts a delayed task to the event loop with default priority.
     *
     * @tparam F The type of the function to post.
     * @tparam Args The types of the arguments to pass to the function.
     * @param delay The delay before the task is executed.
     * @param f The function to post.
     * @param args The arguments to pass to the function.
     * @return A future representing the result of the task.
     */
    template <typename F, typename... Args>
    auto postDelayed(std::chrono::milliseconds delay, F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    /**
     * @brief Adjusts the priority of a task.
     *
     * @param task_id The ID of the task to adjust.
     * @param new_priority The new priority of the task.
     * @return True if the task priority was adjusted, false otherwise.
     */
    auto adjustTaskPriority(int task_id, int new_priority) -> bool;

    /**
     * @brief Posts a task with a dependency on another task.
     *
     * @tparam F The type of the function to post.
     * @tparam G The type of the dependency task.
     * @param f The function to post.
     * @param dependency_task The dependency task.
     */
    template <typename F, typename G>
    void postWithDependency(F&& f, G&& dependency_task);

    /**
     * @brief Schedules a periodic task.
     *
     * @param interval The interval between task executions.
     * @param priority The priority of the task.
     * @param func The function to execute periodically.
     */
    void schedulePeriodic(std::chrono::milliseconds interval, int priority,
                          std::function<void()> func);

    /**
     * @brief Posts a cancelable task.
     *
     * @tparam F The type of the function to post.
     * @tparam Args The types of the arguments to pass to the function.
     * @param f The function to post.
     * @param cancel_flag The atomic flag to check for cancellation.
     * @return A future representing the result of the task.
     */
    template <typename F, typename... Args>
    auto postCancelable(F&& f,
                        std::atomic<bool>& cancel_flag) -> std::future<void>;

    /**
     * @brief Sets a timeout for a function.
     *
     * @param func The function to execute after the timeout.
     * @param delay The delay before the function is executed.
     */
    void setTimeout(std::function<void()> func,
                    std::chrono::milliseconds delay);

    /**
     * @brief Sets an interval for a function.
     *
     * @param func The function to execute periodically.
     * @param interval The interval between function executions.
     */
    void setInterval(std::function<void()> func,
                     std::chrono::milliseconds interval);

    /**
     * @brief Subscribes to an event.
     *
     * @param event_name The name of the event to subscribe to.
     * @param callback The callback function to execute when the event is
     * emitted.
     */
    void subscribeEvent(const std::string& event_name,
                        const EventCallback& callback);

    /**
     * @brief Emits an event.
     *
     * @param event_name The name of the event to emit.
     */
    void emitEvent(const std::string& event_name);

#ifdef __linux__
    /**
     * @brief Adds a file descriptor to the epoll instance.
     *
     * @param fd The file descriptor to add.
     */
    void addEpollFd(int fd) const;

    /**
     * @brief Adds a signal handler.
     *
     * @param signal The signal to handle.
     * @param handler The handler function to execute when the signal is
     * received.
     */
    void addSignalHandler(int signal, std::function<void()> handler);
#elif _WIN32
    /**
     * @brief Adds a socket file descriptor.
     *
     * @param fd The socket file descriptor to add.
     */
    void add_socket_fd(SOCKET fd);
#endif

private:
    struct Task {
        std::function<void()> func;
        int priority;
        std::chrono::steady_clock::time_point execTime;
        int taskId;

        /**
         * @brief Compares two tasks based on their priority and execution time.
         *
         * @param other The other task to compare to.
         * @return True if this task has lower priority or later execution time,
         * false otherwise.
         */
        auto operator<(const Task& other) const -> bool;

        // 添加任务池相关字段
        Task* next_pooled = nullptr;
        bool is_active = true;

        // 添加任务类型标识
        enum class Type {
            Normal,
            Delayed,
            Periodic,
            Cancelable
        } type = Type::Normal;

        // 对象池分配器
        static void* operator new(size_t size) {
            return task_pool.allocate();
        }

        static void operator delete(void* ptr) {
            task_pool.deallocate(static_cast<Task*>(ptr));
        }
    };

    // 添加任务对象池
    class TaskPool {
    public:
        Task* allocate() {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            if (free_list_ == nullptr) {
                return new Task();
            }
            Task* task = free_list_;
            free_list_ = free_list_->next_pooled;
            return task;
        }

        void deallocate(Task* task) {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            task->next_pooled = free_list_;
            free_list_ = task;
        }

    private:
        Task* free_list_ = nullptr;
        std::mutex pool_mutex_;
    };

    static TaskPool task_pool;

    // 添加高性能任务队列
    struct TaskQueue {
        std::array<std::priority_queue<Task>, 3> priority_queues;  // 高中低三个优先级队列
        std::mutex mutex;

        void push(Task&& task) {
            std::lock_guard<std::mutex> lock(mutex);
            int queue_index = task.priority > 0 ? 0 : (task.priority < 0 ? 2 : 1);
            priority_queues[queue_index].push(std::move(task));
        }

        bool pop(Task& task) {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto& queue : priority_queues) {
                if (!queue.empty()) {
                    task = std::move(const_cast<Task&>(queue.top()));
                    queue.pop();
                    return true;
                }
            }
            return false;
        }
    };

    TaskQueue task_queue_;

    std::priority_queue<Task> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_flag_;
    std::vector<std::jthread> thread_pool_;

    std::unordered_map<std::string, std::vector<EventCallback>>
        event_subscribers_;
    std::unordered_map<int, std::function<void()>> signal_handlers_;
    int next_task_id_ = 0;

#ifdef USE_ASIO
    boost::asio::io_context io_context_;
    std::vector<std::unique_ptr<boost::asio::steady_timer>> timers_;
#endif

#ifdef __linux__
    int epoll_fd_;
    std::vector<struct epoll_event> epoll_events_;
    int signal_fd_;

    void handleEpollEvent(const epoll_event& event);
#elif _WIN32
    fd_set read_fds;
#endif

    /**
     * @brief Worker thread function for processing tasks.
     */
    void workerThread();

    /**
     * @brief Wakes up the event loop.
     */
    void wakeup();
};

template <typename F, typename... Args>
auto EventLoop::post(int priority, F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {
    using return_type = std::invoke_result_t<F, Args...>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> result = task->get_future();
    {
        std::unique_lock lock(queue_mutex_);
        tasks_.emplace(Task{[task]() { (*task)(); }, priority,
                            std::chrono::steady_clock::now(), next_task_id_++});
    }
#ifdef USE_ASIO
    io_context_.post(task { (*task)(); });
#else
    condition_.notify_one();
#endif
    return result;
}

template <typename F, typename... Args>
auto EventLoop::post(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {
    return post(0, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename F, typename... Args>
auto EventLoop::postDelayed(std::chrono::milliseconds delay, int priority,
                            F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {
    using return_type = std::invoke_result_t<F, Args...>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> result = task->get_future();
    auto execTime = std::chrono::steady_clock::now() + delay;
#ifdef USE_ASIO
    auto timer =
        std::make_unique<boost::asio::steady_timer>(io_context_, delay);
    timers_.emplace_back(std::move(timer));
    timers_.back()->async_wait([task](const boost::system::error_code& ec) {
        if (!ec) {
            (*task)();
        }
    });
#else
    {
        std::unique_lock lock(queue_mutex_);
        tasks_.emplace(
            Task{[task]() { (*task)(); }, priority, execTime, next_task_id_++});
    }
    condition_.notify_one();
#endif
    return result;
}

template <typename F, typename... Args>
auto EventLoop::postDelayed(std::chrono::milliseconds delay, F&& f,
                            Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {
    return postDelayed(delay, 0, std::forward<F>(f),
                       std::forward<Args>(args)...);
}

template <typename F, typename G>
void EventLoop::postWithDependency(F&& f, G&& dependency_task) {
    std::future<void> dependency = dependency_task.get_future();
    std::thread([this, f = std::forward<F>(f),
                 dependency = std::move(dependency)]() mutable {
        dependency.wait();
        post(std::move(f));
    }).detach();
}

template <typename F, typename... Args>
auto EventLoop::postCancelable(F&& f, std::atomic<bool>& cancel_flag)
    -> std::future<void> {
    using return_type = std::invoke_result_t<F, Args...>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f)));
    std::future<return_type> result = task->get_future();
    {
        std::unique_lock lock(queue_mutex_);
        tasks_.emplace(Task{[task, &cancel_flag]() {
                                if (!cancel_flag.load()) {
                                    (*task)();
                                }
                            },
                            0, std::chrono::steady_clock::now(),
                            next_task_id_++});
    }
    condition_.notify_one();
    return result;
}
}  // namespace lithium::app

#endif  // EVENT_LOOP_HPP
