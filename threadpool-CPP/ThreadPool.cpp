#include "ThreadPool.h"
#include <iostream>
using namespace std;
#include <string.h>
#include <string>
#include <unistd.h> //使用usleep需要

template <typename T>
ThreadPool<T>::ThreadPool(int min, int max)
{
    // 任务队列
    this->m_taskQ = new TaskQueue<T>; // 3.2 放在前面的原因
    do
    {
        if (this->m_taskQ == nullptr) //其实new操作会返回异常，而不是nullptr
        {
            cout << "malloc m_taskQ failed.." << endl;
            break;
        }

        this->m_minNum = min;
        this->m_maxNum = max;
        this->m_busyNum = 0;
        this->m_liveNum = min;    // 一开始实例化min个
        this->m_exitNum = 0;
        this->m_shutdown = false; // 1.4

        pthread_create(&this->m_managerID, NULL, manager, this);
        //1.4 这里直接使用manager会报错：
        //"void *(ThreadPool::*)(void *arg)" 类型的实参与 "void *(*)(void *)" 类型的形参不兼容
        //原因：pthread绑定的"void *(*)(void *)"要的是一个非成员函数的函数指针，必须定义的时候就有一个 函数地址？但是如果只是单纯的类成员函数，必须要实例化后才会有
        //解决方法：定义成外部成员（但需要友元），或者静态成员——然后再把pool作为参数传进去
        this->m_threadIDs = new pthread_t[max];
        if (this->m_threadIDs == nullptr)
        {
            cout << "malloc thread_t[] failed.." << endl;
            break;
        }
        // 3.5先全部设为0，再创建
        memset(this->m_threadIDs, 0, sizeof(this->m_threadIDs) * max);
        // 创建alive线程
        for (int i = 0; i < this->m_liveNum; i++)
        {
            pthread_create(&this->m_threadIDs[i], NULL, worker, this);
        }

        if (pthread_mutex_init(&this->m_lock, NULL) != 0 ||
            pthread_cond_init(&this->notEmpty, NULL) != 0)
        {
            cout << "init mutex or cond failed.." << endl;
            break;
        }
        return;
    } while (0);
    //释放资源-对应初始化失败的情况
    if (this->m_threadIDs) delete [] this->m_threadIDs;
    if (this->m_taskQ) delete this->m_taskQ; 
}

template <typename T>
ThreadPool<T>::~ThreadPool()
{
    this->m_shutdown = true;
    //阻塞回收管理者
    pthread_join(this->m_managerID, NULL);
    //唤醒live个任务线程
    for (int i = 0; i < this->m_liveNum; i ++) {
        pthread_cond_signal(&this->notEmpty);
    }
    if (this->m_taskQ) delete this->m_taskQ;
    if (this->m_threadIDs) delete [] this->m_threadIDs;
    pthread_mutex_destroy(&this->m_lock);
    pthread_cond_destroy(&this->notEmpty);
    
}

template <typename T>
void ThreadPool<T>::addTask(Task<T> task)
{
    if (this->m_shutdown) {
         return;
    }
    this->m_taskQ->addTask(task);？
    pthread_cond_signal(&this->notEmpty);
}

template <typename T>
int ThreadPool<T>::getBusyNumber()
{
    pthread_mutex_lock(&this->m_lock);
    int busyNum = this->m_busyNum;
    pthread_mutex_unlock(&this->m_lock);
    return busyNum; // 1.8需要加锁
}

template <typename T>
int ThreadPool<T>::getLiveNumber()
{
    pthread_mutex_lock(&this->m_lock);
    int liveNum = this->m_liveNum;
    pthread_mutex_unlock(&this->m_lock);
    return liveNum;
}

