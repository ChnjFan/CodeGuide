#include "memory_pool.h"
#include <cstring>
#include <iomanip>

// ============================================================================
// MemoryBlock 实现
// ============================================================================

MemoryBlock::MemoryBlock(size_t size)
    : total_size_(size), used_size_(0), cached_max_free_size_(0), first_block_(nullptr) {

    // 分配原始内存
    raw_memory_ = new char[size];

    if (!raw_memory_) {
        throw std::bad_alloc();
    }

    // 初始化第一个块头
    MemoryBlockHeader* header = reinterpret_cast<MemoryBlockHeader*>(raw_memory_);
    header->magic = MAGIC_NUMBER;
    header->block_size = size - sizeof(MemoryBlockHeader);
    header->is_free = 1;  // 初始为空闲
    header->alignment_padding = 0;
    header->next = nullptr;
    header->prev = nullptr;

    first_block_ = header;

    // 初始化缓存的最大空闲块大小
    cached_max_free_size_ = header->block_size;
}

MemoryBlock::~MemoryBlock() {
    if (raw_memory_) {
        delete[] reinterpret_cast<char*>(raw_memory_);
        raw_memory_ = nullptr;
    }
}

void* MemoryBlock::allocate(size_t size) {
    if (size == 0) return nullptr;

    std::lock_guard<std::mutex> lock(block_mutex_);

    // 对齐申请的大小
    size_t aligned_size = align_up(size, ALIGNMENT);

    // 快速检查：如果缓存的最大空闲块不够大，直接返回
    if (cached_max_free_size_.load() < aligned_size) {
        return nullptr;
    }

    // 查找足够大的空闲块
    MemoryBlockHeader* block = find_free_block(aligned_size);
    if (!block) {
        return nullptr;
    }

    // 如果块太大，拆分它
    if (block->block_size > aligned_size + sizeof(MemoryBlockHeader) + MIN_BLOCK_SIZE) {
        split_block(block, aligned_size);
    }

    // 标记块为已使用
    block->is_free = 0;
    size_t actual_size = align_up(size, ALIGNMENT);
    block->alignment_padding = actual_size - size;

    // 更新已使用内存统计
    used_size_ += block->block_size;

    // 更新缓存的最大空闲块大小
    update_cached_max_free_size();

    // 返回数据指针（跳过块头）
    return reinterpret_cast<void*>(reinterpret_cast<char*>(block) + sizeof(MemoryBlockHeader));
}

bool MemoryBlock::deallocate(void* ptr) {
    if (!ptr) return false;

    std::lock_guard<std::mutex> lock(block_mutex_);

    // 找到块头
    MemoryBlockHeader* header = reinterpret_cast<MemoryBlockHeader*>(
        reinterpret_cast<char*>(ptr) - sizeof(MemoryBlockHeader)
    );

    // 验证魔数
    if (header->magic != MAGIC_NUMBER) {
        // 不打印错误，可能是指针不属于此块
        return false;
    }

    if (header->is_free) {
        std::cerr << "[WARNING] 尝试释放已经释放的内存块" << std::endl;
        return false;
    }

    // 标记为空闲
    header->is_free = 1;
    used_size_ -= header->block_size;

    // 尝试合并相邻的空闲块
    merge_free_blocks(header);

    // 更新缓存的最大空闲块大小
    update_cached_max_free_size();

    return true;
}

size_t MemoryBlock::get_free_space() const {
    std::lock_guard<std::mutex> lock(block_mutex_);
    return get_free_space_unlocked();
}

size_t MemoryBlock::get_free_space_unlocked() const {
    // 内部版本，调用前必须已持有 block_mutex_
    size_t free_space = 0;
    MemoryBlockHeader* current = first_block_;

    while (current) {
        if (current->is_free) {
            free_space += current->block_size;
        }
        current = current->next;
    }

    return free_space;
}

