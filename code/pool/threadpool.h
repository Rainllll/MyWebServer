#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <assert.h>


class ThreadPool {
public:
    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    // 尽量用make_shared代替new，如果通过new再传递给shared_ptr，内存是不连续的，会造成内存碎片化
    explicit ThreadPool(int threadCount = 8) : pool_(std::make_shared<Pool>()) { // make_shared:传递右值，功能是在动态内存中分配一个对象并初始化它，返回指向此对象的shared_ptr
        assert(threadCount > 0);
        for(int i = 0; i < threadCount; i++) {
            std::thread([this]() {
                std::unique_lock<std::mutex> locker(pool_->mtx_);
                while(true) {
                    if(!pool_->tasks.empty()) {
                        auto task = std::move(pool_->tasks.front());    // 左值变右值,资产转移
                        pool_->tasks.pop();
                        locker.unlock();    // 因为已经把任务取出来了，所以可以提前解锁了
                        task();
                        locker.lock();      // 马上又要取任务了，上锁
                    } else if(pool_->isClosed) {
                        break;
                    } else {
                        pool_->cond_.wait(locker);    // 等待,如果任务来了就notify的
                    }
                    
                }
            }).detach();
        }
    }

    ~ThreadPool() {
        // 检查 pool_ 是否有效（即是否指向一个有效的 Pool 对象）
        if(pool_) {
            // 加锁以确保线程安全，防止在修改 isClosed 标志时发生数据竞争
            std::unique_lock<std::mutex> locker(pool_->mtx_);
            // 将 isClosed 标志设置为 true，表示线程池已关闭
            pool_->isClosed = true;
        }
        // 唤醒所有正在等待的线程，让它们检查 isClosed 标志并退出循环
        pool_->cond_.notify_all();
    }

    template<typename T>
    /**
     * @brief 向线程池的任务队列中添加一个任务
     * 
     * @tparam T 任务的类型
     * @param task 要添加的任务，使用右值引用以支持移动语义
     */
    void AddTask(T&& task) {
        // 加锁以确保线程安全，防止在修改任务队列时发生数据竞争
        std::unique_lock<std::mutex> locker(pool_->mtx_);
        // 使用 std::forward 完美转发任务到任务队列中
        pool_->tasks.emplace(std::forward<T>(task));
        // 唤醒一个正在等待的线程来处理新添加的任务
        pool_->cond_.notify_one();
    }

private:
    // 用一个结构体封装起来，方便调用
    struct Pool {
        std::mutex mtx_;
        std::condition_variable cond_;
        bool isClosed;
        std::queue<std::function<void()>> tasks; // 任务队列，函数类型为void()
    };
    std::shared_ptr<Pool> pool_;
};

#endif