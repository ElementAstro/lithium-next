#ifndef EVENT_LOOP_HPP
#define EVENT_LOOP_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

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
 * @brief High-performance event loop for managing asynchronous tasks and
 * events.
 *
 * This class provides a thread-safe, priority-based task scheduling system
 * with support for delayed execution, periodic tasks, event handling, and
 * platform-specific I/O multiplexing.
 */
class EventLoop {
public:
    using EventCallback = std::function<void()>;

    /**
     * @brief Constructs an EventLoop instance with specified thread count.
     *
     * @param thread_count Number of worker threads in the thread pool (default:
     * 1)
     */
    explicit EventLoop(int thread_count = 1);

    /**
     * @brief Destructor that safely shuts down the event loop and joins all
     * threads.
     */
    ~EventLoop();

    /**
     * @brief Starts the event loop processing.
     */
    void run();

    /**
     * @brief Stops the event loop and signals all worker threads to terminate.
     */
    void stop();

    /**
     * @brief Posts a task with specified priority to the event loop.
     *
     * @tparam Function Type of the callable object
     * @tparam Arguments Variadic template for function arguments
     * @param priority Task priority (higher values = higher priority)
     * @param function Callable object to execute
     * @param arguments Arguments to pass to the function
     * @return Future representing the task result
     */
    template <typename Function, typename... Arguments>
    auto post(int priority, Function&& function, Arguments&&... arguments)
        -> std::future<std::invoke_result_t<Function, Arguments...>>;

    /**
     * @brief Posts a task with default priority (0) to the event loop.
     *
     * @tparam Function Type of the callable object
     * @tparam Arguments Variadic template for function arguments
     * @param function Callable object to execute
     * @param arguments Arguments to pass to the function
     * @return Future representing the task result
     */
    template <typename Function, typename... Arguments>
    auto post(Function&& function, Arguments&&... arguments)
        -> std::future<std::invoke_result_t<Function, Arguments...>>;

    /**
     * @brief Posts a delayed task with specified priority.
     *
     * @tparam Function Type of the callable object
     * @tparam Arguments Variadic template for function arguments
     * @param delay Execution delay in milliseconds
     * @param priority Task priority
     * @param function Callable object to execute
     * @param arguments Arguments to pass to the function
     * @return Future representing the task result
     */
    template <typename Function, typename... Arguments>
    auto postDelayed(std::chrono::milliseconds delay, int priority,
                     Function&& function, Arguments&&... arguments)
        -> std::future<std::invoke_result_t<Function, Arguments...>>;

    /**
     * @brief Posts a delayed task with default priority.
     *
     * @tparam Function Type of the callable object
     * @tparam Arguments Variadic template for function arguments
     * @param delay Execution delay in milliseconds
     * @param function Callable object to execute
     * @param arguments Arguments to pass to the function
     * @return Future representing the task result
     */
    template <typename Function, typename... Arguments>
    auto postDelayed(std::chrono::milliseconds delay, Function&& function,
                     Arguments&&... arguments)
        -> std::future<std::invoke_result_t<Function, Arguments...>>;

    /**
     * @brief Dynamically adjusts the priority of an existing task.
     *
     * @param task_id Unique identifier of the task
     * @param new_priority New priority value for the task
     * @return True if task was found and priority adjusted, false otherwise
     */
    auto adjustTaskPriority(int task_id, int new_priority) -> bool;

    /**
     * @brief Posts a task that depends on completion of another task.
     *
     * @tparam Function Type of the main function
     * @tparam DependencyFunction Type of the dependency function
     * @param function Main function to execute after dependency
     * @param dependency_task Task that must complete first
     */
    template <typename Function, typename DependencyFunction>
    void postWithDependency(Function&& function,
                            DependencyFunction&& dependency_task);

    /**
     * @brief Schedules a task to execute periodically at specified intervals.
     *
     * @param interval Time interval between executions
     * @param priority Task priority for each execution
     * @param function Function to execute periodically
     */
    void schedulePeriodic(std::chrono::milliseconds interval, int priority,
                          std::function<void()> function);

    /**
     * @brief Posts a task that can be cancelled via atomic flag.
     *
     * @tparam Function Type of the callable object
     * @tparam Arguments Variadic template for function arguments
     * @param function Callable object to execute
     * @param cancel_flag Atomic boolean flag for cancellation control
     * @return Future for task completion
     */
    template <typename Function, typename... Arguments>
    auto postCancelable(Function&& function, std::atomic<bool>& cancel_flag)
        -> std::future<void>;