MemoryBlockHeader* MemoryBlock::find_free_block(size_t size) {
    // 首次适配算法（First Fit）：找到第一个足够大的空闲块
    MemoryBlockHeader* current = first_block_;

    while (current) {
        if (current->is_free && current->block_size >= size) {
            return current;
        }
        current = current->next;
    }

    return nullptr;
}

void MemoryBlock::split_block(MemoryBlockHeader* header, size_t needed_size) {
    // 计算新块的位置
    char* new_block_addr = reinterpret_cast<char*>(header) + sizeof(MemoryBlockHeader) + needed_size;
    MemoryBlockHeader* new_header = reinterpret_cast<MemoryBlockHeader*>(new_block_addr);

    // 初始化新块头
    new_header->magic = MAGIC_NUMBER;
    new_header->block_size = header->block_size - needed_size - sizeof(MemoryBlockHeader);
    new_header->is_free = 1;
    new_header->alignment_padding = 0;
    new_header->next = header->next;
    new_header->prev = header;

    // 更新原块
    header->block_size = needed_size;
    header->next = new_header;

    // 更新后继块的前驱指针
    if (new_header->next) {
        new_header->next->prev = new_header;
    }
}

void MemoryBlock::merge_free_blocks(MemoryBlockHeader* header) {
    // 尝试与下一个块合并
    if (header->next && header->next->is_free) {
        header->block_size += sizeof(MemoryBlockHeader) + header->next->block_size;
        header->next = header->next->next;
        if (header->next) {
            header->next->prev = header;
        }
    }

    // 尝试与前一个块合并
    if (header->prev && header->prev->is_free) {
        header->prev->block_size += sizeof(MemoryBlockHeader) + header->block_size;
        header->prev->next = header->next;
        if (header->next) {
            header->next->prev = header->prev;
        }
    }
}

void MemoryBlock::compact() {
    std::lock_guard<std::mutex> lock(block_mutex_);

    // 遍历所有块，合并相邻的空闲块
    MemoryBlockHeader* current = first_block_;

    while (current && current->next) {
        if (current->is_free && current->next->is_free) {
            // 合并当前块和下一个块
            current->block_size += sizeof(MemoryBlockHeader) + current->next->block_size;
            current->next = current->next->next;
            if (current->next) {
                current->next->prev = current;
            }
            // 不移动指针，继续检查合并后的块
        } else {
            current = current->next;
        }
    }

    // 更新缓存的最大空闲块大小
    update_cached_max_free_size();
}

void MemoryBlock::print_stats() const {
    std::lock_guard<std::mutex> lock(block_mutex_);

    // 使用不加锁版本避免死锁
    size_t free_space = get_free_space_unlocked();

    std::cout << std::endl << "========== MemoryBlock Statistics ==========" << std::endl;
    std::cout << "Total Size: " << total_size_ << " bytes ("
              << (double)total_size_ / (1024 * 1024) << " MB)" << std::endl;
    std::cout << "Used Size:  " << used_size_ << " bytes ("
              << (double)used_size_ / (1024 * 1024) << " MB)" << std::endl;
    std::cout << "Free Size:  " << free_space << " bytes ("
              << (double)free_space / (1024 * 1024) << " MB)" << std::endl;
    std::cout << "Usage Ratio: " << std::fixed << std::setprecision(2)
              << (total_size_ > 0 ? (double)used_size_ / total_size_ * 100.0 : 0.0) << "%" << std::endl;

    // 打印块的详细信息
    size_t block_count = 0;
    size_t free_block_count = 0;
    MemoryBlockHeader* current = first_block_;

    while (current) {
        block_count++;
        if (current->is_free) {
            free_block_count++;
        }
        current = current->next;
    }

    std::cout << "Block Count: " << block_count << " (Free: " << free_block_count << ")" << std::endl;
    std::cout << "==========================================" << std::endl;
}

size_t MemoryBlock::get_max_free_block_size() const {
    std::lock_guard<std::mutex> lock(block_mutex_);

    size_t max_size = 0;
    MemoryBlockHeader* current = first_block_;

    while (current) {
        if (current->is_free && current->block_size > max_size) {
            max_size = current->block_size;
        }
        current = current->next;
    }

    return max_size;
}

