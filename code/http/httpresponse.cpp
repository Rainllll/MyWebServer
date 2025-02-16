#include "httpresponse.h"

using namespace std;

// 定义一个常量 unordered_map，用于存储文件后缀名与对应的 MIME 类型
const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

// 定义一个常量 unordered_map，用于存储 HTTP 状态码与对应的状态描述
const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

// 定义一个常量 unordered_map，用于存储 HTTP 状态码与对应的错误页面路径
const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

/**
 * @brief 默认构造函数，初始化成员变量
 */
HttpResponse::HttpResponse() {
    // 初始化状态码为 -1，表示未设置
    code_ = -1;
    // 初始化路径为空字符串
    path_ = srcDir_ = "";
    // 初始化是否保持连接为 false
    isKeepAlive_ = false;
    // 初始化内存映射文件指针为 nullptr
    mmFile_ = nullptr; 
    // 初始化文件状态结构体为全零
    mmFileStat_ = { 0 };
};

/**
 * @brief 析构函数，释放内存映射文件
 */
HttpResponse::~HttpResponse() {
    // 调用 UnmapFile 函数释放内存映射文件
    UnmapFile();
}

/**
 * @brief 初始化 HttpResponse 对象
 * @param srcDir 源目录
 * @param path 请求路径
 * @param isKeepAlive 是否保持连接
 * @param code HTTP 状态码
 */
void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code){
    // 确保源目录不为空
    assert(srcDir != "");
    // 如果内存映射文件已存在，则释放
    if(mmFile_) { UnmapFile(); }
    // 设置状态码
    code_ = code;
    // 设置是否保持连接
    isKeepAlive_ = isKeepAlive;
    // 设置请求路径
    path_ = path;
    // 设置源目录
    srcDir_ = srcDir;
    // 重置内存映射文件指针为 nullptr
    mmFile_ = nullptr; 
    // 重置文件状态结构体为全零
    mmFileStat_ = { 0 };
}

/**
 * @brief 生成 HTTP 响应
 * @param buff 缓冲区对象
 */
void HttpResponse::MakeResponse(Buffer& buff) {
    /* 判断请求的资源文件 */
    // 使用 stat 函数获取文件状态，如果文件不存在或为目录，则设置状态码为 404
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;
    }
    // 如果文件不可读，则设置状态码为 403
    else if(!(mmFileStat_.st_mode & S_IROTH)) {
        code_ = 403;
    }
    // 如果状态码未设置，则设置为 200
    else if(code_ == -1) { 
        code_ = 200; 
    }
    // 处理错误页面
    ErrorHtml_();
    // 添加状态行
    AddStateLine_(buff);
    // 添加响应头
    AddHeader_(buff);
    // 添加响应内容
    AddContent_(buff);
}

/**
 * @brief 获取内存映射文件指针
 * @return 内存映射文件指针
 */
char* HttpResponse::File() {
    return mmFile_;
}

/**
 * @brief 获取内存映射文件大小
 * @return 内存映射文件大小
 */
size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

/**
 * @brief 处理错误页面
 */
void HttpResponse::ErrorHtml_() {
    // 如果状态码对应的错误页面路径存在，则设置请求路径为错误页面路径，并获取文件状态
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

/**
 * @brief 添加状态行到缓冲区
 * @param buff 缓冲区对象
 */
void HttpResponse::AddStateLine_(Buffer& buff) {
    string status;
    // 如果状态码对应的状态描述存在，则获取状态描述
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    // 如果状态码不存在，则设置状态码为 400，并获取对应的状态描述
    else {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    // 将状态行添加到缓冲区
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

/**
 * @brief 添加响应头到缓冲区
 * @param buff 缓冲区对象
 */
void HttpResponse::AddHeader_(Buffer& buff) {
    // 添加 Connection 头
    buff.Append("Connection: ");
    // 如果保持连接，则添加 keep-alive 相关头
    if(isKeepAlive_) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else{
        // 如果不保持连接，则添加 close 头
        buff.Append("close\r\n");
    }
    // 添加 Content-type 头
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

/**
 * @brief 添加响应内容到缓冲区
 * @param buff 缓冲区对象
 */
void HttpResponse::AddContent_(Buffer& buff) {
    // 打开文件
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    // 如果文件打开失败，则添加错误内容到缓冲区并返回
    if(srcFd < 0) { 
        ErrorContent(buff, "File NotFound!");
        return; 
    }

    //将文件映射到内存提高文件的访问速度  MAP_PRIVATE 建立一个写入时拷贝的私有映射
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    // 如果内存映射失败，则添加错误内容到缓冲区并返回
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");
        return; 
    }
    // 设置内存映射文件指针
    mmFile_ = (char*)mmRet;
    // 关闭文件描述符
    close(srcFd);
    // 添加 Content-length 头和响应内容到缓冲区
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

/**
 * @brief 释放内存映射文件
 */
void HttpResponse::UnmapFile() {
    // 检查内存映射文件指针是否为空
    if(mmFile_) {
        // 使用 munmap 函数释放内存映射文件
        munmap(mmFile_, mmFileStat_.st_size);
        // 将内存映射文件指针设置为 nullptr
        mmFile_ = nullptr;
    }
}

// 判断文件类型 
string HttpResponse::GetFileType_() {
    // 查找路径中最后一个点的位置，即文件后缀名的起始位置
    string::size_type idx = path_.find_last_of('.');
    // 如果没有找到点，说明没有后缀名，返回默认的MIME类型 "text/plain"
    if(idx == string::npos) {   // 最大值 find函数在找不到指定值得情况下会返回string::npos
        return "text/plain";
    }
    // 获取文件后缀名
    string suffix = path_.substr(idx);
    // 如果后缀名在 SUFFIX_TYPE 映射中存在，返回对应的MIME类型
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    // 如果后缀名不在映射中，返回默认的MIME类型 "text/plain"
    return "text/plain";
}

void HttpResponse::ErrorContent(Buffer& buff, string message) 
{
    string body;
    string status;
    // 构建错误页面的HTML内容
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    // 如果状态码在 CODE_STATUS 映射中存在，获取对应的状态描述
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        // 如果状态码不在映射中，设置状态描述为 "Bad Request"
        status = "Bad Request";
    }
    // 将状态码和状态描述添加到错误页面的HTML内容中
    body += to_string(code_) + " : " + status  + "\n";
    // 将错误信息添加到错误页面的HTML内容中
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    // 将错误页面的HTML内容长度添加到缓冲区
    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    // 将错误页面的HTML内容添加到缓冲区
    buff.Append(body);
}
