
#include "ThreadPool.h"
#include "ThreadPool.cpp"  //解决：template is not enough
#include <unistd.h> //使用usleep需要
#include <stdio.h>

void taskFunc(void* arg)
{
    int* numdic = (int*)arg;
    int num = *numdic;
    printf("thread %ld is working, number = %d\n", pthread_self(), num);
    usleep(2000);
}

int main()
{
    // 创建线程池
    ThreadPool<int> pool(3, 10); //min 3 max 10, queue 100
    for (int i = 0; i < 100; i++) {
        //这里参数建议的是堆内存，避免生命周期提前结束
        int* num = new int(i+100);
        pool.addTask(Task<int>(taskFunc, num)); //num is int
    }

    sleep(30);

    return 0;
}