void MemoryBlock::update_cached_max_free_size() {
    // 注意：调用此函数前必须已持有 block_mutex_
    size_t max_size = 0;
    MemoryBlockHeader* current = first_block_;

    while (current) {
        if (current->is_free && current->block_size > max_size) {
            max_size = current->block_size;
        }
        current = current->next;
    }

    cached_max_free_size_ = max_size;
}

size_t MemoryBlock::get_internal_fragmentation() const {
    std::lock_guard<std::mutex> lock(block_mutex_);

    size_t total_free = 0;
    size_t max_free = 0;
    size_t free_block_count = 0;
    MemoryBlockHeader* current = first_block_;

    while (current) {
        if (current->is_free) {
            total_free += current->block_size;
            free_block_count++;
            if (current->block_size > max_free) {
                max_free = current->block_size;
            }
        }
        current = current->next;
    }

    // 碎片率 = (空闲块数量 - 1) / 空闲块数量 * (1 - max_free / total_free) * 100
    // 或简化为：如果只有一个空闲块，碎片率为0
    // 否则碎片率 = (total_free - max_free) / total_free * 100
    if (free_block_count <= 1 || total_free == 0) {
        return 0;  // 没有碎片或只有一个连续空闲块
    }

    return (total_free - max_free) * 100 / total_free;
}

// ============================================================================
// MemoryPoolManager 实现
// ============================================================================

MemoryPoolManager::MemoryPoolManager(const MemoryPoolConfig& config)
    : config_(config), total_allocated_(0), allocation_count_(0), deallocation_count_(0) {

    // 初始化小块池
    for (size_t i = 0; i < config_.block_count; ++i) {
        small_blocks_.push_back(std::make_unique<MemoryBlock>(config_.small_block_size));
        total_allocated_ += config_.small_block_size;
    }

    // 初始化中块池
    for (size_t i = 0; i < config_.block_count; ++i) {
        medium_blocks_.push_back(std::make_unique<MemoryBlock>(config_.medium_block_size));
        total_allocated_ += config_.medium_block_size;
    }

    // 初始化大块池
    for (size_t i = 0; i < config_.block_count; ++i) {
        large_blocks_.push_back(std::make_unique<MemoryBlock>(config_.large_block_size));
        total_allocated_ += config_.large_block_size;
    }

    std::cout << "[INFO] MemoryPoolManager initialized with " << config_.block_count
              << " blocks per size category" << std::endl;
    std::cout << "[INFO] Total memory allocated: " << (double)total_allocated_ / (1024 * 1024)
              << " MB" << std::endl;
}

MemoryPoolManager::~MemoryPoolManager() {
    std::lock_guard<std::mutex> lock(manager_mutex_);

    small_blocks_.clear();
    medium_blocks_.clear();
    large_blocks_.clear();

    std::cout << "[INFO] MemoryPoolManager destroyed" << std::endl;
}

MemoryBlock* MemoryPoolManager::select_block_for_allocation(size_t size) {
    // 优化：使用缓存的最大空闲块大小进行快速筛选
    // 避免每次都遍历块内所有chunk

    // 计算需要的总大小（包括对齐）
    size_t aligned_size = align_up(size, MemoryBlock::ALIGNMENT);

    // 尝试顺序：小块 -> 中块 -> 大块
    std::vector<std::vector<std::unique_ptr<MemoryBlock>>*> pools_to_try;

    // 根据申请大小确定起始池
    if (size <= config_.small_block_size - sizeof(MemoryBlockHeader) - MemoryBlock::MIN_BLOCK_SIZE) {
        pools_to_try.push_back(&small_blocks_);
        pools_to_try.push_back(&medium_blocks_);
        pools_to_try.push_back(&large_blocks_);
    } else if (size <= config_.medium_block_size - sizeof(MemoryBlockHeader) - MemoryBlock::MIN_BLOCK_SIZE) {
        pools_to_try.push_back(&medium_blocks_);
        pools_to_try.push_back(&large_blocks_);
    } else if (size <= config_.large_block_size - sizeof(MemoryBlockHeader) - MemoryBlock::MIN_BLOCK_SIZE) {
        pools_to_try.push_back(&large_blocks_);
    } else {
        std::cerr << "[ERROR] 申请大小 " << size << " 超过最大块大小 "
                  << config_.large_block_size << std::endl;
        return nullptr;
    }

    // 在各个池中依次尝试查找（使用缓存值快速筛选）
    for (auto target_pool : pools_to_try) {
        for (auto& block : *target_pool) {
            // 使用缓存的最大空闲块大小进行快速检查（无锁）
            if (block->get_cached_max_free_size() >= aligned_size) {
                return block.get();
            }
        }
    }

    return nullptr;
}

