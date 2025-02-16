#include "heaptimer.h"

void HeapTimer::SwapNode_(size_t i, size_t j) {
    // 确保索引 i 和 j 在堆的有效范围内
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    // 交换堆中索引为 i 和 j 的节点
    swap(heap_[i], heap_[j]);
    // 更新节点内部 id 所在索引位置
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
}

void HeapTimer::siftup_(size_t i) {
    // 确保索引 i 在堆的有效范围内
    assert(i >= 0 && i < heap_.size());
    // 计算父节点的索引
    size_t parent = (i-1) / 2;
    // 当父节点存在时
    while(parent >= 0) {
        // 如果父节点的过期时间大于当前节点的过期时间
        if(heap_[parent] > heap_[i]) {
            // 交换当前节点和父节点
            SwapNode_(i, parent);
            // 更新当前节点的索引为父节点的索引
            i = parent;
            // 重新计算父节点的索引
            parent = (i-1)/2;
        } else {
            // 如果父节点的过期时间不大于当前节点的过期时间，说明已经满足堆的性质，跳出循环
            break;
        }
    }
}

// false：不需要下滑  true：下滑成功
bool HeapTimer::siftdown_(size_t i, size_t n) {
    // 确保索引 i 在堆的有效范围内
    assert(i >= 0 && i < heap_.size());
    // 确保索引 n 在堆的有效范围内
    assert(n >= 0 && n <= heap_.size());
    // 初始化当前节点的索引为 i
    auto index = i;
    // 计算左子节点的索引
    auto child = 2*index+1;
    // 当左子节点存在时
    while(child < n) {
        // 如果右子节点存在且右子节点的过期时间小于左子节点的过期时间
        if(child+1 < n && heap_[child+1] < heap_[child]) {
            // 更新子节点的索引为右子节点的索引
            child++;
        }
        // 如果子节点的过期时间小于当前节点的过期时间
        if(heap_[child] < heap_[index]) {
            // 交换当前节点和子节点
            SwapNode_(index, child);
            // 更新当前节点的索引为子节点的索引
            index = child;
            // 重新计算子节点的索引
            child = 2*child+1;
        } else {
            // 如果子节点的过期时间不小于当前节点的过期时间，说明已经满足堆的性质，跳出循环
            break;
        }
    }
    // 返回是否进行了下移操作
    return index > i;
}

// 删除指定位置的结点
void HeapTimer::del_(size_t index) {
    // 确保索引 index 在堆的有效范围内
    assert(index >= 0 && index < heap_.size());
    // 将要删除的节点换到队尾，然后调整堆
    size_t tmp = index;
    size_t n = heap_.size() - 1;
    assert(tmp <= n);
    // 如果就在队尾，就不用移动了
    if(index < heap_.size()-1) {
        // 交换当前节点和队尾节点
        SwapNode_(tmp, heap_.size()-1);
        // 调整当前节点的位置，使其满足堆的性质
        if(!siftdown_(tmp, n)) {
            siftup_(tmp);
        }
    }
    // 从 ref_ 映射表中删除节点的 id
    ref_.erase(heap_.back().id);
    // 从堆中删除队尾节点
    heap_.pop_back();
}

// 调整指定id的结点
void HeapTimer::adjust(int id, int newExpires) {
    // 确保堆不为空且 id 存在于 ref_ 映射表中
    assert(!heap_.empty() && ref_.count(id));
    // 更新指定 id 的节点的过期时间
    heap_[ref_[id]].expires = Clock::now() + MS(newExpires);
    // 调整节点的位置，使其满足堆的性质
    siftdown_(ref_[id], heap_.size());
}

void HeapTimer::add(int id, int timeOut, const TimeoutCallBack& cb) {
    // 确保 id 大于等于 0
    assert(id >= 0);
    // 如果 id 已经存在于 ref_ 映射表中
    if(ref_.count(id)) {
        // 获取 id 对应的节点在堆中的索引
        int tmp = ref_[id];
        // 更新节点的过期时间
        heap_[tmp].expires = Clock::now() + MS(timeOut);
        // 更新节点的回调函数
        heap_[tmp].cb = cb;
        // 调整节点的位置，使其满足堆的性质
        if(!siftdown_(tmp, heap_.size())) {
            siftup_(tmp);
        }
    } else {
        // 获取堆的当前大小
        size_t n = heap_.size();
        // 将 id 映射到堆的末尾
        ref_[id] = n;
        // 在堆的末尾插入一个新的节点
        heap_.push_back({id, Clock::now() + MS(timeOut), cb});
        // 调整新节点的位置，使其满足堆的性质
        siftup_(n);
    }
}

// 删除指定id，并触发回调函数
void HeapTimer::doWork(int id) {
    // 如果堆为空或者 id 不存在于 ref_ 映射表中
    if(heap_.empty() || ref_.count(id) == 0) {
        // 直接返回
        return;
    }
    // 获取 id 对应的节点在堆中的索引
    size_t i = ref_[id];
    // 获取 id 对应的节点
    auto node = heap_[i];
    // 执行节点的回调函数
    node.cb();
    // 从堆中删除该节点
    del_(i);
}

void HeapTimer::tick() {
    /* 清除超时结点 */
    // 如果堆为空，直接返回
    if(heap_.empty()) {
        return;
    }
    // 当堆不为空时
    while(!heap_.empty()) {
        // 获取堆顶节点（即最早过期的节点）
        TimerNode node = heap_.front();
        // 计算当前时间与节点过期时间的差值
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            // 如果差值大于0，说明节点还未过期，跳出循环
            break; 
        }
        // 执行节点的回调函数
        node.cb();
        // 从堆中删除该节点
        pop();
    }
}

void HeapTimer::pop() {
    // 确保堆不为空
    assert(!heap_.empty());
    // 调用 del_ 函数删除堆顶元素（索引为0）
    del_(0);
}

void HeapTimer::clear() {
    // 清空 ref_ 映射表
    ref_.clear();
    // 清空 heap_ 堆
    heap_.clear();
}

int HeapTimer::GetNextTick() {
    // 调用 tick 函数处理堆中的定时器节点
    tick();
    // 初始化结果变量为 -1
    size_t res = -1;
    // 如果堆不为空
    if(!heap_.empty()) {
        // 计算堆顶节点（即最早过期的节点）的过期时间与当前时间的差值
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        // 如果差值小于0，说明节点已经过期，将结果设置为0
        if(res < 0) { res = 0; }
    }
    // 返回结果
    return res;
}