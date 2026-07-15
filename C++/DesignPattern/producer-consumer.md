# 生产者消费者模式

生产者消费者是经典**并发设计模式**，通过一个**缓冲区（队列）** 解耦两类线程：

- **生产者线程**：负责生成任务 / 数据，放入缓冲区；

- **消费者线程**：从缓冲区取出任务 / 数据，执行业务；

缓冲区作为中间隔离层，平衡两边线程的处理速度。

例如项目中的网络连接接收和处理消息，使用生产者消费者模式来平衡消息接收速度和处理速度。

这样做的好处是，IO 线程不需要每条消息都等到处理完才能接收下一条消息，通过 IO 线程和业务线程异步并行来提升吞吐量。

多生产者、消费者同时读写队列，保证线程安全、无数据错乱、无重复丢失。

```cpp
class LogicSystem : Singleton<LogicSystem> {
public:
    void insertMsg(const std::shared_ptr<MsgNode>& msg) {
        std::unique_lock<std::mutex> lock(mtx_);
        requests.push(msg);
        if (!request.empty()) {
            lock.unlock();
            cond_.notify_one();
        }
    }
    
    void dealMsg() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx_);
            conv_.wait(lock, [this](){
                return !requests.empty();
            });
            auto msg = request.front();
            request.pop();
            // ... 处理 msg
        }
    } 
private:
    LogicSystem() {
        worker_ = std::thread(&LogicSystem::dealMsg, this);
    }

    friend class Singleton<LogicSystem>;
    std::mutex mtx_;
    std::condition_variable cond_;
    std::queue<std::shared_ptr<MsgNode> requests;
    std::thread worker_;
};
```

上面的示例中使用了无界队列，生产中必须使用有界队列防止消息无限堆积 OOM。

实现思路：通过两个条件变量分别表示队列空和队列满，如果队列满之后生产者等待消费者消费完数据后通知队列有空位，然后生产者再将数据加入到队列中。
