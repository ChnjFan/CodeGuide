/**
 * @file io_uring_wrapper.cpp
 * @brief io_uring封装类的实现
 *
 * 实现要点：
 * 1. io_uring的初始化需要考虑内核版本兼容性
 * 2. SQE的获取可能失败（队列满），需要处理这种情况
 * 3. CQE的处理需要注意内存管理和异常安全
 * 4. 多线程环境下需要适当的同步机制
 */

#include "io_uring_wrapper.hpp"
#include <iostream>
#include <cerrno>
#include <cstring>
#include <cassert>

namespace asyncio {

// ============================================================================
// IoUring 类实现
// ============================================================================

IoUring::IoUring(unsigned int queue_depth, unsigned int flags)
    : queue_depth_(queue_depth)
    , initialized_(false)
    , next_request_id_(1)
    , pending_requests_(0)
{
    /**
     * io_uring_queue_init_params 函数初始化io_uring实例
     *
     * 参数说明：
     * - queue_depth: 队列深度，决定SQ和CQ的大小
     *   SQ大小 = queue_depth
     *   CQ大小 = queue_depth * 2 (默认，可通过params修改)
     *
     * - ring: io_uring结构体指针
     *
     * - params: 可选的初始化参数，包括：
     *   - sq_entries: SQ条目数
     *   - cq_entries: CQ条目数
     *   - flags: 初始化标志
     *   - sq_thread_cpu: SQ轮询线程绑定的CPU
     *   - sq_thread_idle: SQ轮询线程空闲超时(ms)
     *
     * 常用flags：
     * - IORING_SETUP_SQPOLL: 内核轮询SQ，避免submit系统调用
     * - IORING_SETUP_IOPOLL: 使用忙轮询而非中断
     * - IORING_SETUP_SINGLE_ISSUER: 优化单一提交者场景
     */

    // 初始化io_uring参数结构体
    struct io_uring_params params;
    std::memset(&params, 0, sizeof(params));
    params.flags = flags;

    // 初始化io_uring
    // 返回0表示成功，负值表示错误
    int ret = io_uring_queue_init_params(queue_depth, &ring_, &params);
    if (ret < 0) {
        // 初始化失败，构造详细的错误信息
        std::string error_msg = "io_uring初始化失败: ";
        error_msg += strerror(-ret);
        error_msg += " (错误码: " + std::to_string(-ret) + ")";

        // 常见错误原因分析
        if (ret == -ENOMEM) {
            error_msg += "\n原因: 内存不足，可能是queue_depth设置过大";
        } else if (ret == -ENOSYS) {
            error_msg += "\n原因: 内核不支持io_uring，需要Linux 5.1+";
        } else if (ret == -EPERM) {
            error_msg += "\n原因: 权限不足，可能需要root权限或调整rlimit";
        }

        throw std::runtime_error(error_msg);
    }

    initialized_ = true;

    // 打印io_uring特性信息（调试用）
    #ifdef ASYNCIO_DEBUG
    std::cout << "[io_uring] 初始化成功:\n"
              << "  - 队列深度: " << queue_depth << "\n"
              << "  - SQ条目数: " << params.sq_entries << "\n"
              << "  - CQ条目数: " << params.cq_entries << "\n"
              << "  - 特性标志: 0x" << std::hex << params.features << std::dec << "\n";
    #endif
}

IoUring::~IoUring() {
    if (initialized_) {
        /**
         * 清理流程：
         * 1. 等待所有pending请求完成（可选，这里选择不等待）
         * 2. 清理上下文映射
         * 3. 释放io_uring资源
         */

        // 清理上下文
        {
            std::lock_guard<std::mutex> lock(contexts_mutex_);
            contexts_.clear();
        }

        // 释放io_uring
        // 这个函数会：
        // - 取消所有pending的请求
        // - 释放共享内存映射
        // - 关闭io_uring文件描述符
        io_uring_queue_exit(&ring_);
        initialized_ = false;
    }
}

IoUring::IoUring(IoUring&& other) noexcept
    : queue_depth_(other.queue_depth_)
    , initialized_(other.initialized_)
    , next_request_id_(other.next_request_id_.load())
    , pending_requests_(other.pending_requests_.load())
{
    if (other.initialized_) {
        // 移动io_uring结构体（浅拷贝）
        std::memcpy(&ring_, &other.ring_, sizeof(ring_));
        other.initialized_ = false;

        // 移动上下文映射
        std::lock_guard<std::mutex> lock(other.contexts_mutex_);
        contexts_ = std::move(other.contexts_);
    }
}

IoUring& IoUring::operator=(IoUring&& other) noexcept {
    if (this != &other) {
        // 先清理当前资源
        if (initialized_) {
            io_uring_queue_exit(&ring_);
        }

        // 移动其他成员
        queue_depth_ = other.queue_depth_;
        initialized_ = other.initialized_;
        next_request_id_.store(other.next_request_id_.load());
        pending_requests_.store(other.pending_requests_.load());

        if (other.initialized_) {
            std::memcpy(&ring_, &other.ring_, sizeof(ring_));
            other.initialized_ = false;

            std::lock_guard<std::mutex> lock(other.contexts_mutex_);
            contexts_ = std::move(other.contexts_);
        }
    }
    return *this;
}

struct io_uring_sqe* IoUring::prepare_sqe(IoContext& ctx) {
    /**
     * 获取一个空闲的SQE
     *
     * io_uring_get_sqe() 函数从提交队列获取一个空闲的SQE
     * 如果SQ已满，返回nullptr
     *
     * 这是一个关键的操作，因为：
     * 1. SQ的大小是有限的（queue_depth）
     * 2. 如果提交速度超过完成速度，SQ会满
     * 3. 需要处理SQ满的情况
     */
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        /**
         * SQ已满的处理策略：
         *
         * 方案1（当前采用）：尝试先提交已有的请求，然后重试
         * 方案2：直接返回失败，让调用者处理
         * 方案3：阻塞等待，直到有空闲SQE
         *
         * 这里采用方案1，因为它在大多数情况下能解决问题
         */
        int submitted = io_uring_submit(&ring_);
        if (submitted < 0) {
            throw std::runtime_error("io_uring_submit失败: " + std::string(strerror(-submitted)));
        }

