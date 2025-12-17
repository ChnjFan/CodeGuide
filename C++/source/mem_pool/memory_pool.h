#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <iostream>

// ============================================================================
// 内存对齐工具
// ============================================================================

/**
 * @brief 计算内存对齐所需的填充字节数
 * @param size 当前大小
 * @param alignment 对齐字节数（通常为2的幂）
 * @return 对齐后的大小
 */
inline constexpr size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// ============================================================================
// 内存块管理（Block Management）
// ============================================================================

/**
 * @brief 内存块元数据
 * 存储在每个内存块的头部，用于跟踪块的状态
 */
struct MemoryBlockHeader {
    uint32_t magic;           // 魔数，用于验证块的有效性
    uint32_t block_size;      // 块的大小
    uint8_t is_free;          // 是否空闲（1=空闲，0=被占用）
    uint32_t alignment_padding; // 对齐填充大小
    MemoryBlockHeader* next;   // 指向下一个块
    MemoryBlockHeader* prev;   // 指向上一个块
};

/**
 * @brief 内存池中的内存块类
 * 管理预分配的内存块，支持分割和合并
 */
class MemoryBlock {
public:
    static constexpr uint32_t MAGIC_NUMBER = 0xDEADBEEF;
    static constexpr size_t MIN_BLOCK_SIZE = 64;  // 最小块大小
    static constexpr size_t ALIGNMENT = 16;        // 默认对齐字节数（适应SSE/AVX）

    /**
     * @brief 构造函数
     * @param size 内存块大小
     */
    explicit MemoryBlock(size_t size);

    /**
     * @brief 析构函数
     */
    ~MemoryBlock();

    /**
     * @brief 分配内存
     * @param size 申请的大小
     * @return 指向分配内存的指针，失败返回nullptr
     */
    void* allocate(size_t size);

    /**
     * @brief 释放内存
     * @param ptr 待释放的指针
     * @return 释放是否成功
     */
    bool deallocate(void* ptr);

    /**
     * @brief 获取块中的空闲内存大小
     * @return 可用的连续空闲空间大小
     */
    size_t get_free_space() const;

    /**
     * @brief 获取块的总大小
     */
    size_t get_total_size() const { return total_size_; }

    /**
     * @brief 获取已使用的内存大小
     */
    size_t get_used_size() const { return used_size_; }

    /**
     * @brief 获取原始内存指针
     * 用于判断一个指针是否属于该内存块
     */
    void* get_raw_memory() const { return raw_memory_; }

    /**
     * @brief 获取内存块的使用率（百分比）
     */
    double get_usage_ratio() const {
        return total_size_ > 0 ? (double)used_size_ / total_size_ * 100.0 : 0.0;
    }

    /**
     * @brief 碎片整理（compact）
     * 合并相邻的空闲块以减少碎片
     */
    void compact();

    /**
     * @brief 打印块的统计信息
     */
    void print_stats() const;

    /**
     * @brief 获取最大连续空闲块的大小
     * 这与 get_free_space() 不同，它返回最大的单个连续空闲块，
     * 而不是所有空闲块的总和。用于判断是否能进行特定大小的分配。
     * @return 最大连续空闲块的大小（字节）
     */
    size_t get_max_free_block_size() const;

    /**
     * @brief 获取缓存的最大空闲块大小（无锁，用于快速检查）
     * @return 缓存的最大空闲块大小
     */
    size_t get_cached_max_free_size() const { return cached_max_free_size_.load(); }

    /**
     * @brief 检查指针是否属于此内存块
     * @param ptr 待检查的指针
     * @return 如果指针在此块的范围内返回true
     */
    bool contains(void* ptr) const {
        return ptr >= raw_memory_ &&
               ptr < reinterpret_cast<char*>(raw_memory_) + total_size_;
    }

    /**
     * @brief 计算块内部的碎片率
     * @return 碎片率（0-100）
     */
    size_t get_internal_fragmentation() const;

private:
    /**
     * @brief 查找足够大小的空闲块
     * @param size 所需大小
     * @return 指向内存块头的指针
     */
    MemoryBlockHeader* find_free_block(size_t size);

    /**
     * @brief 拆分一个大块为两个较小的块
     * @param header 要拆分的块
     * @param needed_size 所需的块大小
     */
    void split_block(MemoryBlockHeader* header, size_t needed_size);

