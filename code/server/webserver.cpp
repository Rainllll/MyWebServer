#include "webserver.h"

using namespace std;

/**
 * @brief WebServer类的构造函数
 * 
 * @param port 服务器监听的端口号
 * @param trigMode 事件触发模式（0: LT, 1: ET on conn, 2: ET on listen, 3: ET on both）
 * @param timeoutMS 超时时间（毫秒）
 * @param sqlPort 数据库端口号
 * @param sqlUser 数据库用户名
 * @param sqlPwd 数据库密码
 * @param dbName 数据库名称
 * @param connPoolNum 连接池数量
 * @param threadNum 线程池线程数量
 * @param openLog 是否开启日志
 * @param logLevel 日志级别
 * @param logQueSize 日志队列大小
 */
WebServer::WebServer(
            int port, int trigMode, int timeoutMS,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize):
            port_(port), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    {
    // 是否打开日志标志
    if(openLog) {
        // 初始化日志系统
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) {
            // 服务器初始化错误
            LOG_ERROR("========== Server init error!==========");
        } else {
            // 服务器初始化成功
            LOG_INFO("========== Server init ==========");
            // 打印监听模式和连接模式
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            // 打印日志系统级别
            LOG_INFO("LogSys level: %d", logLevel);
            // 打印资源目录
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            // 打印数据库连接池数量和线程池线程数量
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }

    // 获取当前工作目录
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    // 拼接资源目录
    strcat(srcDir_, "/resources/");
    // 初始化用户数量
    HttpConn::userCount = 0;
    // 设置资源目录
    HttpConn::srcDir = srcDir_;

    // 初始化数据库连接池
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    // 初始化事件模式
    InitEventMode_(trigMode);
    // 初始化套接字
    if(!InitSocket_()) { isClose_ = true;}
}

/**
 * @brief WebServer类的析构函数
 */
WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

/**
 * @brief 初始化事件模式
 * 
 * @param trigMode 事件触发模式（0: LT, 1: ET on conn, 2: ET on listen, 3: ET on both）
 */
