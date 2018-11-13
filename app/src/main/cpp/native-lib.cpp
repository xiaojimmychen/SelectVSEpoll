#include <jni.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <error.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <linux/in.h>
#include <netinet/in.h>
#include <endian.h>
#include "NativeLog.h"
#include <arpa/inet.h>
#include <memory>
#include <cstring>

const int SELECT_SERVER_PORT = 8880;
const int EPOLL_SERVER_PORT = 8889;
const int BUFFER_SIZE = 1024;
std::unique_ptr<NativeLog> Log = nullptr;
const char *LOG_TAG = "SelectVSEpoll";
JavaVM *java_vm = nullptr;

bool select_init_success = false;
bool epoll_init_sucess = false;

pthread_mutex_t select_mutex;
pthread_cond_t select_cond;

pthread_mutex_t epoll_mutex;
pthread_cond_t epoll_cond;

// 封装的条件变量唤醒函数
static void send_cond_signal(pthread_mutex_t *mutex, pthread_cond_t *cond, bool *condition, bool value){
    pthread_mutex_lock(mutex);
    *condition = value;
    pthread_cond_signal(cond);
    pthread_mutex_unlock(mutex);
    return;
}

// Select服务端执行线程
static void *
select_server_start(void *arg)
{
    JNIEnv *env;
    int ret;
    int select_server_fd = 0;
    int client_fd = 0;
    int client_addr_len = 0;
    struct sockaddr_in server_addr, client_addr;
    fd_set select_fd_set;
    fd_set dump_fd_set;
    int select_fd[100] = { 0 };
    int max_select_fd = 0;
    int select_count = 0;
    struct timeval tv;
    char recvBuf[BUFFER_SIZE] = { 0 };
    char sendBuf[BUFFER_SIZE] = { 0 };
    char msg[BUFFER_SIZE] = {0};
    bool still_running = true;

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    // 获取该线程的JNIEnv变量
    ret = java_vm->AttachCurrentThread(&env, NULL);
    if(ret != JNI_OK) {
        send_cond_signal(&select_mutex, &select_cond, &select_init_success, false);
        return NULL;
    }

    memset(msg, 0, BUFFER_SIZE);
    // 创建socket端口
    select_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(select_server_fd < 0) {
        Log->e(env, LOG_TAG, "Unable to create socket!");
        send_cond_signal(&select_mutex, &select_cond, &select_init_success, false);
        return NULL;
    }

    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SELECT_SERVER_PORT);

    ret = bind(select_server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
    if (ret != 0) {
        Log->e(env, LOG_TAG, "Unable to bind server!");
        send_cond_signal(&select_mutex, &select_cond, &select_init_success, false);
        close(select_server_fd);
        return NULL;
    }

    listen(select_server_fd, 100);

    // 走到这里代表Select服务端创建无误，可以通知父线程退出
    send_cond_signal(&select_mutex, &select_cond, &select_init_success, true);
    FD_SET(select_server_fd, &select_fd_set);

    while(still_running) {
        FD_ZERO(&dump_fd_set);
        // select_fd_set用于保存所有文件set，每次循环在此处都需要赋值给dump_fd_set
        dump_fd_set = select_fd_set;

        // 获取最大的select监听句柄号
        max_select_fd = select_server_fd;
        for (int i = 0; i < select_count; ++i) {
            if(select_fd[i] > max_select_fd)
                max_select_fd = select_fd[i];
        }

        ret = select(max_select_fd + 1, &dump_fd_set, NULL, NULL, NULL);

        if(ret < 0) {
            Log->e(env, LOG_TAG, "select syscall failed!");
            continue;
        }

        memset(recvBuf, 0, BUFFER_SIZE);

        // 有客户端连接请求
        if (FD_ISSET(select_server_fd, &dump_fd_set)) {
            client_addr_len = sizeof(struct sockaddr_in);
            client_fd = accept(select_server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
            if (client_fd < 0) {
                Log->e(env, LOG_TAG, "Accept syscall failed!");
                continue;
            } else {
                // 保存客户端的文件句柄
                select_fd[select_count++] = client_fd;
                FD_SET(client_fd, &select_fd_set);
                snprintf(msg, BUFFER_SIZE, "client:%d accept! select_count:%d", client_fd, select_count);
                Log->d(env, LOG_TAG, msg);
            }

        }

        // 处理客户端的数据发送请求
        for (int i = 0; i < select_count; ++i) {
            int read_len = 0;
            if (FD_ISSET(select_fd[i], &dump_fd_set)) {
                read_len = recv(select_fd[i], recvBuf, BUFFER_SIZE, 0);
                // 对端已关闭或者出现错误，都进行关闭处理
                if(read_len <= 0) {
                    if(read_len)
                        snprintf(sendBuf, BUFFER_SIZE, "clientfd:%d recv from client failed! Now select_count:%d", select_fd[i], select_count);
                    else
                        snprintf(sendBuf, BUFFER_SIZE, "clientfd:%d remote has been closed! Now select_count:%d", select_fd[i], select_count);
                    Log->d(env, LOG_TAG, sendBuf);
                    // 获取待关闭的文件句柄
                    int close_fd = select_fd[i];
                    // 将select_fd数组的最后一个文件句柄保存到待关闭文件句柄的slot处
                    select_fd[i] = select_fd[select_count - 1];
                    // 处于连接状态的socket数量减一
                    select_count--;
                    FD_CLR(close_fd, &select_fd_set);
                    close(close_fd);
                    // 因为已经将select_fd数组最后一个保存到当前i值处，所以下一循环还是要处理该句柄数据
                    i--;
                    continue;
                }
                memset(sendBuf, 0, BUFFER_SIZE);
                sprintf(sendBuf, "clientfd:%d-msg:%s", select_fd[i], recvBuf);
                Log->d(env, LOG_TAG, sendBuf);
                send(select_fd[i], sendBuf, BUFFER_SIZE, 0);
            }
        }
    }
    return NULL;
}

// JNI调用，用于创建Select服务端线程
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_blog4jimmy_hp_selectvsepoll_MainActivity_startSelectServer(JNIEnv *env, jobject instance) {

    pthread_t select_pid = 0;
    int ret = 0;

    ret = pthread_create(&select_pid, NULL, select_server_start, NULL);
    if (ret != 0) {
        Log->e(env, LOG_TAG, "pthread_create return error!");
        return false;
    }

    Log->d(env, LOG_TAG, "select server thread start sucessfully!");
    pthread_detach(select_pid);

    // 等待Select服务端线程唤醒
    pthread_mutex_lock(&select_mutex);
    pthread_cond_wait(&select_cond, &select_mutex);
    pthread_mutex_unlock(&select_mutex);

    // 判断Select服务端是否创建成功
    if(!select_init_success) {
        Log->e(env, LOG_TAG, "Start Select server failed!");
        return false;
    }

    Log->d(env, LOG_TAG, "Start Select server successfully!");
    return true;

}

// Native环境初始化函数
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_blog4jimmy_hp_selectvsepoll_MainActivity_nativeInit(JNIEnv *env, jobject instance) {

    jint ret = 0;
    Log = std::unique_ptr<NativeLog>(new NativeLog(env));
    ret = env->GetJavaVM(&java_vm);
    if(ret != JNI_OK) {
        Log->e(env, LOG_TAG, "Cat't not get Java VM!");
        return false;
    }

    pthread_mutex_init(&select_mutex, NULL);
    pthread_cond_init(&select_cond, NULL);
    pthread_mutex_init(&epoll_mutex, NULL);
    pthread_cond_init(&epoll_cond, NULL);

    return true;
}

// Epoll服务端执行线程
static void *
epoll_server_start(void *arg)
{
    JNIEnv *env;
    struct sockaddr_in server_addr, client_addr;
    int epoll_server_fd = 0;
    int ret = 0;
    const int EPOLL_SIZE = 100;
    const int EPOLL_EVENT_SIZE = 100;
    struct epoll_event ev, epoll_event[EPOLL_EVENT_SIZE];
    int epfd = 0;
    char recvBuf[BUFFER_SIZE] = {0};
    char sendBuf[BUFFER_SIZE] = {0};
    char msg[BUFFER_SIZE] = {0};
    bool still_running = true;

    // 获取该线程的JNIEnv变量
    ret = java_vm->AttachCurrentThread(&env, NULL);
    if(ret != JNI_OK) {
        send_cond_signal(&epoll_mutex, &epoll_cond, &epoll_init_sucess, false);
        return NULL;
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    epoll_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (epoll_server_fd < 0) {
        Log->e(env, LOG_TAG, "Unable to create socket!");
        send_cond_signal(&epoll_mutex, &epoll_cond, &epoll_init_sucess, false);
        return NULL;
    }

    server_addr.sin_port = htons(EPOLL_SERVER_PORT);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    ret = bind(epoll_server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        Log->e(env, LOG_TAG, "Unable to bind server!");
        send_cond_signal(&epoll_mutex, &epoll_cond, &epoll_init_sucess, false);
        close(epoll_server_fd);
        return NULL;
    }

    listen(epoll_server_fd, 100);

    // 创建epoll文件句柄
    epfd = epoll_create(EPOLL_SIZE);
    if(epfd < 0) {
        Log->e(env, LOG_TAG, "Unable to create epoll fd!");
        send_cond_signal(&epoll_mutex, &epoll_cond, &epoll_init_sucess, false);
        return NULL;
    }

    // 将服务端socket句柄设置到epoll句柄中
    ev.data.fd = epoll_server_fd;
    ev.events = EPOLLIN | EPOLLET;

    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, epoll_server_fd, &ev);
    if(ret < 0) {
        Log->e(env, LOG_TAG, "Unable to add epoll fd!");
        send_cond_signal(&epoll_mutex, &epoll_cond, &epoll_init_sucess, false);
        close(epfd);
        return NULL;
    }

    // 通知父线程epoll服务端执行线程创建成功
    send_cond_signal(&epoll_mutex, &epoll_cond, &epoll_init_sucess, true);


    while (still_running) {
        int event_size = epoll_wait(epfd, epoll_event, EPOLL_EVENT_SIZE, -1);

        if (event_size < 0) {
            Log->e(env, LOG_TAG, "epoll wait failed!");
            continue;
        }

        if (event_size == 0) {
            Log->e(env, LOG_TAG, "epoll wait time out!");
            continue;
        }

        snprintf(msg, BUFFER_SIZE, "%d requests need to process!", event_size);
        Log->d(env, LOG_TAG, msg);

        for (int i = 0; i < event_size; ++i) {
            // 有客户端连接请求
            if (epoll_event[i].data.fd == epoll_server_fd) {
                Log->d(env, LOG_TAG, "New client comming!");
                int client_addr_len = sizeof(struct sockaddr_in);
                int client_fd = accept(epoll_server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
                if(client_fd < 0) {
                    Log->e(env, LOG_TAG, "accept client failed!");
                    continue;
                }

                // 将新连接到来的客户端句柄添加到epoll监听句柄中
                struct epoll_event client_epoll_event;
                client_epoll_event.events = EPOLLIN | EPOLLET;
                client_epoll_event.data.fd = client_fd;
                ret = epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_epoll_event);
                if(ret < 0) {

                    Log->e(env, LOG_TAG, "epoll add client fd failed!");
                    close(client_fd);
                    continue;
                }
                Log->d(env, LOG_TAG, "Client accept finish!");
                continue;
            }
            // 处理客户端发送的数据
            if (epoll_event[i].events & EPOLLIN) {
                int client_fd = epoll_event[i].data.fd;
                int recv_len = recv(client_fd, recvBuf, BUFFER_SIZE, 0);
                // 客户端已断开获取读取失败，统一处理为关闭端口
                if(recv_len <= 0) {
                    if (recv_len == 0)
                        snprintf(msg, BUFFER_SIZE, "clientfd:%d disconnect!", client_fd);
                    else
                        snprintf(msg, BUFFER_SIZE, "clientfd:%d recv from remote failed!", client_fd);
                    Log->d(env, LOG_TAG, msg);
                    // 移除端口在epoll句柄中的监听
                    struct epoll_event client_epoll_event;
                    client_epoll_event.events = EPOLLIN | EPOLLET;
                    client_epoll_event.data.fd = client_fd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, &client_epoll_event);
                    close(client_fd);
                    continue;
                }

                memset(msg, 0, BUFFER_SIZE);
                snprintf(msg, BUFFER_SIZE, "client:%d - msg:%s", client_fd, recvBuf);
                Log->d(env, LOG_TAG, msg);
                send(client_fd, msg, BUFFER_SIZE, 0);
            }


        }

    }
}

