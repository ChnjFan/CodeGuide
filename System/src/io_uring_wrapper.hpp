/**
 * @file io_uring_wrapper.hpp
 * @brief io_uring 异步IO内核接口的C++封装
 *
 * io_uring是Linux 5.1引入的高性能异步IO接口，它通过共享内存的环形缓冲区
 * 实现用户态和内核态之间的零拷贝通信，避免了传统AIO的系统调用开销。
 *
 * 核心概念：
 * 1. SQ (Submission Queue): 提交队列，用户向内核提交IO请求
 * 2. CQ (Completion Queue): 完成队列，内核向用户返回IO完成事件
 * 3. SQE (Submission Queue Entry): 提交队列项，描述一个IO请求
 * 4. CQE (Completion Queue Entry): 完成队列项，描述一个IO完成结果
 *
 * 工作流程：
 * ┌─────────────────────────────────────────────────────────────┐
 * │                        用户空间                              │
 * │  ┌─────────┐    ┌──────────┐    ┌─────────┐                │
 * │  │ 准备SQE │───>│ 提交到SQ │───>│ 等待CQE │                │
 * │  └─────────┘    └──────────┘    └─────────┘                │
 * │       │              │               ▲                      │
 * └───────│──────────────│───────────────│──────────────────────┘
 *         │              │               │
 *    ┌────▼──────────────▼───────────────│────────────────┐
 *    │                  内核空间                           │
 *    │   ┌──────────┐  ┌──────────┐  ┌──────────┐        │
 *    │   │ 处理请求 │─>│ 执行IO   │─>│ 写入CQE  │        │
 *    │   └──────────┘  └──────────┘  └──────────┘        │
 *    └────────────────────────────────────────────────────┘
 */

#ifndef IO_URING_WRAPPER_HPP
#define IO_URING_WRAPPER_HPP

#include <liburing.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>

namespace asyncio {

/**
 * @brief IO操作类型枚举
 *
 * 定义了io_uring支持的各种异步操作类型
 */
enum class IoOpType {
    READ,           // 读操作 (preadv2)
    WRITE,          // 写操作 (pwritev2)
    FSYNC,          // 文件同步
    FDATASYNC,      // 数据同步（不包含元数据）
    POLL_ADD,       // 添加poll监听
    POLL_REMOVE,    // 移除poll监听
    TIMEOUT,        // 超时操作
    ACCEPT,         // 接受连接（网络）
    CONNECT,        // 建立连接（网络）
    SEND,           // 发送数据（网络）
    RECV,           // 接收数据（网络）
    OPENAT,         // 打开文件
    CLOSE,          // 关闭文件
    NOP             // 空操作（用于测试）
};

/**
 * @brief IO操作结果结构体
 *
 * 封装了io_uring完成队列项(CQE)的结果信息
 */
struct IoResult {
    int32_t result;         // 操作结果：成功时为传输字节数，失败时为负的errno
    uint32_t flags;         // 标志位
    void* user_data;        // 用户数据指针

    /**
     * @brief 检查操作是否成功
     * @return 成功返回true，失败返回false
     */
    bool is_success() const { return result >= 0; }

    /**
     * @brief 获取错误码
     * @return 错误码（正数），如果操作成功则返回0
     */
    int get_error() const { return result < 0 ? -result : 0; }

    /**
     * @brief 获取传输的字节数
     * @return 字节数，如果操作失败则返回0
     */
    size_t bytes_transferred() const { return result > 0 ? static_cast<size_t>(result) : 0; }
};

/**
 * @brief IO请求上下文
 *
 * 存储单个IO请求的所有相关信息，包括：
 * - 操作类型
 * - 文件描述符
 * - 缓冲区信息
 * - 回调函数
 * - 协程句柄（用于协程恢复）
 */
// 支持回调模式和协程模式
// 回调模式：每个IO请求提交后，注册一个回调函数，当IO完成时调用该函数。
// 协程模式：每个IO请求提交后，返回一个协程句柄，调用者可以在协程中等待IO完成，IO完成时恢复协程执行。
struct IoContext {
    IoOpType op_type;                           // 操作类型
    int fd;                                      // 文件描述符
    void* buffer;                                // 数据缓冲区
    size_t length;                               // 操作长度
    off_t offset;                                // 文件偏移量
    std::function<void(const IoResult&)> callback;  // 完成回调
    void* coroutine_handle;                      // 协程句柄（用于co_await）
    uint64_t id;                                 // 请求唯一ID