    /**
     * @brief 尝试合并相邻的空闲块
     * @param header 起始块
     */
    void merge_free_blocks(MemoryBlockHeader* header);

    /**
     * @brief 获取空闲空间（内部版本，不加锁）
     * 调用前必须已持有 block_mutex_
     */
    size_t get_free_space_unlocked() const;

    /**
     * @brief 更新缓存的最大空闲块大小
     * 调用前必须已持有 block_mutex_
     */
    void update_cached_max_free_size();

    void* raw_memory_;           // 原始内存指针
    size_t total_size_;          // 总内存大小
    std::atomic<size_t> used_size_;  // 已使用内存大小（线程安全）
    std::atomic<size_t> cached_max_free_size_;  // 缓存的最大空闲块大小
    MemoryBlockHeader* first_block_; // 首个块的指针
    mutable std::mutex block_mutex_;     // 保护块结构的互斥锁（mutable允许在const函数中使用）
};

// ============================================================================
// 对象池管理（Object Pool Management）
// ============================================================================

/**
 * @brief 通用对象池类
 * 支持预创建固定数量的对象，提供快速的获取和归还机制
 */
template<typename T>
class ObjectPool {
public:
    /**
     * @brief 构造函数
     * @param initial_capacity 初始对象数量
     * @param max_capacity 最大对象数量
     */
    ObjectPool(size_t initial_capacity = 100, size_t max_capacity = 1000)
        : initial_capacity_(initial_capacity),
          max_capacity_(max_capacity),
          current_size_(0),
          peak_used_(0) {

        // 预创建初始对象
        for (size_t i = 0; i < initial_capacity_; ++i) {
            free_objects_.push(new T());
        }
        current_size_ = initial_capacity_;
    }

    /**
     * @brief 析构函数
     */
    ~ObjectPool() {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        while (!free_objects_.empty()) {
            T* obj = free_objects_.front();
            free_objects_.pop();
            delete obj;
        }

        // 删除已使用的对象
        for (auto obj : used_objects_) {
            delete obj;
        }
        used_objects_.clear();
    }

    /**
     * @brief 获取一个对象
     * @return 指向对象的指针
     */
    T* acquire() {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        T* obj = nullptr;

        if (!free_objects_.empty()) {
            obj = free_objects_.front();
            free_objects_.pop();
        } else if (current_size_ < max_capacity_) {
            // 动态扩展
            obj = new T();
            current_size_++;
        } else {
            // 池已满，返回nullptr
            return nullptr;
        }

        used_objects_.insert(obj);

        // 更新峰值使用数
        if (used_objects_.size() > peak_used_) {
            peak_used_ = used_objects_.size();
        }

        return obj;
    }

    /**
     * @brief 归还一个对象
     * @param obj 待归还的对象指针
     * @return 是否成功归还
     */
    bool release(T* obj) {
        if (!obj) return false;

        std::lock_guard<std::mutex> lock(pool_mutex_);

        // 从使用集合中移除（O(1) 平均复杂度）
        auto it = used_objects_.find(obj);
        if (it != used_objects_.end()) {
            used_objects_.erase(it);
            free_objects_.push(obj);
            return true;
        }

        return false;
    }

    /**
     * @brief 获取当前空闲对象数
     */
    size_t get_free_count() const {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        return free_objects_.size();
    }

    /**
     * @brief 获取当前使用中的对象数
     */
    size_t get_used_count() const {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        return used_objects_.size();
    }

    /**
     * @brief 获取峰值使用数
     */
    size_t get_peak_used() const {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        return peak_used_;
    }

    /**
     * @brief 获取池的总容量
     */
    size_t get_capacity() const {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        return current_size_;
    }

    /**
     * @brief 打印对象池的统计信息
     */
    void print_stats() const {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        std::cout << "ObjectPool<" << typeid(T).name() << "> Statistics:" << std::endl;
        std::cout << "  Free Objects: " << free_objects_.size() << std::endl;
        std::cout << "  Used Objects: " << used_objects_.size() << std::endl;
        std::cout << "  Peak Used: " << peak_used_ << std::endl;
        std::cout << "  Total Capacity: " << current_size_ << std::endl;
    }

private:
    std::queue<T*> free_objects_;              // 空闲对象队列
    std::unordered_set<T*> used_objects_;      // 使用中的对象集合（O(1) 查找/删除）
    size_t initial_capacity_;                  // 初始容量
    size_t max_capacity_;                      // 最大容量
    std::atomic<size_t> current_size_;         // 当前创建的对象总数
    std::atomic<size_t> peak_used_;            // 峰值使用数
    mutable std::mutex pool_mutex_;            // 保护池结构的互斥锁
};

