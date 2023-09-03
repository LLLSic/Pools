#pragma once
#include <queue>
#include <pthread.h>

using callback = void (*) (void* arg); //进行重定义，或者typedef别名也可以

//任务结构体
template <typename T>
struct Task {
    Task () 
    {
        function = nullptr;
        arg = nullptr;
    }
    Task(callback f, void *arg) {  //有参构造
        this->arg = (T*)arg;;
        function = f;
    }
    callback function; //void (*function) (void* arg);
    T * arg;
};

template <typename T>
class TaskQueue 
{
public:
    TaskQueue();
    ~TaskQueue();

    //添加任务
    void addTask(Task<T> task);
    void addTask(callback f, void *arg);

    //取出一个任务
    Task<T> takeTask();

    //获取当前任务个数
    inline int taskNumber() //内联不会压栈，提高效率
    {
        return m_taskQ.size();
    }
private:
    pthread_mutex_t m_mutex;
    std::queue<Task<T> > m_taskQ; 
};