    IoContext()
        : op_type(IoOpType::NOP)
        , fd(-1)
        , buffer(nullptr)
        , length(0)
        , offset(0)
        , coroutine_handle(nullptr)
        , id(0) {}
};

/**
 * @brief io_uring封装类
 *
 * 这个类封装了Linux io_uring的核心功能，提供了：
 * 1. 初始化和清理io_uring实例
 * 2. 提交各种类型的IO请求
 * 3. 处理IO完成事件
 * 4. 支持同步等待和异步回调两种模式
 *
 * 使用示例：
 * @code
 * IoUring ring(256);  // 创建队列深度为256的io_uring实例
 *
 * // 提交读请求
 * IoContext ctx;
 * ctx.op_type = IoOpType::READ;
 * ctx.fd = fd;
 * ctx.buffer = buffer;
 * ctx.length = 4096;
 * ctx.offset = 0;
 * ctx.callback = [](const IoResult& result) {
 *     if (result.is_success()) {
 *         std::cout << "读取了 " << result.bytes_transferred() << " 字节\n";
 *     }
 * };
 *
 * ring.submit_request(ctx);
 * ring.process_completions();  // 处理完成事件
 * @endcode
 *
 * 性能优化要点：
 * 1. 队列深度应该根据并发IO数量合理设置
 * 2. 使用IORING_SETUP_SQPOLL可以避免submit系统调用
 * 3. 批量提交和批量处理可以显著提高吞吐量
 */
class IoUring {
public:
    /**
     * @brief 构造函数
     * @param queue_depth 队列深度，决定了可以同时处理的IO请求数量
     * @param flags io_uring初始化标志
     *
     * 队列深度的选择：
     * - 对于低延迟场景（如数据库）：32-128
     * - 对于高吞吐场景（如文件服务器）：256-1024
     * - 内存消耗约为：queue_depth * (sizeof(SQE) + sizeof(CQE)) ≈ queue_depth * 80字节
     *
     * @throws std::runtime_error 如果io_uring初始化失败
     */
    explicit IoUring(unsigned int queue_depth = 256, unsigned int flags = 0);

    /**
     * @brief 析构函数
     *
     * 清理io_uring资源，包括：
     * 1. 等待所有pending的IO请求完成
     * 2. 释放io_uring实例
     * 3. 清理内部数据结构
     */
    ~IoUring();

    // 禁止拷贝（io_uring资源不可复制）
    IoUring(const IoUring&) = delete;
    IoUring& operator=(const IoUring&) = delete;

    // 允许移动
    IoUring(IoUring&& other) noexcept;
    IoUring& operator=(IoUring&& other) noexcept;

    /**
     * @brief 提交IO请求
     * @param ctx IO请求上下文
     * @return 请求ID，用于取消或查询请求状态
     *
     * 这个函数做了以下事情：
     * 1. 从SQ获取一个空闲的SQE
     * 2. 根据操作类型填充SQE
     * 3. 设置用户数据指针（用于回调）
     * 4. 提交到内核
     *
     * 注意：提交后请求可能还没有被内核处理，
     * 需要调用process_completions()来处理完成事件
     */
    uint64_t submit_request(IoContext& ctx);

    /**
     * @brief 提交读请求（便捷接口）
     * @param fd 文件描述符
     * @param buffer 读取缓冲区
     * @param length 读取长度
     * @param offset 文件偏移
     * @param callback 完成回调
     * @return 请求ID
     */
    uint64_t submit_read(int fd, void* buffer, size_t length, off_t offset,
                         std::function<void(const IoResult&)> callback = nullptr);

    /**
     * @brief 提交写请求（便捷接口）
     * @param fd 文件描述符
     * @param buffer 写入缓冲区
     * @param length 写入长度
     * @param offset 文件偏移
     * @param callback 完成回调
     * @return 请求ID
     */
    uint64_t submit_write(int fd, const void* buffer, size_t length, off_t offset,
                          std::function<void(const IoResult&)> callback = nullptr);

    /**
     * @brief 提交fsync请求
     * @param fd 文件描述符
     * @param datasync 是否只同步数据（不包含元数据）
     * @param callback 完成回调
     * @return 请求ID
     */
    uint64_t submit_fsync(int fd, bool datasync = false,
                          std::function<void(const IoResult&)> callback = nullptr);

    /**
     * @brief 处理完成队列中的事件
     * @param max_completions 最多处理的完成事件数量，0表示处理所有
     * @return 实际处理的完成事件数量
     *
     * 这个函数会：
     * 1. 检查CQ中是否有完成的IO
     * 2. 对每个完成的IO调用对应的回调函数
     * 3. 恢复等待中的协程
     *
     * 非阻塞版本，如果没有完成事件则立即返回
     */
    size_t process_completions(size_t max_completions = 0);

    /**
     * @brief 等待并处理完成事件
     * @param min_completions 最少等待的完成事件数量
     * @param timeout_ms 超时时间（毫秒），-1表示无限等待
     * @return 实际处理的完成事件数量
     *
     * 阻塞版本，会等待直到：
     * 1. 有足够数量的完成事件
     * 2. 或者超时
     */
    size_t wait_and_process(size_t min_completions = 1, int timeout_ms = -1);