        // 重试获取SQE
        sqe = io_uring_get_sqe(&ring_);
        if (!sqe) {
            throw std::runtime_error("无法获取SQE: 提交队列仍然已满");
        }
    }

    return sqe;
}

uint64_t IoUring::submit_request(IoContext& ctx) {
    if (!initialized_) {
        throw std::runtime_error("io_uring未初始化");
    }

    // 分配请求ID
    uint64_t request_id = next_request_id_.fetch_add(1);
    ctx.id = request_id;

    // 获取SQE
    struct io_uring_sqe* sqe = prepare_sqe(ctx);

    /**
     * 根据操作类型填充SQE
     *
     * io_uring支持多种操作类型，每种类型有对应的prep函数：
     * - io_uring_prep_read: 读操作
     * - io_uring_prep_write: 写操作
     * - io_uring_prep_fsync: 文件同步
     * - io_uring_prep_openat: 打开文件
     * - io_uring_prep_close: 关闭文件
     * 等等...
     *
     * 这些prep函数都是内联的，只是填充SQE结构体的字段
     */
    switch (ctx.op_type) {
        case IoOpType::READ:
            /**
             * io_uring_prep_read(sqe, fd, buf, nbytes, offset)
             *
             * 准备一个读操作：
             * - fd: 文件描述符
             * - buf: 读取数据的目标缓冲区
             * - nbytes: 要读取的字节数
             * - offset: 文件偏移量（-1表示使用当前位置）
             *
             * 底层使用preadv2系统调用
             */
            io_uring_prep_read(sqe, ctx.fd, ctx.buffer, ctx.length, ctx.offset);
            break;

        case IoOpType::WRITE:
            /**
             * io_uring_prep_write(sqe, fd, buf, nbytes, offset)
             *
             * 准备一个写操作：
             * - fd: 文件描述符
             * - buf: 要写入的数据缓冲区
             * - nbytes: 要写入的字节数
             * - offset: 文件偏移量（-1表示使用当前位置）
             *
             * 底层使用pwritev2系统调用
             */
            io_uring_prep_write(sqe, ctx.fd, ctx.buffer, ctx.length, ctx.offset);
            break;

        case IoOpType::FSYNC:
            /**
             * io_uring_prep_fsync(sqe, fd, flags)
             *
             * 准备一个文件同步操作：
             * - fd: 文件描述符
             * - flags: IORING_FSYNC_DATASYNC表示只同步数据
             */
            io_uring_prep_fsync(sqe, ctx.fd, 0);
            break;

        case IoOpType::FDATASYNC:
            // 仅保证数据块同步，不包括元数据（文件大小、事件等），适用数据库、日志等场景
            io_uring_prep_fsync(sqe, ctx.fd, IORING_FSYNC_DATASYNC);
            break;

        case IoOpType::CLOSE:
            /**
             * io_uring_prep_close(sqe, fd)
             *
             * 准备一个关闭文件操作
             * 异步关闭可以避免阻塞，特别是在关闭网络连接时
             */
            io_uring_prep_close(sqe, ctx.fd);
            break;

        case IoOpType::NOP:
            /**
             * io_uring_prep_nop(sqe)
             *
             * 空操作，用于测试io_uring是否正常工作
             */
            io_uring_prep_nop(sqe);
            break;

        default:
            throw std::runtime_error("不支持的IO操作类型: " + std::to_string(static_cast<int>(ctx.op_type)));
    }

    // 创建上下文副本并存储
    auto ctx_ptr = std::make_unique<IoContext>(ctx);

    /**
     * 设置用户数据
     *
     * io_uring_sqe_set_data设置一个用户数据指针
     * 这个数据会在CQE中原样返回，用于识别是哪个请求完成了
     *
     * 我们使用请求ID作为用户数据（转换为void*），然后通过contexts_映射找到完整的上下文
     *
     * 注意：这里使用reinterpret_cast将uint64_t转换为void*
     * 在64位系统上这是安全的，因为指针和uint64_t大小相同
     */
    io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(request_id));

    // 存储上下文
    {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        contexts_[request_id] = std::move(ctx_ptr);
    }

    /**
     * 提交请求到内核
     *
     * io_uring_submit() 将SQ中的所有待提交请求提交给内核
     *
     * 返回值：
     * - > 0: 成功提交的请求数量
     * - < 0: 错误码
     *
     * 注意：提交后请求可能还没有被处理
     * 需要通过CQ来获取完成状态
     */
    int submitted = io_uring_submit(&ring_);
    if (submitted < 0) {
        // 提交失败，清理上下文
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        contexts_.erase(request_id);
        throw std::runtime_error("io_uring_submit失败: " + std::string(strerror(-submitted)));
    }

    // 增加pending计数
    pending_requests_.fetch_add(1);

    return request_id;
}

