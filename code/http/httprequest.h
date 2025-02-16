#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>    // 正则表达式
#include <errno.h>     
#include <mysql/mysql.h>  //mysql

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"

class HttpRequest {
public:
    /**
     * @brief 解析状态枚举
     */
    enum PARSE_STATE {
        REQUEST_LINE,  // 请求行
        HEADERS,       // 请求头
        BODY,          // 请求体
        FINISH,        // 完成
    };
    
    /**
     * @brief 默认构造函数，初始化成员变量
     */
    HttpRequest() { Init(); }

    /**
     * @brief 默认析构函数
     */
    ~HttpRequest() = default;

    /**
     * @brief 初始化HttpRequest对象
     */
    void Init();

    /**
     * @brief 解析HTTP请求
     * @param buff 缓冲区对象
     * @return 解析成功与否
     */
    bool parse(Buffer& buff);   

    /**
     * @brief 获取请求路径
     * @return 请求路径字符串
     */
    std::string path() const;

    /**
     * @brief 获取请求路径的引用
     * @return 请求路径字符串的引用
     */
    std::string& path();

    /**
     * @brief 获取请求方法
     * @return 请求方法字符串
     */
    std::string method() const;

    /**
     * @brief 获取HTTP版本
     * @return HTTP版本字符串
     */
    std::string version() const;

    /**
     * @brief 获取POST请求中的参数值
     * @param key 参数名
     * @return 参数值字符串
     */
    std::string GetPost(const std::string& key) const;

    /**
     * @brief 获取POST请求中的参数值
     * @param key 参数名
     * @return 参数值字符串
     */
    std::string GetPost(const char* key) const;

    /**
     * @brief 判断是否保持连接
     * @return 是否保持连接
     */
    bool IsKeepAlive() const;

private:
    /**
     * @brief 解析请求行
     * @param line 请求行字符串
     * @return 解析成功与否
     */
    bool ParseRequestLine_(const std::string& line);    // 处理请求行

    /**
     * @brief 解析请求头
     * @param line 请求头字符串
     */
    void ParseHeader_(const std::string& line);         // 处理请求头

    /**
     * @brief 解析请求体
     * @param line 请求体字符串
     */
    void ParseBody_(const std::string& line);           // 处理请求体

    /**
     * @brief 解析请求路径
     */
    void ParsePath_();                                  // 处理请求路径

    /**
     * @brief 解析POST请求
     */
    void ParsePost_();                                  // 处理Post事件

    /**
     * @brief 从URL编码中解析数据
     */
    void ParseFromUrlencoded_();                        // 从url种解析编码

    /**
     * @brief 用户验证
     * @param name 用户名
     * @param pwd 密码
     * @param isLogin 是否登录
     * @return 验证成功与否
     */
    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);  // 用户验证

    PARSE_STATE state_;  // 当前解析状态
    std::string method_, path_, version_, body_;  // 请求方法、路径、版本、请求体
    std::unordered_map<std::string, std::string> header_;  // 请求头
    std::unordered_map<std::string, std::string> post_;  // POST请求参数

    static const std::unordered_set<std::string> DEFAULT_HTML;  // 默认HTML页面
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;  // 默认HTML标签
    static int ConverHex(char ch);  // 16进制转换为10进制
};

#endif