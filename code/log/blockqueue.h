/**
 * @file blockqueue.h
 * @brief 定义了一个线程安全的阻塞队列模板类 BlockQueue。
 */

#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <deque>
#include <condition_variable>
#include <mutex>
#include <sys/time.h>
using namespace std;

/**
 * @brief 线程安全的阻塞队列模板类。
 * 
 * @tparam T 队列中存储的元素类型。
 */
template<typename T>
class BlockQueue {
public:
    /**
     * @brief 构造函数，初始化阻塞队列。
     * 
     * @param maxsize 队列的最大容量，默认为 1000。
     */
    explicit BlockQueue(size_t maxsize = 1000);
    /**
     * @brief 析构函数，关闭队列并清空元素。
     */
    ~BlockQueue();
    /**
     * @brief 检查队列是否为空。
     * 
     * @return 如果队列为空返回 true，否则返回 false。
     */
    bool empty();
    /**
     * @brief 检查队列是否已满。
     * 
     * @return 如果队列已满返回 true，否则返回 false。
     */
    bool full();
    /**
     * @brief 向队列尾部添加一个元素。
     * 
     * @param item 要添加的元素。
     */
    void push_back(const T& item);
    /**
     * @brief 向队列头部添加一个元素。
     * 
     * @param item 要添加的元素。
     */
    void push_front(const T& item); 
    /**
     * @brief 从队列头部弹出一个元素。
     * 
     * @param item 用于存储弹出的元素。
     * @return 如果成功弹出元素返回 true，否则返回 false。
     */
    bool pop(T& item);  // 弹出的任务放入item
    /**
     * @brief 从队列头部弹出一个元素，带有超时时间。
     * 
     * @param item 用于存储弹出的元素。
     * @param timeout 等待的超时时间（秒）。
     * @return 如果成功弹出元素返回 true，超时或队列关闭返回 false。
     */
    bool pop(T& item, int timeout);  // 等待时间
    /**
     * @brief 清空队列中的所有元素。
     */
    void clear();
    /**
     * @brief 获取队列头部的元素。
     * 
     * @return 队列头部的元素。
     */
    T front();
    /**
     * @brief 获取队列尾部的元素。
     * 
     * @return 队列尾部的元素。
     */
    T back();
    /**
     * @brief 获取队列的最大容量。
     * 
     * @return 队列的最大容量。
     */
    size_t capacity();
    /**
     * @brief 获取队列当前的元素数量。
     * 
     * @return 队列当前的元素数量。
     */
    size_t size();

    /**
     * @brief 唤醒一个等待的消费者线程。
     */
    void flush();
    /**
     * @brief 关闭队列，清空元素并通知所有等待的线程。
     */
    void Close();

private:
    deque<T> deq_;                      // 底层数据结构
    mutex mtx_;                         // 锁
    bool isClose_;                      // 关闭标志
    size_t capacity_;                   // 容量
    condition_variable condConsumer_;   // 消费者条件变量
    condition_variable condProducer_;   // 生产者条件变量
};

/**
 * @brief 构造函数的实现，初始化队列的最大容量和关闭标志。
 * 
 * @param maxsize 队列的最大容量。
 */
template<typename T>
BlockQueue<T>::BlockQueue(size_t maxsize) : capacity_(maxsize) {
    assert(maxsize > 0);
    isClose_ = false;
}

/**
 * @brief 析构函数的实现，调用 Close 方法关闭队列。
 */
template<typename T>
BlockQueue<T>::~BlockQueue() {
    Close();
}

/**
 * @brief 关闭队列的实现，清空队列并通知所有等待的线程。
 */
template<typename T>
void BlockQueue<T>::Close() {
    // lock_guard<mutex> locker(mtx_); // 操控队列之前，都需要上锁
    // deq_.clear();                   // 清空队列
    clear();
    isClose_ = true;
    condConsumer_.notify_all();
    condProducer_.notify_all();
}

/**
 * @brief 清空队列的实现，加锁后清空队列。
 */
template<typename T>
void BlockQueue<T>::clear() {
    lock_guard<mutex> locker(mtx_);
    deq_.clear();
}

/**
 * @brief 检查队列是否为空的实现，加锁后检查队列是否为空。
 * 
 * @return 如果队列为空返回 true，否则返回 false。
 */
template<typename T>
bool BlockQueue<T>::empty() {
    lock_guard<mutex> locker(mtx_);
    return deq_.empty();
}

/**
 * @brief 检查队列是否已满的实现，加锁后检查队列的元素数量是否达到最大容量。
 * 
 * @return 如果队列已满返回 true，否则返回 false。
 */
template<typename T>
bool BlockQueue<T>::full() {
    lock_guard<mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

/**
 * @brief 向队列尾部添加元素的实现，使用条件变量处理队列满的情况。
 * 
 * @param item 要添加的元素。
 */
template<typename T>
void BlockQueue<T>::push_back(const T& item) {
    // 注意，条件变量需要搭配unique_lock
    unique_lock<mutex> locker(mtx_);    
    while(deq_.size() >= capacity_) {   // 队列满了，需要等待
        condProducer_.wait(locker);     // 暂停生产，等待消费者唤醒生产条件变量
    }
    deq_.push_back(item);
    condConsumer_.notify_one();         // 唤醒消费者
}

/**
 * @brief 向队列头部添加元素的实现，使用条件变量处理队列满的情况。
 * 
 * @param item 要添加的元素。
 */
template<typename T>
void BlockQueue<T>::push_front(const T& item) {
    unique_lock<mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {   // 队列满了，需要等待
        condProducer_.wait(locker);     // 暂停生产，等待消费者唤醒生产条件变量
    }
    deq_.push_front(item);
    condConsumer_.notify_one();         // 唤醒消费者
}

/**
 * @brief 从队列头部弹出元素的实现，使用条件变量处理队列空的情况。
 * 
 * @param item 用于存储弹出的元素。
 * @return 如果成功弹出元素返回 true，否则返回 false。
 */
template<typename T>
bool BlockQueue<T>::pop(T& item) {
    unique_lock<mutex> locker(mtx_);
    while(deq_.empty()) {
        condConsumer_.wait(locker);     // 队列空了，需要等待
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();         // 唤醒生产者
    return true;
}

/**
 * @brief 从队列头部弹出元素并带有超时时间的实现，使用条件变量处理队列空的情况。
 * 
 * @param item 用于存储弹出的元素。
 * @param timeout 等待的超时时间（秒）。
 * @return 如果成功弹出元素返回 true，超时或队列关闭返回 false。
 */
template<typename T>
bool BlockQueue<T>::pop(T &item, int timeout) {
    unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){
            return false;
        }
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

/**
 * @brief 获取队列头部元素的实现，加锁后返回队列头部的元素。
 * 
 * @return 队列头部的元素。
 */
template<typename T>
T BlockQueue<T>::front() {
    lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}
//返回队尾元素
template<typename T>
T BlockQueue<T>::back() {
    lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}
//返回队列的最大容量
template<typename T>
size_t BlockQueue<T>::capacity() {
    lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}
//返回队列的当前元素数量
template<typename T>
size_t BlockQueue<T>::size() {
    lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

// 唤醒消费者
template<typename T>
void BlockQueue<T>::flush() {
    condConsumer_.notify_one();
}
# endif