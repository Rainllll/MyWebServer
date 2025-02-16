#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> //epoll_ctl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>

/**
 * @brief Epoller类，用于管理epoll实例
 */
class Epoller {
public:
    /**
     * @brief 构造函数，初始化epoll实例
     * 
     * @param maxEvent 最大事件数量，默认为1024
     */
    explicit Epoller(int maxEvent = 1024);

    /**
     * @brief 析构函数，关闭epoll实例
     */
    ~Epoller();

    /**
     * @brief 向epoll实例中添加文件描述符
     * 
     * @param fd 要添加的文件描述符
     * @param events 要监听的事件
     * @return true 添加成功
     * @return false 添加失败
     */
    bool AddFd(int fd, uint32_t events);

    /**
     * @brief 修改epoll实例中文件描述符的监听事件
     * 
     * @param fd 要修改的文件描述符
     * @param events 要监听的新事件
     * @return true 修改成功
     * @return false 修改失败
     */
    bool ModFd(int fd, uint32_t events);

    /**
     * @brief 从epoll实例中删除文件描述符
     * 
     * @param fd 要删除的文件描述符
     * @return true 删除成功
     * @return false 删除失败
     */
    bool DelFd(int fd);

    /**
     * @brief 等待事件发生
     * 
     * @param timeoutMs 超时时间（毫秒），默认为-1，表示无限等待
     * @return int 发生的事件数量
     */
    int Wait(int timeoutMs = -1);

    /**
     * @brief 获取事件的文件描述符
     * 
     * @param i 事件在事件数组中的索引
     * @return int 事件的文件描述符
     */
    int GetEventFd(size_t i) const;

    /**
     * @brief 获取事件的属性
     * 
     * @param i 事件在事件数组中的索引
     * @return uint32_t 事件的属性
     */
    uint32_t GetEvents(size_t i) const;
        
private:
    int epollFd_; // epoll实例的文件描述符
    std::vector<struct epoll_event> events_; // 存储事件的数组    
};

#endif //EPOLLER_H