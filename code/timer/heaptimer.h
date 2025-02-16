#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../log/log.h"

// 定义一个函数对象类型，用于表示超时回调函数
typedef std::function<void()> TimeoutCallBack;
// 定义一个高精度时钟类型
typedef std::chrono::high_resolution_clock Clock;
// 定义一个毫秒时间单位类型
typedef std::chrono::milliseconds MS;
// 定义一个时间戳类型
typedef Clock::time_point TimeStamp;

// 定义一个结构体，表示定时器节点
struct TimerNode {
    int id; // 定时器节点的唯一标识符
    TimeStamp expires; // 定时器的过期时间点
    TimeoutCallBack cb; // 定时器的超时回调函数

    // 重载小于运算符，用于比较两个定时器节点的过期时间
    bool operator<(const TimerNode& t) {
        return expires < t.expires;
    }

    // 重载大于运算符，用于比较两个定时器节点的过期时间
    bool operator>(const TimerNode& t) {
        return expires > t.expires;
    }
};

// 定义一个类，表示基于堆的定时器
class HeapTimer {
public:
    // 默认构造函数，初始化堆的容量为64
    HeapTimer() { heap_.reserve(64); }
    // 析构函数，清空定时器
    ~HeapTimer() { clear(); }

    // 调整指定id的定时器的过期时间
    void adjust(int id, int newExpires);
    // 添加一个新的定时器
    void add(int id, int timeOut, const TimeoutCallBack& cb);
    // 执行指定id的定时器的回调函数，并从堆中删除该定时器
    void doWork(int id);
    // 清空所有定时器
    void clear();
    // 检查并执行所有已过期的定时器的回调函数，并从堆中删除这些定时器
    void tick();
    // 删除堆顶的定时器（即最早过期的定时器）
    void pop();
    // 获取下一个定时器的过期时间与当前时间的差值
    int GetNextTick();

private:
    // 删除指定索引位置的定时器节点
    void del_(size_t i);
    // 将指定索引位置的定时器节点上移，以维护堆的性质
    void siftup_(size_t i);
    // 将指定索引位置的定时器节点下移，以维护堆的性质
    bool siftdown_(size_t i, size_t n);
    // 交换两个定时器节点的位置
    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_; // 存储定时器节点的堆
    // 存储定时器节点id到堆中索引的映射，方便快速查找和操作
    std::unordered_map<int, size_t> ref_;
};

#endif //HEAP_TIMER_H