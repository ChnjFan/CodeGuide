# 内存池

内存池提前向操作系统申请一块连续的大内存，解决频繁向操作系统申请内存带来的系统调用和开销。

[TOC]

## 为什么需要内存池

传统内存分配，如`new/delete`、`malloc/free` 存在的问题：

- **性能开销大**：每次调用`malloc`/`new`都需要向操作系统申请内存，操作系统需要遍历空闲内存块、处理内存碎片、更新内存管理数据结构，频繁的小内存分配会带来大量的系统调用和开销。
- **内存碎片**：频繁分配和释放不同大小的内存块，会导致内存空间中出现大量无法被利用的小空闲块（外部碎片），降低内存利用率。
- **线程安全开销**：标准库的内存分配函数（如`malloc`）是线程安全的，内部会加锁，在高并发场景下，锁竞争会严重影响性能。

内存池的核心解决思路是：

1. 提前向操作系统申请一块连续的大内存，之后内存分配都在这块大内存中进行。
2. 内存耗尽时再按需扩展。
3. 释放内存先归还给内存池，而非直接还给操作系统。

## 核心实现原理

生产级的 C++ 内存池管理系统，融合了**多层级内存块管理**、**智能碎片整理**、**线程安全机制**和**高性能对象池**，旨在提供一个**高效、可靠、易用**的通用内存管理解决方案。

### 内存对齐算法

内存对齐是系统的基础，通过位运算的高效计算实现向上对齐。

```cpp
/**
 * @brief 计算内存对齐所需的填充字节数
 * @param size 当前大小
 * @param alignment 对齐字节数（通常为2的幂）
 * @return 对齐后的大小
 */
inline constexpr size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}
```

算法原理通过向上取整，将对齐字节数的低位清 0 得到对齐后的值：

1. `alignment-1` 生成低位全 1 的掩码，例如 16 对齐时为 0x0F（`0000 1111`）。
2. `~(alignment-1)` 取反得到高位全 1 低位全 0 的掩码，例如 0xFFFFFFF0。
3. `size + alignment - 1` 先加上对齐基数减 1，确保能向上取整。
4. `& ~(alignment-1)` 通过与操作将低位清 0，得到对齐后的值。

`constexpr` 表示函数可以在编译期计算，如果入参是常量，编译器直接计算出结果，没有运行时的开销。

### 环形链表与块管理

内存块采用双向链表管理多个 chunk（内存片段），每个 chunk 前面都有一个元数据头。

```cpp
/**
 * @brief 内存块元数据
 * 存储在每个内存块的头部，用于跟踪块的状态
 */
struct MemoryBlockHeader {
    uint32_t magic;           // 魔数字0xDEADBEEF，用于验证块的有效性
    uint32_t block_size;      // 块的大小
    uint8_t is_free;          // 是否空闲（1=空闲，0=被占用）
    uint32_t alignment_padding; // 对齐填充大小，用于统计内存碎片
    MemoryBlockHeader* next;   // 指向下一个块
    MemoryBlockHeader* prev;   // 指向上一个块
};
```

双向链表的存储方式可以在 O(n) 时间内遍历 Chunk，并在释放时与相邻空闲块合并，降低碎片率。

#### 内存块管理核心类

`MemoryBlock` 类是内存块管理核心，通过管理一个大的预分配内存区域，利用链表维护内部的所有 Chunk。

```cpp
/**
 * @brief 内存池中的内存块类
 * 管理预分配的内存块，支持分割和合并
 */
class MemoryBlock {
public:
    static constexpr uint32_t MAGIC_NUMBER = 0xDEADBEEF;	// 魔术字，Chunk验证
    static constexpr size_t MIN_BLOCK_SIZE = 64;  // 最小块大小，防止过度碎片化
    static constexpr size_t ALIGNMENT = 16;        // 默认对齐字节数（适应SSE/AVX）
	//成员函数
private:
    void* raw_memory_;           // 原始内存指针
    size_t total_size_;          // 总内存大小
    std::atomic<size_t> used_size_;  // 已使用内存大小（线程安全）
    std::atomic<size_t> cached_max_free_size_;  // 缓存的最大连续空闲大小
    MemoryBlockHeader* first_block_; // 首个块的指针
    mutable std::mutex block_mutex_;     // 保护块结构的互斥锁（mutable允许在const函数中使用）
};
```

#### 初始化内存块

内存块创建时按字节类型分配一大块连续内存，并初始化第一个 Chunk。

```cpp
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
```

创建第一个 Chunk 后将缓存的，缓存空闲大小需要减去 Chunk 头大小，此时的内存布局如下：

```bash
+---------------------------------------------------------------------+
| MemoryBlockHeader (32B)  |        Free Space (size - 32B)           |
+---------------------------------------------------------------------+
  ^
  |
first_block_
```

#### 内存分配

块内存分配函数 `MemoryBlock::allocate` 是内存池的核心，实现高效内存分配。

```cpp
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
```

分配内存从 Chunk 链表中使用首次适配算法查找到第一个足够大的空闲 Chunk。

如果 Chunk 块太大，先将块进行拆分，确保拆分后的块至少有 64 字节空间可用，避免产生无用的微小碎片。

`aligned_size + sizeof(MemoryBlockHeader) + MIN_BLOCK_SIZE` 剩余空间大小比最小块大小还大时进行块拆分。

更新缓存内存统计和最大空闲块大小后返回数据指针。

#### 查找可用块

查找可用块时使用**首次适配算法（First Fit）**，需要分配 n 字节内存时，系统从链表头开始遍历找到第一个大小大于 n 的空闲 Chunk。

```cpp
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
```

算法时间复杂度为 O(n)，实际应用中空闲块通常靠前，遍历较少：

- 缓存友好：从头遍历有利于 CPU 缓存。
- 简单高效：不需要复杂的红黑树或跳表结构。
- 减少碎片：通过块拆分和合并策略控制碎片。

维护缓存最大连续空闲内存大小 `cached_max_free_size_` 避免不必要的锁获取和链表遍历。

为什么不用**最佳适配（Best Fit）**：

- 最佳适配每次都需要遍历整个链表找到合适的块，每次时间复杂度都是 O(n)。
- 通过块拆分策略，内存浪费可以控制在可接受范围内。

#### 块拆分

当找到的空闲块大小远大于需求时，将其拆分为两个块。

```cpp
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
```

将分配的块按照所需大小分割为两部分：

```bash
拆分前：
+--------+------------------+
| Header |  Free (1024B)    |
+--------+------------------+
拆分后（分配256B）：
+--------+--------+--------+---------+
| Header | Used   | Header | Free    |
|        | (256B) |        | (768B-H)|
+--------+--------+--------+---------+
```

#### 块合并



### 多层级内存块管理

采用分层设计将内存池分为三个不同级别的层次：

- 小块池（small_blocks_）：默认256KB，用于频繁的小对象分配（1B-256KB）。
- 中块池（medium_blocks_）：默认1MB，用于中等大小的对象分配（256KB-1MB）。
- 大块池（large_blocks_）：默认4MB，用于大对象分配（1MB-4MB）。

分层架构的核心优势在于**按需分配**和**减少浪费**。当应用程序请求某个大小的内存时，系统会根据请求大小自动选择最合适的内存块进行分配，避免了在过大的块中分配小对象造成的空间浪费，也避免了在过小的块中无法满足大对象需求的问题。

```cpp
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
```

使用降级策略选择内存块，优先匹配小块池，匹配失败则降级到更大的池中。

利用 `std::atomic<size_t> cached_max_free_size_;` 保存的空闲块大小进行无锁快速检查，避免在明显不符合条件的块浪费时间。

