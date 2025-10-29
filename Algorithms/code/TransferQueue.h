/**
 * @file transfer_queue.h
 * @brief 传输队列（TransferQueue）实现 - 类似Java的TransferQueue
 *
 * TransferQueue是一个高级的线程安全队列，支持以下特性：
 * 1. 生产者可以等待消费者接收元素（transfer操作）
 * 2. 支持立即传输尝试（tryTransfer）
 * 3. 支持传统的阻塞队列操作（put/take）
 * 4. 线程安全，支持多生产者多消费者
 */

#ifndef TRANSFER_QUEUE_H
#define TRANSFER_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>
#include <memory>
#include <atomic>

/**
 * @brief 传输队列模板类
 * @tparam T 队列中存储的元素类型
 *
 * 这个类提供了一个特殊的阻塞队列，生产者可以等待消费者直接接收元素。
 * 主要特性：
 * - transfer(): 生产者将元素交给消费者，并等待消费者接收
 * - tryTransfer(): 尝试立即传输，如果没有等待的消费者则失败
 * - put()/take(): 传统的阻塞队列操作
 */
template<typename T>
class TransferQueue {
public:
    /**
     * @brief 构造函数
     * @param max_capacity 队列的最大容量，0表示无限制
     */
    explicit TransferQueue(size_t max_capacity = 0)
        : max_capacity_(max_capacity)
        , waiting_consumers_(0)
        , waiting_producers_(0)
        , closed_(false) {
    }

    // 禁止拷贝
    TransferQueue(const TransferQueue&) = delete;
    TransferQueue& operator=(const TransferQueue&) = delete;

    /**
     * @brief 传输元素给等待的消费者
     * @param item 要传输的元素
     * @return true 成功传输，false 队列已关闭
     *
     * 此方法会阻塞直到有消费者接收这个元素。
     * 与put()不同，transfer()确保元素被消费者接收后才返回。
     */
    bool transfer(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        // 如果有等待的消费者，直接传递给它
        if (waiting_consumers_ > 0) {
            queue_.push(item);
            consumer_cv_.notify_one();//唤醒一个等待的消费者

            // 等待消费者实际取走元素
            // 注意：这里我们使用一个特殊的机制来确保元素被取走
            // 消费者被唤醒后取出元素再唤醒阻塞的生产者
            waiting_producers_++;
            producer_cv_.wait(lock, [this, initial_size = queue_.size()] {
                return closed_ || queue_.size() < initial_size; // 检查队列长度防止虚假唤醒
            });
            waiting_producers_--;

            return !closed_;
        }

        // 如果有容量限制且队列已满，需要等待，一直阻塞到有容量为止
        if (max_capacity_ > 0) {
            // pred 参数：首先检查条件是否满足，只有不满足时才会释放锁阻塞线程，被唤醒后会再次执行 pred 检查
            producer_cv_.wait(lock, [this] {
                return closed_ || queue_.size() < max_capacity_;
            });

            if (closed_) {
                return false;
            }
        }

        // 放入队列并等待消费者取走
        queue_.push(item);
        size_t initial_size = queue_.size();

        consumer_cv_.notify_one();

        // 等待这个特定元素被消费
        waiting_producers_++;
        producer_cv_.wait(lock, [this, initial_size] {
            return closed_ || queue_.size() < initial_size;
        });
        waiting_producers_--;

        return !closed_;
    }

    /**
     * @brief 尝试立即传输元素
     * @param item 要传输的元素
     * @return true 成功传输，false 没有等待的消费者
     *
     * 此方法只有在有消费者正在等待时才会成功。
     * 如果没有等待的消费者，立即返回false，不会阻塞。
     */
    bool tryTransfer(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        // 只有在有等待的消费者时才传输
        if (waiting_consumers_ > 0) {
            queue_.push(item);
            consumer_cv_.notify_one();
            return true;
        }

        return false;
    }

    /**
     * @brief 尝试在指定时间内传输元素
     * @param item 要传输的元素
     * @param timeout 超时时间（毫秒）
     * @return true 成功传输，false 超时或队列已关闭
     *
     * 此方法会等待指定的时间，如果在超时前有消费者接收则返回true。
     */
    template<typename Rep, typename Period>
    bool tryTransfer(const T& item, const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        auto deadline = std::chrono::steady_clock::now() + timeout;

        // 如果有等待的消费者，直接传递
        if (waiting_consumers_ > 0) {
            queue_.push(item);
            consumer_cv_.notify_one();

            // 等待消费者取走（带超时）
            waiting_producers_++;
            bool success = producer_cv_.wait_until(lock, deadline, [this, initial_size = queue_.size()] {
                return closed_ || queue_.size() < initial_size;
            });
            waiting_producers_--;

            return success && !closed_;
        }

        // 等待消费者出现（带超时）
        bool has_consumer = consumer_appeared_cv_.wait_until(lock, deadline, [this] {
            return closed_ || waiting_consumers_ > 0;
        });

        if (!has_consumer || closed_) {
            return false;
        }

        // 现在有消费者了，放入元素
        queue_.push(item);
        size_t initial_size = queue_.size();
        consumer_cv_.notify_one();

        // 等待元素被取走（使用剩余时间）
        waiting_producers_++;
        bool success = producer_cv_.wait_until(lock, deadline, [this, initial_size] {
            return closed_ || queue_.size() < initial_size;
        });
        waiting_producers_--;

        return success && !closed_;
    }