// ============================================================================
// 多层级内存池管理器（Tiered Memory Pool Manager）
// ============================================================================

/**
 * @brief 内存池配置结构
 */
struct MemoryPoolConfig {
    size_t small_block_size;    // 小块大小（字节）
    size_t medium_block_size;   // 中块大小（字节）
    size_t large_block_size;    // 大块大小（字节）
    size_t block_count;         // 每种大小的块数量

    MemoryPoolConfig(
        size_t small = 256 * 1024,      // 256KB
        size_t medium = 1024 * 1024,    // 1MB
        size_t large = 4 * 1024 * 1024, // 4MB
        size_t count = 10
    ) : small_block_size(small),
        medium_block_size(medium),
        large_block_size(large),
        block_count(count) {}
};

/**
 * @brief 统计信息结构
 */
struct PoolStatistics {
    size_t total_allocated;   // 总分配内存
    size_t total_used;        // 总已用内存
    size_t fragmentation_ratio; // 碎片率
    size_t block_count;       // 块总数
    double avg_utilization;   // 平均利用率
};

/**
 * @brief 主内存池管理器
 * 管理多个不同大小的内存块，使用分层策略优化分配
 */
class MemoryPoolManager {
public:
    /**
     * @brief 构造函数
     * @param config 内存池配置
     */
    explicit MemoryPoolManager(const MemoryPoolConfig& config = MemoryPoolConfig());

    /**
     * @brief 析构函数
     */
    ~MemoryPoolManager();

    /**
     * @brief 分配内存
     * @param size 申请的大小
     * @return 指向分配内存的指针
     */
    void* allocate(size_t size);

    /**
     * @brief 释放内存
     * @param ptr 待释放的指针
     * @return 是否释放成功
     */
    bool deallocate(void* ptr);

    /**
     * @brief 获取当前内存统计信息
     */
    PoolStatistics get_statistics() const;

    /**
     * @brief 打印所有内存块的统计信息
     */
    void print_all_stats() const;

    /**
     * @brief 对所有块进行碎片整理
     */
    void compact_all();

    /**
     * @brief 获取分配的总内存大小
     */
    size_t get_total_allocated() const {
        std::lock_guard<std::mutex> lock(manager_mutex_);
        return total_allocated_;
    }

    /**
     * @brief 获取使用的总内存大小
     */
    size_t get_total_used() const {
        std::lock_guard<std::mutex> lock(manager_mutex_);
        size_t total = 0;
        for (const auto& block : small_blocks_) {
            total += block->get_used_size();
        }
        for (const auto& block : medium_blocks_) {
            total += block->get_used_size();
        }
        for (const auto& block : large_blocks_) {
            total += block->get_used_size();
        }
        return total;
    }

    /**
     * @brief 重置所有统计信息
     */
    void reset_statistics();

private:
    /**
     * @brief 选择合适的块来分配内存
     * @param size 所需大小
     * @return 指向合适块的指针，失败返回nullptr
     */
    MemoryBlock* select_block_for_allocation(size_t size);

    /**
     * @brief 根据指针查找对应的块
     * @param ptr 内存指针
     * @return 指向块的指针
     */
    MemoryBlock* find_block_for_pointer(void* ptr);

    // 不同大小的内存块管理
    std::vector<std::unique_ptr<MemoryBlock>> small_blocks_;   // 小块池
    std::vector<std::unique_ptr<MemoryBlock>> medium_blocks_;  // 中块池
    std::vector<std::unique_ptr<MemoryBlock>> large_blocks_;   // 大块池

    // 配置和统计
    MemoryPoolConfig config_;
    size_t total_allocated_;        // 总分配的内存
    std::atomic<size_t> allocation_count_;  // 分配计数
    std::atomic<size_t> deallocation_count_; // 释放计数

    // 线程安全
    mutable std::mutex manager_mutex_; // 保护管理器结构
};

#endif // MEMORY_POOL_H
