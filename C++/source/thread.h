//
// Created by Fan on 2025/11/23.
//

#ifndef SOURCE_THREAD_H
#define SOURCE_THREAD_H

#include <thread>
#include <iostream>
#include <queue>

namespace CodeGuide {

std::mutex mtx;
std::condition_variable cv;

// 线程参数通过值传递
void producer(std::queue<int> &q) {
    for (int i = 0; i < 3; ++i) {
        std::unique_lock<std::mutex> lock(mtx);
        q.push(i);
        lock.unlock();
        cv.notify_one();
    }
}

void consumer(std::queue<int> &q) {
    for (int i = 0; i < 3; ++i) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&q] { return !q.empty(); });
        std::cout << "Consumed: " << q.front() << std::endl;
        q.pop();
    }
}

void thread_test() {
    std::queue<int> queue;
    // 线程创建后立即启动
    std::thread thread_1(producer, std::ref(queue));
    // std::ref 引用包装器，将引用 “包装” 为可拷贝、可赋值的对象。
    std::thread thread_2(consumer, std::ref(queue));

    // 如果不使用 join 阻塞等待线程，程序终止时会调用 std::terminate
    thread_1.join();
    thread_2.join();
}

}

#endif //SOURCE_THREAD_H