#include "TaskQueue.h"

template <typename T>
TaskQueue<T>::TaskQueue()
{
    pthread_mutex_init(&m_mutex, NULL);
}

template <typename T>
TaskQueue<T>::~TaskQueue()
{
    pthread_mutex_destroy(&m_mutex);
}

template <typename T>
void TaskQueue<T>::addTask(Task<T> task)
{
    pthread_mutex_lock(&m_mutex);
    this->m_taskQ.push(task); 
    pthread_mutex_unlock(&m_mutex);
}

template <typename T>
void TaskQueue<T>::addTask(callback f, void *arg)
{  
    Task<T> task(f, arg);
    pthread_mutex_lock(&m_mutex);
    this->m_taskQ.push(task); 
    pthread_mutex_unlock(&m_mutex);
}

template <typename T>
Task<T> TaskQueue<T>::takeTask()
{
    Task<T> task = Task<T>();
    pthread_mutex_lock(&m_mutex);
    if (m_taskQ.size() > 0) {
        task = this->m_taskQ.front();
        this->m_taskQ.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return task;
}