uint64_t IoUring::submit_read(int fd, void* buffer, size_t length, off_t offset,
                               std::function<void(const IoResult&)> callback) {
    IoContext ctx;
    ctx.op_type = IoOpType::READ;
    ctx.fd = fd;
    ctx.buffer = buffer;
    ctx.length = length;
    ctx.offset = offset;
    ctx.callback = std::move(callback);
    return submit_request(ctx);
}

uint64_t IoUring::submit_write(int fd, const void* buffer, size_t length, off_t offset,
                                std::function<void(const IoResult&)> callback) {
    IoContext ctx;
    ctx.op_type = IoOpType::WRITE;
    ctx.fd = fd;
    ctx.buffer = const_cast<void*>(buffer);  // io_uring需要非const指针
    ctx.length = length;
    ctx.offset = offset;
    ctx.callback = std::move(callback);
    return submit_request(ctx);
}

uint64_t IoUring::submit_fsync(int fd, bool datasync,
                                std::function<void(const IoResult&)> callback) {
    IoContext ctx;
    ctx.op_type = datasync ? IoOpType::FDATASYNC : IoOpType::FSYNC;
    ctx.fd = fd;
    ctx.callback = std::move(callback);
    return submit_request(ctx);
}

void IoUring::handle_cqe(struct io_uring_cqe* cqe) {
    /**
     * 处理完成队列项(CQE)
     *
     * CQE结构体包含：
     * - user_data: 提交时设置的用户数据（这里是请求ID）
     * - res: 操作结果（成功时为字节数，失败时为负的errno）
     * - flags: 标志位
     */

    // 获取请求ID
    // 使用io_uring_cqe_get_data获取void*，然后转换为uint64_t
    uint64_t request_id = reinterpret_cast<uint64_t>(io_uring_cqe_get_data(cqe));

    // 构造结果结构体
    IoResult result;
    result.result = cqe->res;
    result.flags = cqe->flags;
    result.user_data = nullptr;

    // 查找并获取上下文
    std::unique_ptr<IoContext> ctx;
    {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        auto it = contexts_.find(request_id);
        if (it != contexts_.end()) {
            ctx = std::move(it->second);
            contexts_.erase(it);
        }
    }

    // 减少pending计数
    pending_requests_.fetch_sub(1);

    if (ctx) {
        result.user_data = ctx.get();

        /**
         * 调用回调函数
         *
         * 回调函数在io_uring处理线程中执行，需要注意：
         * 1. 回调应该尽快返回，避免阻塞后续CQE处理
         * 2. 如果需要执行耗时操作，应该投递到工作线程
         * 3. 回调中的异常需要捕获，否则可能导致程序崩溃
         */
        if (ctx->callback) {
            try {
                ctx->callback(result);
            } catch (const std::exception& e) {
                std::cerr << "[io_uring] 回调异常: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[io_uring] 回调发生未知异常" << std::endl;
            }
        }

        /**
         * 恢复协程
         *
         * 如果这是一个协程发起的IO操作，coroutine_handle会指向
         * 等待中的协程，需要恢复它的执行
         *
         * 协程恢复的时机很重要：
         * 1. 必须在回调之后，因为回调可能需要先处理结果
         * 2. 恢复后协程会从co_await处继续执行
         */
        if (ctx->coroutine_handle) {
            // 协程恢复在coroutine_support.hpp中实现
            // 这里只是标记需要恢复
        }
    }
}

