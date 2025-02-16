#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../timer/heaptimer.h"

#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"

#include "../http/httpconn.h"

/**
 * @class WebServer
 * @brief 一个简单的Web服务器类，用于处理HTTP请求和响应。
 */
class WebServer {
public:
    /**
     * @brief 构造函数，初始化Web服务器的各种参数。
     * @param port 服务器监听的端口号。
     * @param trigMode 事件触发模式（例如，ET或LT）。
     * @param timeoutMS 连接超时时间（毫秒）。
     * @param sqlPort 数据库连接的端口号。
     * @param sqlUser 数据库用户名。
     * @param sqlPwd 数据库用户密码。
     * @param dbName 数据库名称。
     * @param connPoolNum 数据库连接池中的连接数量。
     * @param threadNum 线程池中的线程数量。
     * @param openLog 是否开启日志记录。
     * @param logLevel 日志记录级别。
     * @param logQueSize 日志队列的大小。
     */
    WebServer(
        int port, int trigMode, int timeoutMS, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    /**
     * @brief 析构函数，清理Web服务器的资源。
     */
    ~WebServer();

    /**
     * @brief 启动Web服务器，开始监听和处理客户端连接。
     */
    void Start();

private:
    /**
     * @brief 初始化服务器套接字。
     * @return 如果初始化成功返回true，否则返回false。
     */
    bool InitSocket_(); 

    /**
     * @brief 根据给定的触发模式初始化事件模式。
     * @param trigMode 事件触发模式（例如，ET或LT）。
     */
    void InitEventMode_(int trigMode);

    /**
     * @brief 将新客户端添加到服务器的用户列表中。
     * @param fd 客户端套接字文件描述符。
     * @param addr 客户端的地址信息。
     */
    void AddClient_(int fd, sockaddr_in addr);
  
    /**
     * @brief 处理监听套接字上的事件，接受新的客户端连接。
     */
    void DealListen_();

    /**
     * @brief 处理客户端的写事件。
     * @param client 指向HttpConn对象的指针，表示客户端连接。
     */
    void DealWrite_(HttpConn* client);

    /**
     * @brief 处理客户端的读事件。
     * @param client 指向HttpConn对象的指针，表示客户端连接。
     */
    void DealRead_(HttpConn* client);

    /**
     * @brief 向客户端发送错误信息。
     * @param fd 客户端套接字文件描述符。
     * @param info 错误信息字符串。
     */
    void SendError_(int fd, const char*info);

    /**
     * @brief 延长客户端连接的超时时间。
     * @param client 指向HttpConn对象的指针，表示客户端连接。
     */
    void ExtentTime_(HttpConn* client);

    /**
     * @brief 关闭客户端连接。
     * @param client 指向HttpConn对象的指针，表示客户端连接。
     */
    void CloseConn_(HttpConn* client);

    /**
     * @brief 处理客户端的读事件。
     * @param client 指向HttpConn对象的指针，表示客户端连接。
     */
    void OnRead_(HttpConn* client);

    /**
     * @brief 处理客户端的写事件。
     * @param client 指向HttpConn对象的指针，表示客户端连接。
     */
    void OnWrite_(HttpConn* client);

    /**
     * @brief 处理客户端的请求。
     * @param client 指向HttpConn对象的指针，表示客户端连接。
     */
    void OnProcess(HttpConn* client);

    /**
     * @brief 服务器支持的最大文件描述符数量。
     */
    static const int MAX_FD = 65536;

    /**
     * @brief 设置文件描述符为非阻塞模式。
     * @param fd 文件描述符。
     * @return 设置成功返回0，否则返回-1。
     */
    static int SetFdNonblock(int fd);

    int port_;                 // 服务器监听的端口号
    bool openLinger_;          // 是否开启linger选项
    int timeoutMS_;            // 连接超时时间（毫秒）
    bool isClose_;             // 服务器是否关闭
    int listenFd_;             // 监听套接字文件描述符
    char* srcDir_;             // 服务器资源目录

    uint32_t listenEvent_;     // 监听事件
    uint32_t connEvent_;       // 连接事件
   
    std::unique_ptr<HeapTimer> timer_;     // 定时器，用于管理连接超时
    std::unique_ptr<ThreadPool> threadpool_; // 线程池，用于处理客户端请求
    std::unique_ptr<Epoller> epoller_;       // epoll实例，用于I/O多路复用
    std::unordered_map<int, HttpConn> users_; // 存储所有客户端连接的映射表
};

#endif //WEBSERVER_H