// 线程池的一个线程的工作内容：
// 找到任务队列，执行
// 内部进行自杀
template <typename T>
void *ThreadPool<T>::worker(void *arg)
{
    // ThreadPool* pool = (ThreadPool*)arg;
    ThreadPool<T>* pool = static_cast<ThreadPool<T> *>(arg);
    
    while (1)
    {
        // 1.8会对pool内部进行改变-加锁
        pthread_mutex_lock(&pool->m_lock);
        while (pool->m_taskQ->taskNumber() <= 0 && !pool->m_shutdown)
        {
            pthread_cond_wait(&pool->notEmpty, &pool->m_lock);
        }
        // 唤醒，但有线程需要自杀，这个线程自杀
        if (pool->m_exitNum > 0)
        {
            pool->m_exitNum--; // 2.10 先减少，后销毁，避免：一直大于0，但live确实没办法减少了
            if (pool->m_liveNum > pool->m_minNum)
            {
                pool->m_liveNum--;
                // 需要先解锁，然后自己去销毁（退出并从记录里面消失）
                pthread_mutex_unlock(&pool->m_lock);
                pool->threadExit();

            }
        }
        // 唤醒，但shutdown
        if (pool->m_shutdown)
        {
            // 需要先解锁，然后自己去销毁（退出并从记录里面消失）
            pthread_mutex_unlock(&pool->m_lock);
            pool->threadExit();
        }
        // 取任务
        Task<T> task = pool->m_taskQ->takeTask();

        pool->m_busyNum++;
        pthread_mutex_unlock(&pool->m_lock);

        // 开始执行
        cout << "thread " << to_string(pthread_self()) << " start working" << endl;
        task.function(task.arg);
        // 建议传参为堆内存
        if (task.arg) delete task.arg;  //可能出现问题：因为delete void* 只会delete4个字节，如果参数大于4个字节，会释放不完全——使用模板10.0
        task.arg = NULL;
        cout << "thread " << to_string(pthread_self()) << " end working" << endl;

        // 执行结束
        pthread_mutex_lock(&pool->m_lock);
        pool->m_busyNum--;
        pthread_mutex_unlock(&pool->m_lock);
    }
    return nullptr;
}

template <typename T>
// 对线程池中的任务线程进行更改数量，销毁
void *ThreadPool<T>::manager(void *arg)
{
    ThreadPool<T>* pool = static_cast<ThreadPool<T>*>(arg);
    while (!pool->m_shutdown)
    {
        sleep(3);
        int minNum = pool->m_minNum; //这个就是只读的，完全不会写
        int maxNum = pool->m_maxNum;

        //2.13 取数量
        pthread_mutex_lock(&pool->m_lock);
        int queueSize = pool->m_taskQ->taskNumber();
        int busyNum = pool->m_busyNum;
        int liveNum = pool->m_liveNum;
        pthread_mutex_unlock(&pool->m_lock);

        //添加线程
        //执行规则：任务的个数>存活的线程个数(需要保证一次)-busyNum
        //&& 存活的线程数<最大线程数（需要保证两次）
        //每次最多添加2(ADDTHREADNUMBERPERTIME)个
        if (queueSize > liveNum-busyNum && liveNum < maxNum) {
            //找到数组中0的，把线程地址放进去
            int counterHasAdd = 0;
            pthread_mutex_lock(&pool->m_lock);
            for (int j = 0; j < maxNum
                    && counterHasAdd < NUBERCHANGE
                    && pool->m_liveNum < pool->m_maxNum; j++) 
            {
                if (pool->m_threadIDs[j] == 0) {
                    pthread_create(&pool->m_threadIDs[j], NULL, worker, pool);
                    counterHasAdd ++;
                    pool->m_liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->m_lock);
        }

        //删除线程——通过控制this->m_exitNum使任务线程自杀
        //执行规则：忙的线程*2 < 存活的线程个数(确认一次)
        //&& 存活的线程数>最小线程数（确认两次）
        //每次添加2(ADDTHREADNUMBERPERTIME)个,如果减到最少也不够，就减到最少
        if (busyNum*2 < liveNum && liveNum > minNum) {

            pthread_mutex_lock(&pool->m_lock);
            pool->m_exitNum = 2;
            pthread_mutex_unlock(&pool->m_lock);
            int realChange = NUBERCHANGE > (liveNum - minNum) ? NUBERCHANGE : (liveNum - minNum);
            for (int i = 0; i < NUBERCHANGE && i < realChange ; i++) {
                pthread_cond_signal(&pool->notEmpty);
                //我的想法：用了同一把锁的地方，不要在锁里调这个地方的其他函数——避免死锁
            }
        }
    }
}

template <typename T>
// 线程从live变成不可用（地址变成0
void ThreadPool<T>::threadExit()
{
    pthread_t tid = pthread_self();
    for (int i = 0; i < this->m_maxNum; i++)
    {
        if (tid == this->m_threadIDs[i])
        {
            // 需要从this里面删除这个线程
            this->m_threadIDs[i] = 0; // 2.12是否是这样给地址归零
            break;
        }
    }
    pthread_exit(NULL);
}
