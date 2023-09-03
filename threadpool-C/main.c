#include <stdio.h>
#include "threadpool.h"
#include <pthread.h>
#include <unistd.h> //使用usleep需要
#include <stdlib.h> //malloc

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
    ThreadPool* pool = threadPoolCreate(3, 10, 100); //min 3 max 10, queue 100
    for (int i = 0; i < 100; i++) {
        //这里参数建议的是堆内存，避免生命周期提前结束
        int* num = (int*)malloc(sizeof(int));
        *num = i + 100;
        threadPoolAdd(pool, taskFunc, num);
    }

    sleep(30);  //让子线程都执行完

    threadPoolDestory(pool);
    return 0;
}