void WebServer::InitEventMode_(int trigMode) {
    // 初始化监听事件，设置为检测socket关闭
    listenEvent_ = EPOLLRDHUP;    
    // 初始化连接事件，设置为EPOLLONESHOT（由一个线程处理）和检测socket关闭
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;     
    // 根据传入的trigMode参数，设置事件触发模式
    switch (trigMode)
    {
    case 0:
        // 默认模式，不做任何修改
        break;
    case 1:
        // 设置连接事件为ET模式
        connEvent_ |= EPOLLET;
        break;
    case 2:
        // 设置监听事件为ET模式
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        // 设置监听事件和连接事件都为ET模式
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        // 默认设置监听事件和连接事件都为ET模式
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    // 设置HttpConn类的isET静态成员变量，用于判断是否为ET模式
    HttpConn::isET = (connEvent_ & EPOLLET);
}

/**
 * @brief 启动服务器
 */
void WebServer::Start() {
    // 初始化epoll_wait的超时时间为-1，表示无事件时将阻塞
    int timeMS = -1;  
    // 如果服务器未关闭，则打印服务器启动信息
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    // 进入服务器主循环，直到服务器关闭
    while(!isClose_) {
        // 如果设置了超时时间，则获取下一次的超时等待时间
        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();     
        }
        // 调用epoller的Wait函数等待事件发生，返回发生的事件数量
        int eventCnt = epoller_->Wait(timeMS);
        // 遍历所有发生的事件
        for(int i = 0; i < eventCnt; i++) {
            // 获取事件的文件描述符
            int fd = epoller_->GetEventFd(i);
            // 获取事件的属性
            uint32_t events = epoller_->GetEvents(i);
            // 如果事件的文件描述符是监听套接字，则处理监听事件
            if(fd == listenFd_) {
                DealListen_();
            }
            // 如果事件是客户端关闭连接、挂起或错误，则关闭客户端连接
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            }
            // 如果事件是可读事件，则处理读事件
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            // 如果事件是可写事件，则处理写事件
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            } else {
                // 如果事件类型未知，则打印错误信息
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

/**
 * @brief 向客户端发送错误信息并关闭连接
 * 
 * @param fd 客户端套接字
 * @param info 错误信息
 */
void WebServer::SendError_(int fd, const char*info) {
    // 确保文件描述符有效
    assert(fd > 0);
    // 向客户端发送错误信息
    int ret = send(fd, info, strlen(info), 0);
    // 如果发送失败，记录警告日志
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    // 关闭客户端连接
    close(fd);
}

/**
 * @brief 关闭客户端连接
 * 
 * @param client 客户端连接对象
 */
void WebServer::CloseConn_(HttpConn* client) {
    // 确保客户端连接对象有效
    assert(client);
    // 记录客户端关闭连接的日志信息
    LOG_INFO("Client[%d] quit!", client->GetFd());
    // 从epoll实例中删除客户端的文件描述符
    epoller_->DelFd(client->GetFd());
    // 关闭客户端连接
    client->Close();
}

/**
 * @brief 添加新客户端连接
 * 
 * @param fd 客户端套接字
 * @param addr 客户端地址
 */
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    // 确保文件描述符有效
    assert(fd > 0);
    // 初始化客户端连接对象
    users_[fd].init(fd, addr);
    // 如果设置了超时时间，则将客户端连接添加到定时器中
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    // 将客户端连接添加到epoll实例中，监听读事件和连接事件
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    // 设置客户端套接字为非阻塞模式
    SetFdNonblock(fd);
    // 记录客户端连接成功的日志信息
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

// 处理监听套接字，主要逻辑是accept新的套接字，并加入timer和epoller中
/**
 * @brief 处理监听套接字上的事件
 * 
 * 该函数用于处理监听套接字上的事件，主要逻辑是接受新的客户端连接，并将其加入到定时器和epoll实例中。
 * 如果事件模式为ET（边缘触发），则会持续接受新的连接，直到没有新的连接为止。
 */
void WebServer::DealListen_() {
    // 定义一个sockaddr_in结构体，用于存储客户端的地址信息
    struct sockaddr_in addr;
    // 定义一个socklen_t类型的变量，用于存储客户端地址的长度
    socklen_t len = sizeof(addr);
    // 使用do-while循环，确保至少执行一次循环体
    do {
        // 调用accept函数接受新的客户端连接，返回一个新的套接字描述符
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        // 如果返回的套接字描述符小于等于0，表示接受连接失败，直接返回
        if(fd <= 0) { return;}
        // 如果当前的用户数量已经达到了最大限制
        else if(HttpConn::userCount >= MAX_FD) {
            // 向客户端发送错误信息，表示服务器繁忙
            SendError_(fd, "Server busy!");
            // 记录警告日志，提示客户端连接已满
            LOG_WARN("Clients is full!");
            // 返回，不再处理新的连接
            return;
        }
        // 调用AddClient_函数，将新的客户端连接添加到服务器中
        AddClient_(fd, addr);
    // 如果事件模式为ET（边缘触发），则继续循环，直到没有新的连接为止
    } while(listenEvent_ & EPOLLET);
}

// 处理读事件，主要逻辑是将OnRead加入线程池的任务队列中
/**
 * @brief 处理读事件
 * 
 * @param client 客户端连接对象
 */
void WebServer::DealRead_(HttpConn* client) {
    // 确保客户端连接对象有效
    assert(client);
    // 延长客户端连接的超时时间
    ExtentTime_(client);
    // 将读事件处理函数添加到线程池的任务队列中
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client)); // 这是一个右值，bind将参数和函数绑定
}

// 处理写事件，主要逻辑是将OnWrite加入线程池的任务队列中
/**
 * @brief 处理写事件
 * 
 * @param client 客户端连接对象
 */