size_t IoUring::process_completions(size_t max_completions) {
    if (!initialized_) {
        return 0;
    }

    size_t processed = 0;
    struct io_uring_cqe* cqe;

    /**
     * 非阻塞方式处理完成队列
     *
     * io_uring_peek_cqe() 不会阻塞，只是检查是否有完成的请求
     * 如果有，返回0并设置cqe指针
     * 如果没有，返回-EAGAIN
     */
    while ((max_completions == 0 || processed < max_completions)) {
        int ret = io_uring_peek_cqe(&ring_, &cqe);
        if (ret == -EAGAIN) {
            // 没有更多的完成事件
            break;
        }
        if (ret < 0) {
            throw std::runtime_error("io_uring_peek_cqe失败: " + std::string(strerror(-ret)));
        }

        // 处理CQE
        handle_cqe(cqe);

        /**
         * 标记CQE已处理
         *
         * io_uring_cqe_seen() 告诉内核这个CQE已经被处理
         * 这样内核可以重用这个CQE槽位
         *
         * 注意：必须在处理完CQE后立即调用，否则可能导致：
         * 1. CQ溢出（新的完成事件无法放入）
         * 2. 内存问题（CQE被覆盖）
         */
        io_uring_cqe_seen(&ring_, cqe);
        processed++;
    }

    return processed;
}

size_t IoUring::wait_and_process(size_t min_completions, int timeout_ms) {
    if (!initialized_) {
        return 0;
    }

    struct io_uring_cqe* cqe;
    size_t processed = 0;

    /**
     * 阻塞等待完成事件
     *
     * 有两种等待方式：
     * 1. io_uring_wait_cqe(): 无限等待，直到有CQE
     * 2. io_uring_wait_cqe_timeout(): 带超时的等待
     */
    if (timeout_ms < 0) {
        // 无限等待
        while (processed < min_completions) {
            /**
             * io_uring_wait_cqe() 阻塞等待一个CQE
             *
             * 这个函数会阻塞当前线程，直到：
             * 1. 有一个CQE可用
             * 2. 收到信号（返回-EINTR）
             */
            int ret = io_uring_wait_cqe(&ring_, &cqe);
            if (ret < 0) {
                if (ret == -EINTR) {
                    // 被信号中断，继续等待
                    continue;
                }
                throw std::runtime_error("io_uring_wait_cqe失败: " + std::string(strerror(-ret)));
            }

            handle_cqe(cqe);
            io_uring_cqe_seen(&ring_, cqe);
            processed++;

            // 处理队列中可能还有的其他CQE
            processed += process_completions(0);
        }
    } else {
        // 带超时等待
        struct __kernel_timespec ts;
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;

        while (processed < min_completions) {
            /**
             * io_uring_wait_cqe_timeout() 带超时等待CQE
             *
             * 返回值：
             * - 0: 成功获取CQE
             * - -ETIME: 超时
             * - 其他负值: 错误
             */
            int ret = io_uring_wait_cqe_timeout(&ring_, &cqe, &ts);
            if (ret == -ETIME) {
                // 超时，返回已处理的数量
                break;
            }
            if (ret < 0) {
                if (ret == -EINTR) {
                    continue;
                }
                throw std::runtime_error("io_uring_wait_cqe_timeout失败: " + std::string(strerror(-ret)));
            }

            handle_cqe(cqe);
            io_uring_cqe_seen(&ring_, cqe);
            processed++;

            // 更新剩余超时时间（简化处理，实际应该计算已用时间）
            processed += process_completions(0);
        }
    }

    return processed;
}

bool IoUring::cancel_request(uint64_t request_id) {
    if (!initialized_) {
        return false;
    }

    /**
     * 取消IO请求
     *
     * io_uring支持取消已提交但尚未完成的请求
     * 取消操作本身也是异步的：
     * 1. 提交一个IORING_OP_ASYNC_CANCEL请求
     * 2. 等待取消请求的CQE
     * 3. CQE的res字段表示取消是否成功
     */
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        return false;
    }

    /**
     * io_uring_prep_cancel() 准备取消请求
     *
     * 参数：
     * - sqe: SQE指针
     * - user_data: 要取消的请求的user_data（void*类型）
     * - flags: 取消标志
     *
     * 注意：将request_id转换为void*来匹配函数签名
     */
    io_uring_prep_cancel(sqe, reinterpret_cast<void*>(request_id), 0);

    int ret = io_uring_submit(&ring_);
    return ret > 0;
}

