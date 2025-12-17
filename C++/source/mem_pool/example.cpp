#include "memory_pool.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>

// ============================================================================
// 测试用例1：基本内存分配和释放
// ============================================================================

void test_basic_allocation() {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "测试1：基本内存分配和释放" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    MemoryPoolManager pool;

    // 分配不同大小的内存
    std::cout << "\n[测试] 分配内存块..." << std::endl;
    void* ptr1 = pool.allocate(1024);        // 1KB
    void* ptr2 = pool.allocate(5 * 1024);    // 5KB
    void* ptr3 = pool.allocate(50 * 1024);   // 50KB

    if (ptr1 && ptr2 && ptr3) {
        std::cout << "[成功] 分配了3个内存块" << std::endl;

        // 向内存中写入数据
        memset(ptr1, 'A', 1024);
        memset(ptr2, 'B', 5 * 1024);
        memset(ptr3, 'C', 50 * 1024);

        std::cout << "[成功] 向内存块写入数据" << std::endl;

        // 验证数据
        if (*reinterpret_cast<char*>(ptr1) == 'A' &&
            *reinterpret_cast<char*>(ptr2) == 'B' &&
            *reinterpret_cast<char*>(ptr3) == 'C') {
            std::cout << "[成功] 数据验证通过" << std::endl;
        }

        pool.print_all_stats();

        // 释放内存
        std::cout << "\n[测试] 释放内存块..." << std::endl;
        pool.deallocate(ptr1);
        pool.deallocate(ptr2);
        pool.deallocate(ptr3);

        std::cout << "[成功] 释放了3个内存块" << std::endl;
        pool.print_all_stats();
    }
}

// ============================================================================
// 测试用例2：内存碎片和整理
// ============================================================================

void test_fragmentation_and_compaction() {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "测试2：内存碎片和整理" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    MemoryPoolManager pool(MemoryPoolConfig(512 * 1024, 2 * 1024 * 1024, 8 * 1024 * 1024, 5));

    std::cout << "\n[测试] 分配多个内存块创建碎片..." << std::endl;

    // 分配大量小块，然后释放部分，造成碎片
    std::vector<void*> ptrs;
    const int ALLOC_COUNT = 20;

    for (int i = 0; i < ALLOC_COUNT; ++i) {
        size_t size = 10 * 1024 * (i % 5 + 1);  // 10KB-50KB 交替
        void* ptr = pool.allocate(size);
        if (ptr) {
            ptrs.push_back(ptr);
        }
    }

    std::cout << "[成功] 分配了 " << ptrs.size() << " 个内存块" << std::endl;
    pool.print_all_stats();

    // 释放偶数位置的块，创建碎片
    std::cout << "\n[测试] 释放部分块创建碎片..." << std::endl;
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        pool.deallocate(ptrs[i]);
    }

    std::cout << "[成功] 释放了 " << (ptrs.size() / 2) << " 个块" << std::endl;
    pool.print_all_stats();

    // 进行碎片整理
    std::cout << "\n[测试] 执行碎片整理..." << std::endl;
    auto stats_before = pool.get_statistics();
    pool.compact_all();
    auto stats_after = pool.get_statistics();

    std::cout << "[成功] 碎片整理完成" << std::endl;
    std::cout << "整理前碎片率: " << stats_before.fragmentation_ratio << "%" << std::endl;
    std::cout << "整理后碎片率: " << stats_after.fragmentation_ratio << "%" << std::endl;

    // 清理
    for (size_t i = 1; i < ptrs.size(); i += 2) {
        pool.deallocate(ptrs[i]);
    }
}

// ============================================================================
// 测试用例3：对象池
// ============================================================================

struct TestObject {
    int id;
    double value;
    char buffer[256];

    TestObject() : id(-1), value(0.0) {
        memset(buffer, 0, sizeof(buffer));
    }

    void init(int i, double v) {
        id = i;
        value = v;
        snprintf(buffer, sizeof(buffer), "Object_%d", i);
    }
};

void test_object_pool() {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "测试3：对象池管理" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    ObjectPool<TestObject> obj_pool(10, 100);

    std::cout << "\n[测试] 创建并获取对象..." << std::endl;
    std::vector<TestObject*> objects;

    for (int i = 0; i < 15; ++i) {
        TestObject* obj = obj_pool.acquire();
        if (obj) {
            obj->init(i, i * 3.14);
            objects.push_back(obj);
            std::cout << "  获取对象 " << i << ": id=" << obj->id << ", value=" << obj->value << std::endl;
        } else {
            std::cout << "  [警告] 无法获取对象 " << i << std::endl;
        }
    }

    obj_pool.print_stats();

    // 验证对象数据
    std::cout << "\n[测试] 验证对象数据..." << std::endl;
    bool data_valid = true;
    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i]->id != static_cast<int>(i)) {
            data_valid = false;
            std::cout << "  [错误] 对象 " << i << " 的id不匹配" << std::endl;
        }
    }

    if (data_valid) {
        std::cout << "[成功] 所有对象数据验证通过" << std::endl;
    }

    // 释放对象
    std::cout << "\n[测试] 释放对象..." << std::endl;
    for (size_t i = 0; i < objects.size(); ++i) {
        if (obj_pool.release(objects[i])) {
            std::cout << "  释放对象 " << i << std::endl;
        }
    }

    obj_pool.print_stats();
}

// ============================================================================
// 测试用例4：线程安全性
// ============================================================================