    /**
     * @brief 取消IO请求
     * @param request_id 要取消的请求ID
     * @return 是否成功提交取消请求
     *
     * 注意：取消请求本身也是异步的，需要等待CQE来确认取消是否成功
     */
    bool cancel_request(uint64_t request_id);

    /**
     * @brief 获取pending的请求数量
     * @return 尚未完成的IO请求数量
     */
    size_t pending_count() const { return pending_requests_.load(); }

    /**
     * @brief 检查io_uring是否有效
     * @return 有效返回true
     */
    bool is_valid() const { return initialized_; }

    /**
     * @brief 获取队列深度
     * @return 队列深度
     */
    unsigned int queue_depth() const { return queue_depth_; }

    /**
     * @brief 获取io_uring特性标志
     * @return 支持的特性标志位
     *
     * 可以用这个函数检查内核支持哪些io_uring特性，例如：
     * - IORING_FEAT_SINGLE_MMAP
     * - IORING_FEAT_NODROP
     * - IORING_FEAT_SUBMIT_STABLE
     */
    uint32_t get_features() const;

    /**
     * @brief 获取原始的io_uring指针（高级用法）
     * @return io_uring结构体指针
     *
     * 警告：直接操作io_uring可能导致状态不一致
     */
    struct io_uring* raw() { return &ring_; }

private:
    struct io_uring ring_;                      // io_uring实例
    unsigned int queue_depth_;                   // 队列深度
    bool initialized_;                           // 是否已初始化
    std::atomic<uint64_t> next_request_id_;     // 下一个请求ID
    std::atomic<size_t> pending_requests_;      // pending请求计数

    // 请求上下文管理
    std::mutex contexts_mutex_;
    std::unordered_map<uint64_t, std::unique_ptr<IoContext>> contexts_;

    /**
     * @brief 准备SQE（内部函数）
     * @param ctx IO上下文
     * @return 准备好的SQE指针，失败返回nullptr
     */
    struct io_uring_sqe* prepare_sqe(IoContext& ctx);

    /**
     * @brief 处理单个CQE（内部函数）
     * @param cqe 完成队列项
     */
    void handle_cqe(struct io_uring_cqe* cqe);
};

/**
 * @brief 事件循环类
 *
 * 管理io_uring的事件循环，支持多线程环境
 *
 * 这个类实现了一个简单但高效的事件循环：
 * 1. 在单独的线程中运行
 * 2. 持续处理IO完成事件
 * 3. 支持优雅停止
 *
 * 事件循环的工作方式：
 * ┌─────────────────────────────────────────┐
 * │               事件循环线程               │
 * │  ┌───────────────────────────────────┐  │
 * │  │         while (running_)          │  │
 * │  │  ┌─────────────────────────────┐  │  │
 * │  │  │  io_uring_wait_cqe()        │  │  │
 * │  │  │  (阻塞等待完成事件)          │  │  │
 * │  │  └─────────────────────────────┘  │  │
 * │  │              ▼                    │  │
 * │  │  ┌─────────────────────────────┐  │  │
 * │  │  │  process_completions()      │  │  │
 * │  │  │  (处理完成事件、调用回调)     │  │  │
 * │  │  └─────────────────────────────┘  │  │
 * │  └───────────────────────────────────┘  │
 * └─────────────────────────────────────────┘
 */
class EventLoop {
public:
    /**
     * @brief 构造函数
     * @param ring 关联的IoUring实例
     */
    explicit EventLoop(IoUring& ring);

    /**
     * @brief 析构函数
     *
     * 会自动停止事件循环并等待线程结束
     */
    ~EventLoop();

    // 禁止拷贝和移动
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    /**
     * @brief 启动事件循环
     *
     * 在新线程中启动事件循环，非阻塞返回
     */
    void start();

    /**
     * @brief 停止事件循环
     *
     * 设置停止标志，事件循环会在处理完当前事件后停止
     */
    void stop();

    /**
     * @brief 等待事件循环结束
     *
     * 阻塞直到事件循环线程结束
     */
    void join();

    /**
     * @brief 检查事件循环是否在运行
     * @return 运行中返回true
     */
    bool is_running() const { return running_.load(); }

    /**
     * @brief 在事件循环线程中执行函数
     * @param func 要执行的函数
     *
     * 这个函数是线程安全的，可以从任何线程调用
     */
    void post(std::function<void()> func);

private:
    IoUring& ring_;
    std::atomic<bool> running_;
    std::thread loop_thread_;

    // 任务队列
    std::mutex task_mutex_;
    std::queue<std::function<void()>> task_queue_;

    /**
     * @brief 事件循环主函数
     */
    void run();

    /**
     * @brief 处理任务队列
     */
    void process_tasks();
};

} // namespace asyncio

#endif // IO_URING_WRAPPER_HPP
