#pragma once
#include <future>
#include <queue>
#include <functional>
#include <vector>
#include <thread>

class ThreadPool
{
public:
    // Explicit to prevent weird conversions
    explicit ThreadPool(size_t thread_count)
    {
        for (size_t i = 0; i < thread_count; i++)
        {
            workers_.emplace_back([this]()
                                  { worker_loop(); });
        }
    }

    ~ThreadPool()
    {
        shutdown();
    }

    // Prevent Copy
    ThreadPool(const ThreadPool &) = delete;

    template <typename F, typename... Args>
    auto enqueue(F &&f, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>>
    {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            // Forwarding needed for lvalue vs rvalue (location value vs temporary value)
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_)
                throw std::runtime_error("enqueue on stopped ThreadPool");

            tasks_.emplace([task]()
                           { (*task)(); });
        }

        condition_.notify_one();
        return result;
    }

    void shutdown()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }

        // Notify Workers to prevent hanging
        condition_.notify_all();

        for (auto &worker : workers_)
        {
            if (worker.joinable())
                worker.join();
        }
    }

    size_t size() const noexcept
    {
        return workers_.size();
    }

private:
    std::vector<std::thread> workers_;

    std::queue<std::function<void()>> tasks_;

    std::mutex queue_mutex_;
    std::condition_variable condition_;

    // Force Shutdown
    std::atomic<bool> stop_{false};

    void worker_loop()
    {
        while (true)
        {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this]
                                { return stop_ || !tasks_.empty(); });

                if (stop_ && tasks_.empty())
                    return;

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            // Run Task
            task();
        }
    }
};