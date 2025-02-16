#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
public:
    /**
     * @brief 默认构造函数，初始化成员变量
     */
    HttpResponse();

    /**
     * @brief 析构函数，释放资源
     */
    ~HttpResponse();

    /**
     * @brief 初始化 HttpResponse 对象
     * @param srcDir 源目录
     * @param path 请求路径
     * @param isKeepAlive 是否保持连接，默认为 false
     * @param code HTTP 状态码，默认为 -1
     */
    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);

    /**
     * @brief 生成 HTTP 响应
     * @param buff 缓冲区对象
     */
    void MakeResponse(Buffer& buff);

    /**
     * @brief 释放内存映射文件
     */
    void UnmapFile();

    /**
     * @brief 获取内存映射文件指针
     * @return 内存映射文件指针
     */
    char* File();

    /**
     * @brief 获取内存映射文件大小
     * @return 内存映射文件大小
     */
    size_t FileLen() const;

    /**
     * @brief 设置错误内容
     * @param buff 缓冲区对象
     * @param message 错误信息
     */
    void ErrorContent(Buffer& buff, std::string message);

    /**
     * @brief 获取 HTTP 状态码
     * @return HTTP 状态码
     */
    int Code() const { return code_; }

private:
    /**
     * @brief 添加状态行到缓冲区
     * @param buff 缓冲区对象
     */
    void AddStateLine_(Buffer &buff);

    /**
     * @brief 添加响应头到缓冲区
     * @param buff 缓冲区对象
     */
    void AddHeader_(Buffer &buff);

    /**
     * @brief 添加响应内容到缓冲区
     * @param buff 缓冲区对象
     */
    void AddContent_(Buffer &buff);

    /**
     * @brief 处理错误页面
     */
    void ErrorHtml_();

    /**
     * @brief 获取文件类型
     * @return 文件类型字符串
     */
    std::string GetFileType_();

    // HTTP 状态码
    int code_;
    // 是否保持连接
    bool isKeepAlive_;
    // 请求路径
    std::string path_;
    // 源目录
    std::string srcDir_;
    // 内存映射文件指针
    char* mmFile_; 
    // 内存映射文件状态
    struct stat mmFileStat_;

    // 后缀类型集
    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  
    // 编码状态集
    static const std::unordered_map<int, std::string> CODE_STATUS;          
    // 编码路径集
    static const std::unordered_map<int, std::string> CODE_PATH;            
};


#endif //HTTP_RESPONSE_H
