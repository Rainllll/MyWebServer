#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>   // readv/writev
#include <arpa/inet.h> // sockaddr_in
#include <stdlib.h>    // atoi()
#include <errno.h>

#include "../log/log.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"
/*
进行读写数据并调用httprequest 来解析数据以及httpresponse来生成响应
*/
class HttpConn
{
public:
    /**
     * @brief 默认构造函数，初始化成员变量
     */
    HttpConn();

    /**
     * @brief 析构函数，关闭连接
     */
    ~HttpConn();

    /**
     * @brief 初始化连接
     * @param sockFd 文件描述符
     * @param addr 地址结构体
     */
    void init(int sockFd, const sockaddr_in &addr);

    /**
     * @brief 从文件描述符读取数据到读缓冲区
     * @param saveErrno 保存错误码的指针
     * @return 读取的字节数
     */
    ssize_t read(int *saveErrno);

    /**
     * @brief 将数据从写缓冲区写入文件描述符
     * @param saveErrno 保存错误码的指针
     * @return 写入的字节数
     */
    ssize_t write(int *saveErrno);

    /**
     * @brief 关闭连接
     */
    void Close();

    /**
     * @brief 获取文件描述符
     * @return 文件描述符
     */
    int GetFd() const;

    /**
     * @brief 获取客户端端口号
     * @return 端口号
     */
    int GetPort() const;

    /**
     * @brief 获取客户端IP地址
     * @return IP地址字符串
     */
    const char *GetIP() const;

    /**
     * @brief 获取地址结构体
     * @return 地址结构体
     */
    sockaddr_in GetAddr() const;

    /**
     * @brief 处理HTTP请求
     * @return 处理结果
     */
    bool process();

    /**
     * @brief 计算待写入的总字节数
     * @return 待写入的总字节数
     */
    int ToWriteBytes()
    {
        return iov_[0].iov_len + iov_[1].iov_len;
    }

    /**
     * @brief 检查是否保持连接
     * @return 是否保持连接
     */
    bool IsKeepAlive() const
    {
        return request_.IsKeepAlive();
    }

    // 静态成员变量，指示是否使用ET模式
    static bool isET;
    // 静态成员变量，存储源目录
    static const char *srcDir;
    // 原子变量，记录当前连接的用户数量
    static std::atomic<int> userCount; // 原子，支持锁

private:
    // 文件描述符
    int fd_;
    // 地址结构体
    struct sockaddr_in addr_;

    // 连接状态
    bool isClose_;

    // iovec结构体数量
    int iovCnt_;
    // iovec结构体数组
    struct iovec iov_[2];

    // 读缓冲区
    Buffer readBuff_;
    // 写缓冲区
    Buffer writeBuff_;

    // HTTP请求对象
    HttpRequest request_;
    // HTTP响应对象
    HttpResponse response_;
};

#endif // HTTP_CONN_H