    /**
     * @brief Executes a function after specified timeout delay.
     *
     * @param function Function to execute after timeout
     * @param delay Timeout delay in milliseconds
     */
    void setTimeout(std::function<void()> function,
                    std::chrono::milliseconds delay);

    /**
     * @brief Executes a function repeatedly at specified intervals.
     *
     * @param function Function to execute at each interval
     * @param interval Time interval between executions
     */
    void setInterval(std::function<void()> function,
                     std::chrono::milliseconds interval);

    /**
     * @brief Subscribes a callback to be executed when specific event is
     * emitted.
     *
     * @param event_name Name of the event to subscribe to
     * @param callback Function to execute when event occurs
     */
    void subscribeEvent(const std::string& event_name,
                        const EventCallback& callback);

    /**
     * @brief Emits an event, triggering all subscribed callbacks.
     *
     * @param event_name Name of the event to emit
     */
    void emitEvent(const std::string& event_name);

#ifdef __linux__
    /**
     * @brief Adds file descriptor to Linux epoll instance for I/O monitoring.
     *
     * @param file_descriptor File descriptor to monitor for I/O events
     */
    void addEpollFileDescriptor(int file_descriptor) const;

    /**
     * @brief Registers signal handler for Linux signal processing.
     *
     * @param signal_number Signal number to handle
     * @param handler Function to execute when signal is received
     */
    void addSignalHandler(int signal_number, std::function<void()> handler);
#elif _WIN32
    /**
     * @brief Adds socket file descriptor for Windows I/O monitoring.
     *
     * @param socket_fd Socket file descriptor to monitor
     */
    void addSocketFileDescriptor(SOCKET socket_fd);
#endif

private:
    /**
     * @brief Internal task representation with priority and scheduling
     * information.
     */
    struct Task {
        std::function<void()> function;
        int priority;
        std::chrono::steady_clock::time_point execution_time;
        int task_id;

        /**
         * @brief Task priority comparison operator for priority queue ordering.
         *
         * @param other Task to compare against
         * @return True if this task has lower priority or later execution time
         */
        auto operator<(const Task& other) const -> bool;

        Task* next_pooled = nullptr;
        bool is_active = true;

        /**
         * @brief Task type enumeration for different scheduling behaviors.
         */
        enum class Type {
            Normal,     ///< Standard one-time execution task
            Delayed,    ///< Task with delayed execution
            Periodic,   ///< Repeating task with fixed interval
            Cancelable  ///< Task that can be cancelled
        } type = Type::Normal;

        static void* operator new(size_t size) { return task_pool_.allocate(); }

        static void operator delete(void* ptr) {
            task_pool_.deallocate(static_cast<Task*>(ptr));
        }
    };

    /**
     * @brief Memory pool for efficient task allocation and deallocation.
     */
    class TaskPool {
    public:
        /**
         * @brief Allocates a task object from the pool or creates new one.
         * @return Pointer to allocated task object
         */
        Task* allocate() {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            if (free_list_ == nullptr) {
                return new Task();
            }
            Task* task = free_list_;
            free_list_ = free_list_->next_pooled;
            return task;
        }

        /**
         * @brief Returns task object to the pool for reuse.
         * @param task Task object to deallocate
         */
        void deallocate(Task* task) {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            task->next_pooled = free_list_;
            free_list_ = task;
        }

    private:
        Task* free_list_ = nullptr;
        std::mutex pool_mutex_;
    };

    static TaskPool task_pool_;

    /**
     * @brief Multi-priority task queue for efficient task scheduling.
     */
    struct TaskQueue {
        std::array<std::priority_queue<Task>, 3> priority_queues;
        std::mutex mutex;

        /**
         * @brief Adds task to appropriate priority queue.
         * @param task Task to add to queue
         */
        void push(Task&& task) {
            std::lock_guard<std::mutex> lock(mutex);
            int queue_index =
                task.priority > 0 ? 0 : (task.priority < 0 ? 2 : 1);
            priority_queues[queue_index].push(std::move(task));
        }

        /**
         * @brief Retrieves highest priority task from queues.
         * @param task Reference to store retrieved task
         * @return True if task was retrieved, false if all queues empty
         */
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
    std::priority_queue<Task> legacy_tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_flag_;
    std::vector<std::jthread> thread_pool_;

