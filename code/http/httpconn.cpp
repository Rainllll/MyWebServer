#include "httpconn.h"
using namespace std;

// 静态成员变量，存储源目录
const char *HttpConn::srcDir;
// 原子变量，记录当前连接的用户数量
std::atomic<int> HttpConn::userCount;
// 静态成员变量，指示是否使用ET模式
bool HttpConn::isET;

/**
 * @brief 默认构造函数，初始化成员变量
 */
HttpConn::HttpConn()
{
    // 初始化文件描述符为-1，表示未连接
    fd_ = -1;
    // 初始化地址结构体为0
    addr_ = {0};
    // 初始化连接状态为关闭
    isClose_ = true;
};

/**
 * @brief 析构函数，关闭连接
 */
HttpConn::~HttpConn()
{
    // 调用关闭连接的函数
    Close();
};

/**
 * @brief 初始化连接
 * @param fd 文件描述符
 * @param addr 地址结构体
 */
void HttpConn::init(int fd, const sockaddr_in &addr)
{
    // 确保文件描述符有效
    assert(fd > 0);
    // 增加用户计数
    userCount++;
    // 设置地址
    addr_ = addr;
    // 设置文件描述符
    fd_ = fd;
    // 清空写缓冲区
    writeBuff_.RetrieveAll();
    // 清空读缓冲区
    readBuff_.RetrieveAll();
    // 设置连接状态为打开
    isClose_ = false;
    // 记录日志
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

/**
 * @brief 关闭连接
 */
void HttpConn::Close()
{
    // 取消文件映射
    response_.UnmapFile();
    // 如果连接未关闭
    if (isClose_ == false)
    {
        // 设置连接状态为关闭
        isClose_ = true;
        // 减少用户计数
        userCount--;
        // 关闭文件描述符
        close(fd_);
        // 记录日志
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

/**
 * @brief 获取文件描述符
 * @return 文件描述符
 */
int HttpConn::GetFd() const
{
    return fd_;
};

/**
 * @brief 获取地址结构体
 * @return 地址结构体
 */
struct sockaddr_in HttpConn::GetAddr() const
{
    return addr_;
}

/**
 * @brief 获取客户端IP地址
 * @return IP地址字符串
 */
const char *HttpConn::GetIP() const
{
    return inet_ntoa(addr_.sin_addr);
}

/**
 * @brief 获取客户端端口号
 * @return 端口号
 */
int HttpConn::GetPort() const
{
    return addr_.sin_port;
}

/**
 * @brief 从文件描述符读取数据到读缓冲区
 * @param saveErrno 保存错误码的指针
 * @return 读取的字节数
 */
ssize_t HttpConn::read(int *saveErrno)
{
    ssize_t len = -1;
    do
    {
        // 从文件描述符读取数据到读缓冲区
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0)
        {
            break;
        }
    } while (isET); // ET:边沿触发要一次性全部读出
    return len;
}

/**
 * @brief 将数据从写缓冲区写入文件描述符
 * @param saveErrno 保存错误码的指针
 * @return 写入的字节数
 */
ssize_t HttpConn::write(int *saveErrno)
{
    ssize_t len = -1;
    do
    {
        // 将iov的内容写到fd中
        len = writev(fd_, iov_, iovCnt_);
        if (len <= 0)
        {
            *saveErrno = errno;
            break;
        }
        if (iov_[0].iov_len + iov_[1].iov_len == 0)
        {
            break;
        } /* 传输结束 */
        else if (static_cast<size_t>(len) > iov_[0].iov_len)
        {
            iov_[1].iov_base = (uint8_t *)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if (iov_[0].iov_len)
            {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else
        {
            iov_[0].iov_base = (uint8_t *)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuff_.Retrieve(len);
        }
    } while (isET || ToWriteBytes() > 10240);
    return len;
}

/**
 * @brief 处理HTTP请求
 * @return 处理结果
 */
bool HttpConn::process()
{
    // 初始化请求对象
    request_.Init();
    if (readBuff_.ReadableBytes() <= 0)
    {
        return false;
    }
    else if (request_.parse(readBuff_))
    { // 解析成功
        // 记录日志
        LOG_DEBUG("%s", request_.path().c_str());
        // 初始化响应对象
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    }
    else
    {
        // 初始化响应对象，状态码为400
        response_.Init(srcDir, request_.path(), false, 400);
    }

    // 生成响应报文放入写缓冲区
    response_.MakeResponse(writeBuff_);
    // 响应头
    iov_[0].iov_base = const_cast<char *>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    // 文件
    if (response_.FileLen() > 0 && response_.File())
    {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    // 记录日志
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen(), iovCnt_, ToWriteBytes());
    return true;
}