// JNI调用，用于创建epoll服务端执行线程
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_blog4jimmy_hp_selectvsepoll_MainActivity_startEpollServer(JNIEnv *env, jobject instance) {

    pthread_t epoll_pid = 0;
    int ret = 0;

    ret = pthread_create(&epoll_pid, NULL, epoll_server_start, NULL);
    if (ret != 0) {
        Log->e(env, LOG_TAG, "pthread_create return error!");
        return false;
    }

    Log->d(env, LOG_TAG, "epoll server thread start sucessfully!");
    pthread_detach(epoll_pid);

    // 等待epoll服务端执行线程唤醒
    pthread_mutex_lock(&epoll_mutex);
    pthread_cond_wait(&epoll_cond, &epoll_mutex);
    pthread_mutex_unlock(&epoll_mutex);

    if(!epoll_init_sucess) {
        Log->e(env, LOG_TAG, "Create epoll server failed!");
        return false;
    }

    Log->d(env, LOG_TAG, "Create epoll server sucessfully");
    return true;

}

// select客户端测试函数
extern "C"
JNIEXPORT jint JNICALL
Java_com_blog4jimmy_hp_selectvsepoll_MainActivity_testSelectClient(JNIEnv *env, jobject instance) {

    int ret = 0;
    struct sockaddr_in server_addr, client_addr;
    int client_fd[100];
    char recvBuf[BUFFER_SIZE];
    char sendBuf[BUFFER_SIZE] = "Hello from select client!";
    char msg[BUFFER_SIZE];

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    for (int i = 0; i < 100; ++i) {
        client_fd[i] = socket(AF_INET, SOCK_STREAM, 0);
        if(client_fd[i] < 0) {
            Log->e(env, LOG_TAG, "create test select client socket failed!");
            return -1;
        }
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = ntohs(SELECT_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    Log->d(env, LOG_TAG, "start to connect to select server!");
    time_t start = time(NULL);
    // 创建100个socket端口
    for (int i = 0; i < 100; ++i) {
        ret = connect(client_fd[i], (struct sockaddr *)&server_addr, sizeof(sockaddr_in));
        if (ret < 0) {
            Log->e(env, LOG_TAG, "select client connect to server failed!");
            return -1;
        }
    }

    // 这里单线程测试50000次，所以在Java中调用该测试函数时会出现界面卡主的情况
    for (int i = 0; i < 50000; ++i) {
        srand(time(NULL) + i);
        int client_no = rand() % 100;
        int client = client_fd[client_no];
        int sendlen = send(client, sendBuf, BUFFER_SIZE, 0);
        if (sendlen < 0) {
            Log->e(env, LOG_TAG, "Send msg to server failed!");
        }

        int recvlen = recv(client, recvBuf, BUFFER_SIZE, 0);
        if (recvlen < 0) {
            Log->e(env, LOG_TAG, "Recv msg from server failed!");
        } else {
            snprintf(msg, BUFFER_SIZE, "msg from server: %s", recvBuf);
            Log->d(env, LOG_TAG, msg);
        }
    }

    time_t end = time(NULL);

    snprintf(msg, BUFFER_SIZE, "Test Client exit! Spend time:%d", (end - start));

    Log->d(env, LOG_TAG, msg);
    // 100个socket端口连接到服务端
    for (int i = 0; i < 100; ++i) {
        snprintf(msg, BUFFER_SIZE, "client:%d close! i = %d", client_fd[i], i);
        Log->d(env, LOG_TAG, msg);
        shutdown(client_fd[i], SHUT_RDWR);
    }

    // 返回测试结果
    return (end-start);
}

// epoll服务端测试函数
extern "C"
JNIEXPORT jint JNICALL
Java_com_blog4jimmy_hp_selectvsepoll_MainActivity_testEpollClient(JNIEnv *env, jobject instance) {

    int ret = 0;
    struct sockaddr_in server_addr, client_addr;
    int client_fd[100];
    char recvBuf[BUFFER_SIZE];
    char sendBuf[BUFFER_SIZE] = "Hello from select client!";
    char msg[BUFFER_SIZE];

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    // 创建100个socket句柄
    for (int i = 0; i < 100; ++i) {
        client_fd[i] = socket(AF_INET, SOCK_STREAM, 0);
        if(client_fd[i] < 0) {
            Log->e(env, LOG_TAG, "create test select client socket failed!");
            return -1;
        }
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = ntohs(EPOLL_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    Log->d(env, LOG_TAG, "start to connect to select server!");
    time_t start = time(NULL);
    for (int i = 0; i < 100; ++i) {
        ret = connect(client_fd[i], (struct sockaddr *)&server_addr, sizeof(sockaddr_in));
        if (ret < 0) {
            Log->e(env, LOG_TAG, "select client connect to server failed!");
            return -1;
        }
    }

    // 单线程测试发送接收50000次，所以点击测试后，主界面会卡主
    for (int i = 0; i < 50000; ++i) {
        srand(time(NULL) + i);
        int client_no = rand() % 100;
        int client = client_fd[client_no];
        int sendlen = send(client, sendBuf, BUFFER_SIZE, 0);
        if (sendlen < 0) {
            Log->e(env, LOG_TAG, "Send msg to server failed!");
        }

        int recvlen = recv(client, recvBuf, BUFFER_SIZE, 0);
        if (recvlen < 0) {
            Log->e(env, LOG_TAG, "Recv msg from server failed!");
        } else {
            snprintf(msg, BUFFER_SIZE, "msg from server: %s", recvBuf);
            Log->d(env, LOG_TAG, msg);
        }
    }

    time_t end = time(NULL);

    snprintf(msg, BUFFER_SIZE, "Test Client exit! Spend time:%d", (end - start));

    Log->d(env, LOG_TAG, msg);
    for (int i = 0; i < 100; ++i) {
        shutdown(client_fd[i], SHUT_RDWR);
    }

    // 返回测试结果
    return (end-start);

}