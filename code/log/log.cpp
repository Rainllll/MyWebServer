#include "log.h"

// 构造函数
Log::Log() {
    fp_ = nullptr;
    deque_ = nullptr;
    writeThread_ = nullptr;
    lineCount_ = 0;
    toDay_ = 0;
    isAsync_ = false;
}

Log::~Log() {
    while(!deque_->empty()) {
        deque_->flush();    // 唤醒消费者，处理掉剩下的任务
    }
    deque_->Close();    // 关闭队列
    writeThread_->join();   // 等待当前线程完成手中的任务
    if(fp_) {       // 冲洗文件缓冲区，关闭文件描述符
        lock_guard<mutex> locker(mtx_);
        flush();        // 清空缓冲区中的数据
        fclose(fp_);    // 关闭日志文件
    }
}

// 唤醒阻塞队列消费者，开始写日志
void Log::flush() {
    if(isAsync_) {  // 只有异步日志才会用到deque
        deque_->flush();
    }
    fflush(fp_);    // 清空输入缓冲区
}

// 懒汉模式 局部静态变量法（这种方法不需要加锁和解锁操作）
Log* Log::Instance() {
    static Log log;
    return &log;
}

// 异步日志的写线程函数
void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}

// 写线程真正的执行函数
void Log::AsyncWrite_() {
    string str = "";
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

// 初始化日志实例
void Log::init(int level, const char* path, const char* suffix, int maxQueCapacity) {
    // 设置日志系统为开启状态
    isOpen_ = true;
    // 设置日志级别
    level_ = level;
    // 设置日志文件路径
    path_ = path;
    // 设置日志文件后缀
    suffix_ = suffix;
    // 如果最大队列容量大于0，则启用异步日志
    if(maxQueCapacity) {    
        isAsync_ = true;
        // 如果队列未初始化，则创建一个新的队列
        if(!deque_) {   
            // 使用unique_ptr创建一个新的BlockQueue<std::string>对象
            unique_ptr<BlockQueue<std::string>> newQue(new BlockQueue<std::string>);
            // 将新创建的队列的所有权转移给deque_
            deque_ = move(newQue);  
            // 使用unique_ptr创建一个新的线程，该线程将执行FlushLogThread函数
            unique_ptr<thread> newThread(new thread(FlushLogThread));
            // 将新创建的线程的所有权转移给writeThread_
            writeThread_ = move(newThread);
        }
    } else {
        // 如果最大队列容量为0，则禁用异步日志
        isAsync_ = false;
    }

    // 初始化日志行数为0
    lineCount_ = 0;
    
    // 获取当前时间
    time_t timer = time(nullptr);
    // 将时间转换为本地时间
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;
    // 设置日志文件路径
    path_ = path;
    // 设置日志文件后缀
    suffix_ = suffix;
    // 生成日志文件名
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    // 设置当前日期
    toDay_ = t.tm_mday;

    {
        // 加锁，确保线程安全
        lock_guard<mutex> locker(mtx_);
        // 清空缓冲区
        buff_.RetrieveAll();
        // 如果文件指针不为空，则刷新缓冲区并关闭文件
        if(fp_) {   
            flush();
            fclose(fp_);
        }
        // 打开日志文件，如果文件不存在则创建
        fp_ = fopen(fileName, "a"); 
        // 如果文件打开失败，则创建目录并重新打开文件
        if(fp_ == nullptr) {
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a"); 
        }
        // 确保文件指针不为空
        assert(fp_ != nullptr);
    }
}

void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    // 日志日期 日志行数  如果不是今天或行数超了
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();
        
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday)    // 时间不匹配，则替换为最新的日志文件名
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        locker.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    // 在buffer内生成一条对应的日志信息
    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
                    
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);    

        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        if(isAsync_ && deque_ && !deque_->full()) { // 异步方式（加入阻塞队列中，等待写线程读取日志信息）
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {    // 同步方式（直接向文件中写入日志信息）
            fputs(buff_.Peek(), fp_);   // 同步就直接写入文件
        }
        buff_.RetrieveAll();    // 清空buff
    }
}

// 添加日志等级
void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}