void test_thread_safety() {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "测试4：线程安全性" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    MemoryPoolManager pool;
    std::vector<std::thread> threads;
    const int THREAD_COUNT = 4;
    const int ALLOC_PER_THREAD = 50;

    std::cout << "\n[测试] 启动 " << THREAD_COUNT << " 个线程进行并发分配..." << std::endl;

    // 创建多个线程并发分配内存
    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&pool, t]() {
            std::vector<void*> local_ptrs;

            // 分配内存
            for (int i = 0; i < ALLOC_PER_THREAD; ++i) {
                size_t size = 1024 + (t * 100 + i) % 10000;
                void* ptr = pool.allocate(size);
                if (ptr) {
                    local_ptrs.push_back(ptr);
                }
            }

            std::cout << "  线程 " << t << " 分配了 " << local_ptrs.size() << " 个块" << std::endl;

            // 模拟一些工作
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // 释放内存
            for (void* ptr : local_ptrs) {
                pool.deallocate(ptr);
            }

            std::cout << "  线程 " << t << " 释放了 " << local_ptrs.size() << " 个块" << std::endl;
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "[成功] 所有线程执行完成" << std::endl;
    pool.print_all_stats();
}

// ============================================================================
// 测试用例5：内存对齐
// ============================================================================

void test_memory_alignment() {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "测试5：内存对齐" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    MemoryPoolManager pool;

    std::cout << "\n[测试] 分配不同大小的块验证对齐..." << std::endl;

    std::vector<size_t> test_sizes = {1, 7, 15, 16, 17, 31, 32, 33, 64, 128, 256, 512};
    std::vector<void*> ptrs;

    for (size_t size : test_sizes) {
        void* ptr = pool.allocate(size);
        if (ptr) {
            ptrs.push_back(ptr);

            // 检查对齐
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            bool aligned = (addr % MemoryBlock::ALIGNMENT) == 0;

            std::cout << "  大小: " << std::setw(3) << size << " 字节 -> "
                      << "地址: 0x" << std::hex << addr << std::dec
                      << " -> 对齐: " << (aligned ? "✓" : "✗") << std::endl;

            if (!aligned) {
                std::cerr << "[错误] 内存未正确对齐!" << std::endl;
            }
        }
    }

    std::cout << "\n[测试] 清理分配的内存..." << std::endl;
    for (void* ptr : ptrs) {
        pool.deallocate(ptr);
    }

    std::cout << "[成功] 内存对齐测试完成" << std::endl;
}

// ============================================================================
// 测试用例6：性能对比（vs 标准malloc）
// ============================================================================

void test_performance() {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "测试6：性能对比（内存池 vs 标准malloc）" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    const int ITERATIONS = 10000;
    const size_t ALLOC_SIZE = 4096;

    // 测试标准malloc
    std::cout << "\n[测试] 标准malloc性能测试（" << ITERATIONS << " 次分配）..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<void*> malloc_ptrs;
    for (int i = 0; i < ITERATIONS; ++i) {
        malloc_ptrs.push_back(malloc(ALLOC_SIZE));
    }

    auto malloc_alloc_time = std::chrono::high_resolution_clock::now();
    std::cout << "[成功] malloc分配耗时: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                  malloc_alloc_time - start).count() << " ms" << std::endl;

    for (void* ptr : malloc_ptrs) {
        free(ptr);
    }

    auto malloc_free_time = std::chrono::high_resolution_clock::now();
    std::cout << "[成功] malloc释放耗时: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                  malloc_free_time - malloc_alloc_time).count() << " ms" << std::endl;

    // 测试内存池
    std::cout << "\n[测试] 内存池性能测试（" << ITERATIONS << " 次分配）..." << std::endl;
    MemoryPoolManager pool(MemoryPoolConfig(
        256 * 1024,
        1024 * 1024,
        4 * 1024 * 1024,
        20
    ));

    start = std::chrono::high_resolution_clock::now();

    std::vector<void*> pool_ptrs;
    for (int i = 0; i < ITERATIONS; ++i) {
        void* ptr = pool.allocate(ALLOC_SIZE);
        if (ptr) {
            pool_ptrs.push_back(ptr);
        }
    }

    auto pool_alloc_time = std::chrono::high_resolution_clock::now();
    std::cout << "[成功] 内存池分配耗时: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                  pool_alloc_time - start).count() << " ms" << std::endl;

    for (void* ptr : pool_ptrs) {
        pool.deallocate(ptr);
    }

    auto pool_free_time = std::chrono::high_resolution_clock::now();
    std::cout << "[成功] 内存池释放耗时: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                  pool_free_time - pool_alloc_time).count() << " ms" << std::endl;

    // 性能对比总结
    std::cout << "\n========== 性能对比总结 ==========" << std::endl;
    std::cout << "内存池相对于malloc的优势在于减少系统调用和" << std::endl;
    std::cout << "提高缓存局部性，特别是在频繁分配释放场景。" << std::endl;
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                  高性能内存池/对象池管理系统 - 全功能测试                      ║" << std::endl;
    std::cout << "║                                                                              ║" << std::endl;
    std::cout << "║  功能特性：                                                                  ║" << std::endl;
    std::cout << "║  ✓ 多层级内存块管理（小/中/大块）                                           ║" << std::endl;
    std::cout << "║  ✓ 智能块拆分和合并减少碎片                                                  ║" << std::endl;
    std::cout << "║  ✓ 完整的线程安全机制                                                        ║" << std::endl;
    std::cout << "║  ✓ 灵活的对象池管理                                                          ║" << std::endl;
    std::cout << "║  ✓ 内存对齐支持（SSE/AVX优化）                                              ║" << std::endl;
    std::cout << "║  ✓ 全面的统计和监控                                                          ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════════════════╝" << std::endl;

    try {
        test_basic_allocation();
        test_fragmentation_and_compaction();
        test_object_pool();
        test_memory_alignment();
        test_thread_safety();
        test_performance();

        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "✓ 所有测试完成！" << std::endl;
        std::cout << std::string(80, '=') << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\n[异常] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