    std::unordered_map<std::string, std::vector<EventCallback>>
        event_subscribers_;
    std::unordered_map<int, std::function<void()>> signal_handlers_;
    std::atomic<int> next_task_id_;

#ifdef USE_ASIO
    boost::asio::io_context io_context_;
    std::vector<std::unique_ptr<boost::asio::steady_timer>> timers_;
#endif

#ifdef __linux__
    int epoll_fd_;
    std::vector<struct epoll_event> epoll_events_;
    int signal_fd_;

    /**
     * @brief Handles Linux epoll I/O events.
     * @param event Epoll event structure containing event information
     */
    void handleEpollEvent(const epoll_event& event);
#elif _WIN32
    fd_set read_fds_;
#endif

    /**
     * @brief Main worker thread function for processing tasks and I/O events.
     */
    void workerThread();

    /**
     * @brief Wakes up the event loop from blocking I/O operations.
     */
    void wakeup();
};

// Template Implementation

template <typename Function, typename... Arguments>
auto EventLoop::post(int priority, Function&& function,
                     Arguments&&... arguments)
    -> std::future<std::invoke_result_t<Function, Arguments...>> {
    using return_type = std::invoke_result_t<Function, Arguments...>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<Function>(function),
                  std::forward<Arguments>(arguments)...));
    std::future<return_type> result = task->get_future();

    {
        std::unique_lock lock(queue_mutex_);
        legacy_tasks_.emplace(Task{[task]() { (*task)(); }, priority,
                                   std::chrono::steady_clock::now(),
                                   next_task_id_.fetch_add(1)});
    }

#ifdef USE_ASIO
    io_context_.post([task]() { (*task)(); });
#else
    condition_.notify_one();
#endif
    return result;
}

template <typename Function, typename... Arguments>
auto EventLoop::post(Function&& function, Arguments&&... arguments)
    -> std::future<std::invoke_result_t<Function, Arguments...>> {
    return post(0, std::forward<Function>(function),
                std::forward<Arguments>(arguments)...);
}

template <typename Function, typename... Arguments>
auto EventLoop::postDelayed(std::chrono::milliseconds delay, int priority,
                            Function&& function, Arguments&&... arguments)
    -> std::future<std::invoke_result_t<Function, Arguments...>> {
    using return_type = std::invoke_result_t<Function, Arguments...>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<Function>(function),
                  std::forward<Arguments>(arguments)...));
    std::future<return_type> result = task->get_future();
    auto execution_time = std::chrono::steady_clock::now() + delay;

#ifdef USE_ASIO
    auto timer =
        std::make_unique<boost::asio::steady_timer>(io_context_, delay);
    timers_.emplace_back(std::move(timer));
    timers_.back()->async_wait(
        [task](const boost::system::error_code& error_code) {
            if (!error_code) {
                (*task)();
            }
        });
#else
    {
        std::unique_lock lock(queue_mutex_);
        legacy_tasks_.emplace(Task{[task]() { (*task)(); }, priority,
                                   execution_time, next_task_id_.fetch_add(1)});
    }
    condition_.notify_one();
#endif
    return result;
}

template <typename Function, typename... Arguments>
auto EventLoop::postDelayed(std::chrono::milliseconds delay,
                            Function&& function, Arguments&&... arguments)
    -> std::future<std::invoke_result_t<Function, Arguments...>> {
    return postDelayed(delay, 0, std::forward<Function>(function),
                       std::forward<Arguments>(arguments)...);
}

template <typename Function, typename DependencyFunction>
void EventLoop::postWithDependency(Function&& function,
                                   DependencyFunction&& dependency_task) {
    std::future<void> dependency = dependency_task.get_future();
    std::thread([this, f = std::forward<Function>(function),
                 dependency = std::move(dependency)]() mutable {
        dependency.wait();
        post(std::move(f));
    }).detach();
}

template <typename Function, typename... Arguments>
auto EventLoop::postCancelable(Function&& function,
                               std::atomic<bool>& cancel_flag)
    -> std::future<void> {
    using return_type = std::invoke_result_t<Function, Arguments...>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<Function>(function)));
    std::future<return_type> result = task->get_future();

    {
        std::unique_lock lock(queue_mutex_);
        legacy_tasks_.emplace(Task{[task, &cancel_flag]() {
                                       if (!cancel_flag.load()) {
                                           (*task)();
                                       }
                                   },
                                   0, std::chrono::steady_clock::now(),
                                   next_task_id_.fetch_add(1)});
    }
    condition_.notify_one();
    return result;
}

}  // namespace lithium::app

#endif  // EVENT_LOOP_HPP
