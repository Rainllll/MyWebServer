#include "epoller.h"

/**
 * @brief Epoller类的构造函数
 * 
 * @param maxEvent 最大事件数量
 */
Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent){
    // 断言确保epoll文件描述符创建成功且事件数组大小大于0
    assert(epollFd_ >= 0 && events_.size() > 0);
}

/**
 * @brief Epoller类的析构函数
 */
Epoller::~Epoller() {
    // 关闭epoll文件描述符
    close(epollFd_);
}

/**
 * @brief 向epoll实例中添加文件描述符
 * 
 * @param fd 要添加的文件描述符
 * @param events 要监听的事件
 * @return true 添加成功
 * @return false 添加失败
 */
bool Epoller::AddFd(int fd, uint32_t events) {
    // 如果文件描述符无效，返回false
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    // 使用epoll_ctl函数将文件描述符添加到epoll实例中
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

/**
 * @brief 修改epoll实例中文件描述符的监听事件
 * 
 * @param fd 要修改的文件描述符
 * @param events 要监听的新事件
 * @return true 修改成功
 * @return false 修改失败
 */
bool Epoller::ModFd(int fd, uint32_t events) {
    // 如果文件描述符无效，返回false
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    // 使用epoll_ctl函数修改epoll实例中文件描述符的监听事件
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

/**
 * @brief 从epoll实例中删除文件描述符
 * 
 * @param fd 要删除的文件描述符
 * @return true 删除成功
 * @return false 删除失败
 */
bool Epoller::DelFd(int fd) {
    // 如果文件描述符无效，返回false
    if(fd < 0) return false;
    // 使用epoll_ctl函数从epoll实例中删除文件描述符
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, 0);
}

/**
 * @brief 等待事件发生
 * 
 * @param timeoutMs 超时时间（毫秒）
 * @return int 发生的事件数量
 */
int Epoller::Wait(int timeoutMs) {
    // 使用epoll_wait函数等待事件发生
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

/**
 * @brief 获取事件的文件描述符
 * 
 * @param i 事件在事件数组中的索引
 * @return int 事件的文件描述符
 */
int Epoller::GetEventFd(size_t i) const {
    // 断言确保索引在有效范围内
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

/**
 * @brief 获取事件的属性
 * 
 * @param i 事件在事件数组中的索引
 * @return uint32_t 事件的属性
 */
uint32_t Epoller::GetEvents(size_t i) const {
    // 断言确保索引在有效范围内
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}