    /**
     * @brief 放入元素到队列
     * @param item 要放入的元素
     * @return true 成功放入，false 队列已关闭
     *
     * 如果队列已满（有容量限制时），此方法会阻塞直到有空间。
     * 与transfer()不同，put()不会等待消费者接收，只要放入队列就返回。
     */
    bool put(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        // 如果有容量限制，等待直到有空间
        if (max_capacity_ > 0) {
            producer_cv_.wait(lock, [this] {
                return closed_ || queue_.size() < max_capacity_;
            });

            if (closed_) {
                return false;
            }
        }

        queue_.push(item);
        consumer_cv_.notify_one();
        return true;
    }

    /**
     * @brief 尝试放入元素（非阻塞）
     * @param item 要放入的元素
     * @return true 成功放入，false 队列已满或已关闭
     */
    bool offer(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        // 如果有容量限制且已满，直接返回false
        if (max_capacity_ > 0 && queue_.size() >= max_capacity_) {
            return false;
        }

        queue_.push(item);
        consumer_cv_.notify_one();
        return true;
    }

    /**
     * @brief 从队列取出元素
     * @param item 用于存储取出的元素
     * @return true 成功取出，false 队列已关闭
     *
     * 如果队列为空，此方法会阻塞直到有元素可用。
     */
    bool take(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        // 通知可能在等待消费者的生产者
        waiting_consumers_++;
        consumer_appeared_cv_.notify_all();

        consumer_cv_.wait(lock, [this] {
            return closed_ || !queue_.empty();
        });

        waiting_consumers_--;

        if (closed_ && queue_.empty()) {
            return false;
        }

        item = queue_.front();
        queue_.pop();

        // 通知等待的生产者（可能在transfer中等待）
        producer_cv_.notify_all();

        return true;
    }

    /**
     * @brief 尝试取出元素（非阻塞）
     * @param item 用于存储取出的元素
     * @return true 成功取出，false 队列为空或已关闭
     */
    bool poll(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        // 修复BUG：应该使用 && 而不是 ||，允许关闭后取出剩余元素
        if (closed_ && queue_.empty()) {
            return false;
        }

        // 即使关闭，只要队列非空就可以取出
        if (queue_.empty()) {
            return false;
        }

        item = queue_.front();
        queue_.pop();

        // 通知等待的生产者
        producer_cv_.notify_all();

        return true;
    }

    /**
     * @brief 带超时的取出操作
     * @param item 用于存储取出的元素
     * @param timeout 超时时间
     * @return true 成功取出，false 超时或队列已关闭
     */
    template<typename Rep, typename Period>
    bool poll(T& item, const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        // 通知可能在等待消费者的生产者
        waiting_consumers_++;
        consumer_appeared_cv_.notify_all();

        bool success = consumer_cv_.wait_for(lock, timeout, [this] {
            return closed_ || !queue_.empty();
        });

        waiting_consumers_--;

        if (!success || (closed_ && queue_.empty())) {
            return false;
        }

        item = queue_.front();
        queue_.pop();

        // 通知等待的生产者
        producer_cv_.notify_all();

        return true;
    }

    /**
     * @brief 获取队列中的元素数量
     * @return 当前队列大小
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * @brief 检查队列是否为空
     * @return true 队列为空，false 队列非空
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /**
     * @brief 检查是否有等待的消费者
     * @return true 有消费者在等待，false 没有等待的消费者
     *
     * 此方法用于判断tryTransfer()是否可能成功。
     */
    bool hasWaitingConsumer() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return waiting_consumers_ > 0;
    }

    /**
     * @brief 获取等待消费者的数量估计值
     * @return 等待的消费者数量
     */
    size_t getWaitingConsumerCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return waiting_consumers_;
    }

    /**
     * @brief 获取等待生产者的数量估计值
     * @return 等待的生产者数量
     */
    size_t getWaitingProducerCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return waiting_producers_;
    }

    /**
     * @brief 关闭队列
     *
     * 关闭队列后：
     * - 所有阻塞的操作会被唤醒并返回false
     * - 新的放入操作会失败
     * - 仍可以取出已存在的元素
     */
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        consumer_cv_.notify_all();
        producer_cv_.notify_all();
        consumer_appeared_cv_.notify_all();
    }

    /**
     * @brief 检查队列是否已关闭
     * @return true 已关闭，false 未关闭
     */
    bool isClosed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    /**
     * @brief 清空队列中的所有元素
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
        producer_cv_.notify_all();
    }

    /**
     * @brief 获取队列的最大容量
     * @return 最大容量，0表示无限制
     */
    size_t capacity() const {
        return max_capacity_;
    }

private:
    mutable std::mutex mutex_;                      // 保护所有共享数据的互斥锁
    std::condition_variable consumer_cv_;            // 队列空变为非空时，用于唤醒在 take 或 poll 操作等待的消费者
    std::condition_variable producer_cv_;            // 队列满变为不满时，唤醒 put 操作等待的生产者；元素被消费者取走唤醒 transfer 阻塞的生产者
    std::condition_variable consumer_appeared_cv_;   // 用于通知消费者出现，当消费者开始等待时，唤醒 tryTransfer 中等待的生产者

    std::queue<T> queue_;                           // 底层队列存储
    size_t max_capacity_;                           // 队列最大容量（0表示无限制）
    size_t waiting_consumers_;                      // 等待的消费者数量
    size_t waiting_producers_;                      // 等待的生产者数量
    bool closed_;                                   // 队列是否已关闭
};

#endif // TRANSFER_QUEUE_H
