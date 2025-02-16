// 包含日志模块的头文件
#include "../code/log/log.h"
// 包含线程池模块的头文件
#include "../code/pool/threadpool.h"
// 包含特性测试宏的头文件
#include <features.h>

// 检查当前使用的glibc版本是否小于2.30
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
// 如果是，包含系统调用的头文件
#include <sys/syscall.h>
// 定义gettid函数，用于获取线程ID
#define gettid() syscall(SYS_gettid)
#endif

/**
 * @brief 测试日志功能
 * 
 * 该函数用于测试日志模块的功能，包括不同日志级别的设置和日志文件的写入。
 */
void TestLog() {
    // 初始化计数器和日志级别
    int cnt = 0, level = 0;
    // 初始化日志实例，设置日志级别为0，日志文件名为testlog1，后缀为.log，缓冲区大小为0
    Log::Instance()->init(level, "./testlog1", ".log", 0);
    // 从最高日志级别（3）开始，逐步降低日志级别
    for(level = 3; level >= 0; level--) {
        // 设置当前日志级别
        Log::Instance()->SetLevel(level);
        // 循环10000次
        for(int j = 0; j < 10000; j++ ){
            // 循环4次
            for(int i = 0; i < 4; i++) {
                // 记录日志，日志级别为i，日志内容为"Test 111111111 cnt ============= "
                LOG_BASE(i,"%s 111111111 %d ============= ", "Test", cnt++);
            }
        }
    }
    // 重置计数器
    cnt = 0;
    // 重新初始化日志实例，设置日志级别为0，日志文件名为testlog2，后缀为.log，缓冲区大小为5000
    Log::Instance()->init(level, "./testlog2", ".log", 5000);
    // 从最低日志级别（0）开始，逐步提高日志级别
    for(level = 0; level < 4; level++) {
        // 设置当前日志级别
        Log::Instance()->SetLevel(level);
        // 循环10000次
        for(int j = 0; j < 10000; j++ ){
            // 循环4次
            for(int i = 0; i < 4; i++) {
                // 记录日志，日志级别为i，日志内容为"Test 222222222 cnt ============= "
                LOG_BASE(i,"%s 222222222 %d ============= ", "Test", cnt++);
            }
        }
    }
}

/**
 * @brief 线程日志任务
 * 
 * 该函数用于在线程池中执行的日志任务，记录线程ID和计数器的值。
 * 
 * @param i 日志级别
 * @param cnt 计数器
 */
void ThreadLogTask(int i, int cnt) {
    // 循环10000次
    for(int j = 0; j < 10000; j++ ){
        // 记录日志，日志级别为i，日志内容为"PID:[线程ID]======= cnt ========= "
        LOG_BASE(i,"PID:[%04d]======= %05d ========= ", gettid(), cnt++);
    }
}

/**
 * @brief 测试线程池功能
 * 
 * 该函数用于测试线程池模块的功能，包括线程池的创建和任务的添加。
 */
void TestThreadPool() {
    // 初始化日志实例，设置日志级别为0，日志文件名为testThreadpool，后缀为.log，缓冲区大小为5000
    Log::Instance()->init(0, "./testThreadpool", ".log", 5000);
    // 创建一个包含6个线程的线程池
    ThreadPool threadpool(6);
    // 循环18次
    for(int i = 0; i < 18; i++) {
        // 向线程池中添加任务，任务为ThreadLogTask函数，参数为i % 4和i * 10000
        threadpool.AddTask(std::bind(ThreadLogTask, i % 4, i * 10000));
    }
    // 等待用户输入，用于暂停程序
    getchar();
}

/**
 * @brief 主函数
 * 
 * 该函数是程序的入口点，调用TestLog和TestThreadPool函数进行测试。
 * 
 * @return int 程序的返回值
 */
int main() {
    // 调用TestLog函数进行日志功能测试
    TestLog();
    // 调用TestThreadPool函数进行线程池功能测试
    TestThreadPool();
    // 返回0，表示程序正常结束
    return 0;
}