MemoryBlock* MemoryPoolManager::find_block_for_pointer(void* ptr) {
    // 优化：使用 contains() 方法进行快速地址范围检查
    for (auto& block : small_blocks_) {
        if (block->contains(ptr)) {
            return block.get();
        }
    }

    for (auto& block : medium_blocks_) {
        if (block->contains(ptr)) {
            return block.get();
        }
    }

    for (auto& block : large_blocks_) {
        if (block->contains(ptr)) {
            return block.get();
        }
    }

    return nullptr;
}

void* MemoryPoolManager::allocate(size_t size) {
    if (size == 0) return nullptr;

    std::lock_guard<std::mutex> lock(manager_mutex_);

    MemoryBlock* target_block = select_block_for_allocation(size);

    if (!target_block) {
        std::cerr << "[ERROR] 无法为大小为 " << size << " 的内存分配寻找合适的块" << std::endl;
        return nullptr;
    }

    void* ptr = target_block->allocate(size);

    if (ptr) {
        allocation_count_++;
    }

    return ptr;
}

bool MemoryPoolManager::deallocate(void* ptr) {
    if (!ptr) return false;

    std::lock_guard<std::mutex> lock(manager_mutex_);

    // 优化：先精确定位指针所属的块，避免在所有块中尝试
    MemoryBlock* target_block = find_block_for_pointer(ptr);

    if (target_block) {
        if (target_block->deallocate(ptr)) {
            deallocation_count_++;
            return true;
        }
    }

    std::cerr << "[WARNING] 无法找到待释放的指针: " << ptr << std::endl;
    return false;
}

PoolStatistics MemoryPoolManager::get_statistics() const {
    std::lock_guard<std::mutex> lock(manager_mutex_);

    PoolStatistics stats;
    stats.total_allocated = total_allocated_;

    // Calculate total_used without recursively locking
    size_t total_used = 0;
    for (const auto& block : small_blocks_) {
        total_used += block->get_used_size();
    }
    for (const auto& block : medium_blocks_) {
        total_used += block->get_used_size();
    }
    for (const auto& block : large_blocks_) {
        total_used += block->get_used_size();
    }
    stats.total_used = total_used;

    stats.block_count = small_blocks_.size() + medium_blocks_.size() + large_blocks_.size();

    // 计算平均利用率
    if (stats.total_allocated > 0) {
        stats.avg_utilization = (double)stats.total_used / stats.total_allocated * 100.0;
    } else {
        stats.avg_utilization = 0.0;
    }

    // 只计算已使用块的碎片率，空块不算碎片
    size_t total_fragmentation = 0;
    size_t blocks_with_usage = 0;

    auto calc_block_frag = [&](const std::vector<std::unique_ptr<MemoryBlock>>& blocks) {
        for (const auto& block : blocks) {
            if (block->get_used_size() > 0) {
                // 只有使用中的块才计算碎片率
                total_fragmentation += block->get_internal_fragmentation();
                blocks_with_usage++;
            }
        }
    };

    calc_block_frag(small_blocks_);
    calc_block_frag(medium_blocks_);
    calc_block_frag(large_blocks_);

    if (blocks_with_usage > 0) {
        stats.fragmentation_ratio = total_fragmentation / blocks_with_usage;
    } else {
        stats.fragmentation_ratio = 0;  // 没有使用任何块，碎片率为0
    }

    return stats;
}