void WebServer::DealWrite_(HttpConn* client) {
    // 确保客户端连接对象有效
    assert(client);
    // 延长客户端连接的超时时间
    ExtentTime_(client);
    // 将写事件处理函数添加到线程池的任务队列中
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

/**
 * @brief 延长客户端连接的超时时间
 * 
 * @param client 客户端连接对象
 */
void WebServer::ExtentTime_(HttpConn* client) {
    // 确保客户端连接对象有效
    assert(client);
    // 如果设置了超时时间，则调整客户端连接的超时时间
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

/**
 * @brief 处理读事件
 * 
 * @param client 客户端连接对象
 */
void WebServer::OnRead_(HttpConn* client) {
    // 确保客户端连接对象有效
    assert(client);
    // 定义一个变量用于存储读取操作的返回值
    int ret = -1;
    // 定义一个变量用于存储读取操作的错误码
    int readErrno = 0;
    // 调用客户端连接对象的read方法读取数据，并将错误码存储在readErrno中
    ret = client->read(&readErrno);
    // 如果读取操作返回值小于等于0且错误码不是EAGAIN（表示没有数据可读），则关闭客户端连接
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    // 业务逻辑的处理（先读后处理）
    OnProcess(client);
}

/* 处理读（请求）数据的函数 */
void WebServer::OnProcess(HttpConn* client) {
    // 首先调用process()进行逻辑处理
    if(client->process()) { // 根据返回的信息重新将fd置为EPOLLOUT（写）或EPOLLIN（读）
    //读完事件就跟内核说可以写了
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);    // 响应成功，修改监听事件为写,等待OnWrite_()发送
    } else {
    //写完事件就跟内核说可以读了
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

/**
 * @brief 处理写事件
 * 
 * @param client 客户端连接对象
 */
void WebServer::OnWrite_(HttpConn* client) {
    // 确保客户端连接对象有效
    assert(client);
    // 定义一个变量用于存储写入操作的返回值
    int ret = -1;
    // 定义一个变量用于存储写入操作的错误码
    int writeErrno = 0;
    // 调用客户端连接对象的write方法写入数据，并将错误码存储在writeErrno中
    ret = client->write(&writeErrno);
    // 如果客户端没有数据需要写入
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            // 重新将文件描述符设置为监听读事件
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
            return;
        }
    }
    // 如果写入操作返回值小于0
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {  // 缓冲区满了 
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    // 关闭客户端连接
    CloseConn_(client);
}

/* Create listenFd */
/**
 * @brief 初始化服务器套接字
 * 
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool WebServer::InitSocket_() {
    // 定义一个变量用于存储函数返回值
    int ret;
    // 定义一个sockaddr_in结构体，用于存储服务器地址信息
    struct sockaddr_in addr;
    // 设置地址族为IPv4
    addr.sin_family = AF_INET;
    // 设置地址为INADDR_ANY，表示监听所有可用的网络接口
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // 设置端口号
    addr.sin_port = htons(port_);

    // 创建套接字，使用IPv4协议，流式套接字，默认协议
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    // 如果创建套接字失败
    if(listenFd_ < 0) {
        // 记录错误日志
        LOG_ERROR("Create socket error!", port_);
        // 返回失败
        return false;
    }

    // 设置套接字选项，允许地址重用
    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    // 如果设置选项失败
    if(ret == -1) {
        // 记录错误日志
        LOG_ERROR("set socket setsockopt error !");
        // 关闭套接字
        close(listenFd_);
        // 返回失败
        return false;
    }

    // 绑定套接字到指定的地址和端口
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    // 如果绑定失败
    if(ret < 0) {
        // 记录错误日志
        LOG_ERROR("Bind Port:%d error!", port_);
        // 关闭套接字
        close(listenFd_);
        // 返回失败
        return false;
    }

    // 开始监听套接字，设置最大连接数为8
    ret = listen(listenFd_, 8);
    // 如果监听失败
    if(ret < 0) {
        // 记录错误日志
        LOG_ERROR("Listen port:%d error!", port_);
        // 关闭套接字
        close(listenFd_);
        // 返回失败
        return false;
    }
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);  // 将监听套接字加入epoller
    if(ret == 0) {
        // 记录错误日志
        LOG_ERROR("Add listen error!");
        // 关闭套接字
        close(listenFd_);
        // 返回失败
        return false;
    }
    // 设置监听套接字为非阻塞模式
    SetFdNonblock(listenFd_);
    // 记录服务器启动信息
    LOG_INFO("Server port:%d", port_);
    // 返回成功
    return true;
}

// 设置非阻塞
/**
 * @brief 设置文件描述符为非阻塞模式
 * 
 * @param fd 需要设置的文件描述符
 * @return int 返回值，成功返回0，失败返回-1
 */
int WebServer::SetFdNonblock(int fd) {
    // 确保文件描述符有效
    assert(fd > 0);
    // 使用fcntl函数将文件描述符设置为非阻塞模式
    // F_SETFL用于设置文件状态标志
    // F_GETFD用于获取文件描述符标志
    // O_NONBLOCK用于设置非阻塞模式
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


