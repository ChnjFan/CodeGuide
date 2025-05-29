#include <iostream>
#include <vector>

template <typename T>
class ArrayQueue {
private:
    int front;
    int rear;
    int capacity;
    T *val;

public:
    // 末尾留一个哨兵位置
    ArrayQueue(int t_capacity) : front(0), rear(0), capacity(t_capacity+1) {
        val = new T[capacity];
    }

    ~ArrayQueue() {
        delete[] val;
    }

    int size() {
        if (rear >= front) {
            return rear - front;
        } else {
            return rear + capacity - front;
        }
    }

    bool isEmpty() {
        return front == rear;
    }

    void push(T t_val) {
        if ((rear + 1) % capacity == front) {
            std::cout << "Queue is full." << std::endl;
            return;
        }
        val[rear] = t_val;
        rear = (rear + 1) % capacity;
    }

    T pop() {
        if (isEmpty()) {
            std::cout << "Queue is empty." << std::endl;
            throw std::out_of_range("Queue is empty.");
        }
        T res = val[front];
        front = (front + 1) % capacity;
        return res;
    }

    std::vector<T> toVector() {
        std::vector<T> res;
        for (int i = front; i != rear; i = (i + 1) % capacity) {
            res.push_back(val[i]);
        }
        return res;
    }
};

int main() {
    ArrayQueue<int> queue(5);
    queue.push(1);
    queue.push(2);
    queue.push(3);
    queue.push(4);
    queue.push(5);
    queue.push(6);
    std::cout << "Queue size: " << queue.size() << std::endl;
    std::cout << "Queue is empty: " << queue.isEmpty() << std::endl;
    std::cout << "Queue pop: " << queue.pop() << std::endl;
    std::cout << "Queue pop: " << queue.pop() << std::endl;
    std::vector<int> vec = queue.toVector();
    for (auto it : vec) {
        std::cout << it << " ";
    }
    std::cout << std::endl;
    std::cout << "Queue size: " << queue.size() << std::endl;

    queue.push(6);
    queue.push(7);
    queue.push(8);
    vec = queue.toVector();
    for (auto it : vec) {
        std::cout << it << " ";
    }
    std::cout << std::endl;
    std::cout << "Queue size: " << queue.size() << std::endl;

    return 0;
}