void MemoryPoolManager::print_all_stats() const {
    std::lock_guard<std::mutex> lock(manager_mutex_);

    std::cout << std::endl << "========== MemoryPoolManager Statistics ==========" << std::endl;
    std::cout << "Total Allocated: " << (double)total_allocated_ / (1024 * 1024) << " MB" << std::endl;


    size_t total_used = 0;
    for (const auto& block : small_blocks_) {
        total_used += block->get_used_size();
    }
    for (const auto& block : medium_blocks_) {
        total_used += block->get_used_size();
    }
    for (const auto& block : large_blocks_) {
        total_used += block->get_used_size();
    }
    std::cout << "Total Used: " << (double)total_used / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Allocation Count: " << allocation_count_.load() << std::endl;
    std::cout << "Deallocation Count: " << deallocation_count_.load() << std::endl;

    size_t total_fragmentation = 0;
    size_t blocks_with_usage = 0;

    auto calc_block_frag = [&](const std::vector<std::unique_ptr<MemoryBlock>>& blocks) {
        for (const auto& block : blocks) {
            if (block->get_used_size() > 0) {
                total_fragmentation += block->get_internal_fragmentation();
                blocks_with_usage++;
            }
        }
    };

    calc_block_frag(small_blocks_);
    calc_block_frag(medium_blocks_);
    calc_block_frag(large_blocks_);

    double avg_utilization = total_allocated_ > 0 ? (double)total_used / total_allocated_ * 100.0 : 0.0;
    size_t fragmentation_ratio = blocks_with_usage > 0 ?
        total_fragmentation / blocks_with_usage : 0;

    std::cout << "Average Utilization: " << std::fixed << std::setprecision(2)
              << avg_utilization << "%" << std::endl;
    std::cout << "Fragmentation Ratio: " << fragmentation_ratio << "%" << std::endl;

    std::cout << "\n--- Small Blocks (" << config_.small_block_size / 1024 << "KB) ---" << std::endl;
    for (size_t i = 0; i < small_blocks_.size(); ++i) {
        std::cout << "Block " << i << ": " << small_blocks_[i]->get_used_size() << " / "
                  << small_blocks_[i]->get_total_size() << " bytes" << std::endl;
    }

    std::cout << "\n--- Medium Blocks (" << config_.medium_block_size / (1024 * 1024) << "MB) ---" << std::endl;
    for (size_t i = 0; i < medium_blocks_.size(); ++i) {
        std::cout << "Block " << i << ": " << (double)medium_blocks_[i]->get_used_size() / (1024 * 1024)
                  << " / " << (double)medium_blocks_[i]->get_total_size() / (1024 * 1024) << " MB" << std::endl;
    }

    std::cout << "\n--- Large Blocks (" << config_.large_block_size / (1024 * 1024) << "MB) ---" << std::endl;
    for (size_t i = 0; i < large_blocks_.size(); ++i) {
        std::cout << "Block " << i << ": " << (double)large_blocks_[i]->get_used_size() / (1024 * 1024)
                  << " / " << (double)large_blocks_[i]->get_total_size() / (1024 * 1024) << " MB" << std::endl;
    }

    std::cout << "=================================================" << std::endl;
}

void MemoryPoolManager::compact_all() {
    std::lock_guard<std::mutex> lock(manager_mutex_);

    std::cout << "[INFO] 开始对所有内存块进行碎片整理..." << std::endl;

    for (auto& block : small_blocks_) {
        block->compact();
    }

    for (auto& block : medium_blocks_) {
        block->compact();
    }

    for (auto& block : large_blocks_) {
        block->compact();
    }

    std::cout << "[INFO] 碎片整理完成" << std::endl;
}

void MemoryPoolManager::reset_statistics() {
    std::lock_guard<std::mutex> lock(manager_mutex_);

    allocation_count_ = 0;
    deallocation_count_ = 0;

    std::cout << "[INFO] 统计信息已重置" << std::endl;
}
