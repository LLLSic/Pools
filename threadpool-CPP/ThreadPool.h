#pragma once
#include "TaskQueue.h"
#include "TaskQueue.cpp"

template <typename T>
class ThreadPool
{
public:
    ThreadPool(int min, int max);
    ~ThreadPool();

    // 添加任务
    void addTask(Task<T> task);

    // 获取线程池中工作线程个数
    int getBusyNumber();

    // 获取线程池中活着的线程个数（包括工作中的
    int getLiveNumber();

private:
    // 工作线程的任务函数
    static void *worker(void *arg);  //1.4 这里需要使用static，或者外部友元函数

    // 管理者线程的任务函数
    static void *manager(void *arg);

    // 线程退出函数
    void threadExit();  //6.1

private:
    //两种线程
    pthread_t m_managerID;    //管理线程ID
    pthread_t* m_threadIDs;   //工作线程ID，多个

    //任务队列
    TaskQueue<T>* m_taskQ; //1.3 这里也可以是变量

    int m_minNum;     //线程数量min
    int m_maxNum;
    int m_busyNum;    //当前正忙线程数——此变量会经常变化
    int m_liveNum;    //当前存活线程（包含正忙）
    int m_exitNum;    //需要销毁线程数

    bool m_shutdown;

    static const int NUBERCHANGE = 2;
    //锁--这里只用一个，锁整个线程池
    pthread_mutex_t m_lock; //1.1可以每个单独资源都对应一个锁，但可能造成死锁，这里统一一个锁，任意修改都用这个，但是牺牲了效率
    
    pthread_cond_t notEmpty;
};