uint32_t IoUring::get_features() const {
    /**
     * 获取io_uring支持的特性
     *
     * 通过io_uring_params的features字段可以了解内核支持哪些特性
     * 这对于编写兼容不同内核版本的代码很有用
     *
     * 常见特性：
     * - IORING_FEAT_SINGLE_MMAP: 只需要一次mmap
     * - IORING_FEAT_NODROP: 保证CQE不会丢失
     * - IORING_FEAT_SUBMIT_STABLE: 提交后缓冲区可以立即重用
     * - IORING_FEAT_RW_CUR_POS: 支持使用当前文件位置
     */
    struct io_uring_params params;
    std::memset(&params, 0, sizeof(params));

    // 创建临时ring获取特性
    struct io_uring temp_ring;
    if (io_uring_queue_init_params(1, &temp_ring, &params) == 0) {
        uint32_t features = params.features;
        io_uring_queue_exit(&temp_ring);
        return features;
    }

    return 0;
}

// ============================================================================
// EventLoop 类实现
// ============================================================================

EventLoop::EventLoop(IoUring& ring)
    : ring_(ring)
    , running_(false)
{
}

EventLoop::~EventLoop() {
    stop();
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

void EventLoop::start() {
    if (running_.exchange(true)) {
        // 已经在运行
        return;
    }

    /**
     * 在新线程中启动事件循环
     *
     * 使用单独的线程运行事件循环有以下好处：
     * 1. 不阻塞主线程
     * 2. IO完成事件可以被及时处理
     * 3. 支持真正的异步编程模型
     */
    loop_thread_ = std::thread(&EventLoop::run, this);
}

void EventLoop::stop() {
    running_.store(false);

    /**
     * 唤醒等待中的事件循环
     *
     * 当事件循环在io_uring_wait_cqe中阻塞时，
     * 我们需要一种方式来唤醒它。
     *
     * 方案1（当前采用）：提交一个NOP操作来唤醒
     * 方案2：使用eventfd进行通知
     * 方案3：使用带超时的等待
     */
    if (ring_.is_valid()) {
        IoContext ctx;
        ctx.op_type = IoOpType::NOP;
        try {
            ring_.submit_request(ctx);
        } catch (...) {
            // 忽略错误，可能ring已经被关闭
        }
    }
}

void EventLoop::join() {
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

void EventLoop::post(std::function<void()> func) {
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        task_queue_.push(std::move(func));
    }

    // 唤醒事件循环（如果它在等待）
    // 这里使用NOP操作来唤醒
    if (ring_.is_valid()) {
        IoContext ctx;
        ctx.op_type = IoOpType::NOP;
        try {
            ring_.submit_request(ctx);
        } catch (...) {
            // 忽略错误
        }
    }
}

void EventLoop::run() {
    /**
     * 事件循环主函数
     *
     * 这个循环会持续运行，直到running_变为false
     *
     * 循环中的操作：
     * 1. 处理任务队列中的任务
     * 2. 等待并处理IO完成事件
     * 3. 检查是否应该停止
     */
    while (running_.load()) {
        // 处理任务队列
        process_tasks();

        try {
            /**
             * 等待IO完成事件
             *
             * 使用带超时的等待，这样即使没有IO事件，
             * 也能定期检查running_标志和任务队列
             */
            ring_.wait_and_process(1, 100);  // 100ms超时
        } catch (const std::exception& e) {
            std::cerr << "[EventLoop] 处理完成事件时发生异常: " << e.what() << std::endl;
        }
    }

    // 处理剩余的完成事件
    try {
        ring_.process_completions(0);
    } catch (...) {
        // 忽略清理时的错误
    }

    // 处理剩余的任务
    process_tasks();
}

void EventLoop::process_tasks() {
    /**
     * 处理任务队列
     *
     * 从队列中取出任务并执行
     * 使用交换技术减少锁的持有时间
     */
    std::queue<std::function<void()>> tasks;

    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        tasks.swap(task_queue_);
    }

    while (!tasks.empty()) {
        auto& task = tasks.front();
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "[EventLoop] 任务执行异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[EventLoop] 任务执行发生未知异常" << std::endl;
        }
        tasks.pop();
    }
}

} // namespace asyncio
