#ifndef EVENT_SYSTEM_H
#define EVENT_SYSTEM_H

#include <functional>
#include <memory>
#include <queue>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>

// ============================================================================
// 事件基类 - 所有事件都必须继承此类
// ============================================================================
class Event {
public:
    // 虚析构函数确保使用基类指针删除派生类对象时调用正确的析构函数
    virtual ~Event() = default;

    // 获取事件类型的唯一标识
    // std::type_index 类型索引工具，将 std::type_info 封装为可哈希的类型，让事件类型可以作为键值保存在 std::unordered_map 中
    virtual std::type_index getType() const = 0;

    // 获取事件名称（用于调试）
    virtual std::string getName() const = 0;

    // 事件优先级（数值越大优先级越高）
    int priority = 0;

    // 事件时间戳
    std::chrono::steady_clock::time_point timestamp;

    Event() : timestamp(std::chrono::steady_clock::now()) {}
};

// ============================================================================
// 事件队列项 - 用于优先级队列
// ============================================================================
struct EventQueueItem {
    std::shared_ptr<Event> event;

    EventQueueItem(std::shared_ptr<Event> e) : event(std::move(e)) {}

    // 优先级比较（优先级高的排在前面）
    bool operator<(const EventQueueItem& other) const {
        return event->priority < other.event->priority;
    }
};

// ============================================================================
// 事件监听器基类
// 类型擦除：不带模板参数的基类可以在容器 vector 中存储不同类型的事件监听器
// ============================================================================
class EventListenerBase {
public:
    virtual ~EventListenerBase() = default;
    virtual void onEvent(const std::shared_ptr<Event>& event) = 0;

    // 监听器ID（用于注销）
    size_t listenerId = 0;
};

// ============================================================================
// 具体事件监听器模板类
// ============================================================================
template<typename T>
class EventListener : public EventListenerBase {
public:
    // 事件回调函数类型，使用 std::function 封装
    // 可以接受函数指针，lambda 表达式，或绑定的函数对象，成员函数（通过 std::bind 绑定或 lambda 捕获 this 指针）
    using CallbackType = std::function<void(const std::shared_ptr<T>&)>;

    explicit EventListener(CallbackType callback)
        : callback_(std::move(callback)) {}

    void onEvent(const std::shared_ptr<Event>& event) override {
        // 类型安全的向下转换，运行时检查事件类型是否匹配，转换失败会返回 nullptr
        auto typedEvent = std::dynamic_pointer_cast<T>(event);
        if (typedEvent && callback_) {
            callback_(typedEvent);
        }
    }

private:
    CallbackType callback_;
};

// ============================================================================
// 事件系统核心类
// ============================================================================
class EventSystem {
public:
    // 获取单例实例
    static EventSystem& getInstance() {
        static EventSystem instance;
        return instance;
    }

    // 禁止拷贝和赋值
    EventSystem(const EventSystem&) = delete;
    EventSystem& operator=(const EventSystem&) = delete;

    // 启动异步事件处理线程
    void start() {
        if (running_.load()) {
            std::cout << "[EventSystem] 已经在运行中" << std::endl;
            return;
        }

        running_.store(true);
        workerThread_ = std::thread(&EventSystem::processEvents, this);
        std::cout << "[EventSystem] 异步处理线程已启动" << std::endl;
    }

    // 停止异步事件处理线程
    void stop() {
        if (!running_.load()) {
            return;
        }

        running_.store(false);
        cv_.notify_all();

        if (workerThread_.joinable()) {
            workerThread_.join();
        }

        std::cout << "[EventSystem] 异步处理线程已停止" << std::endl;
    }

    // 注册事件监听器
    template<typename T>
    size_t subscribe(typename EventListener<T>::CallbackType callback) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto listener = std::make_shared<EventListener<T>>(std::move(callback));
        listener->listenerId = nextListenerId_++;

        std::type_index typeIndex(typeid(T));
        listeners_[typeIndex].push_back(listener);

        std::cout << "[EventSystem] 注册监听器 ID=" << listener->listenerId
                  << " 事件类型=" << typeIndex.name() << std::endl;

        return listener->listenerId;
    }

    // 注销事件监听器
    template<typename T>
    bool unsubscribe(size_t listenerId) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::type_index typeIndex(typeid(T));
        auto it = listeners_.find(typeIndex);

        if (it == listeners_.end()) {
            return false;
        }

        auto& listenerList = it->second;
        auto listenerIt = std::remove_if(listenerList.begin(), listenerList.end(),
            [listenerId](const std::shared_ptr<EventListenerBase>& listener) {
                return listener->listenerId == listenerId;
            });

        if (listenerIt != listenerList.end()) {
            listenerList.erase(listenerIt, listenerList.end());
            std::cout << "[EventSystem] 注销监听器 ID=" << listenerId << std::endl;
            return true;
        }

        return false;
    }

    // 发布事件（异步）
    // 注意：优先级只对队列中的事件有效。如果需要确保多个事件按优先级处理，
    // 请使用 publishBatch() 方法批量发布。
    template<typename T>
    void publish(std::shared_ptr<T> event) {
        static_assert(std::is_base_of<Event, T>::value,
                      "T must inherit from Event");

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            eventQueue_.push(EventQueueItem(event));
            eventCount_.fetch_add(1);
        }

        cv_.notify_one();

        std::cout << "[EventSystem] 发布事件: " << event->getName()
                  << " 优先级=" << event->priority << std::endl;
    }

    // 批量发布事件（确保所有事件都入队后再唤醒处理线程）
    // 用法：当需要发布多个相关事件时，使用此方法确保优先级正确生效
    void publishBatch(std::vector<std::shared_ptr<Event>> events) {
        if (events.empty()) return;

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            for (auto& event : events) {
                eventQueue_.push(EventQueueItem(event));
                eventCount_.fetch_add(1);
                std::cout << "[EventSystem] 批量发布事件: " << event->getName()
                          << " 优先级=" << event->priority << std::endl;
            }
        }

        // 只在所有事件入队后唤醒一次
        cv_.notify_one();
    }

    // 立即分发事件（同步）
    template<typename T>
    void dispatch(std::shared_ptr<T> event) {
        static_assert(std::is_base_of<Event, T>::value,
                      "T must inherit from Event");

        std::cout << "[EventSystem] 同步分发事件: " << event->getName() << std::endl;

        std::lock_guard<std::mutex> lock(mutex_);

        std::type_index typeIndex(typeid(T));
        auto it = listeners_.find(typeIndex);

        if (it != listeners_.end()) {
            for (auto& listener : it->second) {
                listener->onEvent(event);
            }
        }
    }

    // 获取待处理事件数量
    size_t getPendingEventCount() const {
        return eventCount_.load();
    }

    // 清空事件队列
    void clearQueue() {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!eventQueue_.empty()) {
            eventQueue_.pop();
        }
        eventCount_.store(0);
        std::cout << "[EventSystem] 事件队列已清空" << std::endl;
    }

    // 析构函数
    ~EventSystem() {
        stop();
    }

private:
    EventSystem() : running_(false), nextListenerId_(1), eventCount_(0) {}

    // 事件处理线程函数
    void processEvents() {
        std::cout << "[EventSystem] 事件处理线程开始运行" << std::endl;

        while (running_.load()) {
            std::shared_ptr<Event> event;

            {
                std::unique_lock<std::mutex> lock(queueMutex_);

                // 等待事件或停止信号
                cv_.wait(lock, [this] {
                    return !eventQueue_.empty() || !running_.load();
                });

                if (!running_.load() && eventQueue_.empty()) {
                    break;
                }

                if (!eventQueue_.empty()) {
                    event = eventQueue_.top().event;
                    eventQueue_.pop();
                    eventCount_.fetch_sub(1);
                }
            }

            // 分发事件
            if (event) {
                // 复制监听器列表（避免在持有锁时调用回调导致死锁）
                std::vector<std::shared_ptr<EventListenerBase>> listenersCopy;

                {
                    std::lock_guard<std::mutex> lock(mutex_);

                    std::type_index typeIndex = event->getType();
                    auto it = listeners_.find(typeIndex);

                    if (it != listeners_.end()) {
                        std::cout << "[EventSystem] 处理事件: " << event->getName()
                                  << " 监听器数量=" << it->second.size() << std::endl;

                        // 复制监听器列表
                        listenersCopy = it->second;
                    }
                }  // 锁在这里释放

                // 在锁外调用监听器回调（安全，不会死锁）
                for (auto& listener : listenersCopy) {
                    try {
                        listener->onEvent(event);
                    } catch (const std::exception& e) {
                        std::cerr << "[EventSystem] 监听器异常: " << e.what() << std::endl;
                    }
                }
            }
        }

        std::cout << "[EventSystem] 事件处理线程结束" << std::endl;
    }

    // 监听器映射表（事件类型 -> 监听器列表）
    std::unordered_map<std::type_index, std::vector<std::shared_ptr<EventListenerBase>>> listeners_;

    // 事件优先级队列
    std::priority_queue<EventQueueItem> eventQueue_;

    // 线程同步
    std::mutex mutex_;           // 保护监听器列表
    std::mutex queueMutex_;      // 保护事件队列
    std::condition_variable cv_; // 条件变量
    std::thread workerThread_;   // 工作线程
    std::atomic<bool> running_;  // 运行状态

    // 监听器ID生成器
    size_t nextListenerId_;

    // 待处理事件计数
    std::atomic<size_t> eventCount_;
};

#endif // EVENT_SYSTEM_H
