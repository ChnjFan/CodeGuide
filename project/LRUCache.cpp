#include <iostream>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <random>

template<typename Key, typename Value>
class CacheNode {
private:
    std::shared_ptr<CacheNode<Key, Value>> next;
    std::shared_ptr<CacheNode<Key, Value>> prev;
    Key key;
    Value value;

public:
    CacheNode(Key k, Value v) : key(k), value(v), next(nullptr), prev(nullptr) {}
    Key getKey() { return key; }
    Value getValue() { return value; }
    void setValue(Value v) { value = v; }
    void setNext(std::shared_ptr<CacheNode<Key, Value>> n) { next = n; }
    void setPrev(std::shared_ptr<CacheNode<Key, Value>> p) { prev = p; }
    std::shared_ptr<CacheNode<Key, Value>> getNext() { return next; }
    std::shared_ptr<CacheNode<Key, Value>> getPrev() { return prev; }
};

template<typename Key, typename Value>
class LRUCache
{
public:
    using CacheNodePtr = std::shared_ptr<CacheNode<Key, Value>>;
    LRUCache(size_t capacity) : capacity(capacity) {
        dummy = std::make_shared<CacheNode<Key, Value>>(Key(), Value());
        dummy->setNext(dummy);
        dummy->setPrev(dummy);
    }

    Value get(Key key) {
       Value value{};
       get(key, value);
       return value;
    }

    bool get(Key key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = keyNodeMap.find(key);
        if (it == keyNodeMap.end()) {
            return false; 
        }

        // 将最近访问的缓存节点放在连表头部
        moveToMostRecent(it->second);
        value = it->second->getValue();
        return true;
    }

    void put(Key key, Value value) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = keyNodeMap.find(key);
        if (it != keyNodeMap.end()) {
            // 缓存节点已存在，更新节点值并将其移动到链表头部
            it->second->setValue(value);
            moveToMostRecent(it->second); 
        }
        if (isFull()) {
            // 移除最久未使用的缓存节点
            auto last = dummy->getPrev();
            removeNode(last);
            keyNodeMap.erase(last->getKey());
        }

        // 将新节点添加到链表头部
        auto newNode = std::make_shared<CacheNode<Key, Value>>(key, value);
        addToHead(newNode);
        keyNodeMap.insert({key, newNode});
    }

private:
    void moveToMostRecent(CacheNodePtr node) {
        removeNode(node);
        addToHead(node);
    }

    void removeNode(CacheNodePtr node) {
        node->getPrev()->setNext(node->getNext());
        node->getNext()->setPrev(node->getPrev());
    }

    void addToHead(CacheNodePtr node) {
        node->setNext(dummy->getNext());
        node->setPrev(dummy);
        dummy->getNext()->setPrev(node); 
    }

    bool isFull() {
        return keyNodeMap.size() >= capacity;
    }

    std::mutex mutex;   // 保护关键操作的锁，防止多线程并发访问时数据不一致的情况发生，保证线程安全。
    size_t capacity;
    CacheNodePtr dummy;   // 哨兵节点
    std::unordered_map<Key, CacheNodePtr> keyNodeMap;   // key-node哈希表，用来映射链表节点
};

// 测试LRUCache的性能和命中率
template<typename Key, typename Value>
void testLRUCachePerformance(LRUCache<Key, Value>& cache, size_t testSize, size_t keyRange) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, keyRange - 1);

    size_t hitCount = 0;
    size_t missCount = 0;

    // 记录开始时间
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < testSize; ++i) {
        Key key = dis(gen);
        Value value;
        cache.put(key, Value());
    }

    for (size_t i = 0; i < testSize; ++i) {
        Key key = dis(gen);
        Value value;
        if (cache.get(key, value)) {
            ++hitCount;
        } else {
            ++missCount;
        }
    }

    // 记录结束时间
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Test completed in " << elapsed.count() << " seconds." << std::endl;
    std::cout << "Cache hits: " << hitCount << std::endl;
    std::cout << "Cache misses: " << missCount << std::endl;
    double hitRate = static_cast<double>(hitCount) / (hitCount + missCount);
    std::cout << "Cache hit rate: " << hitRate * 100 << "%" << std::endl;
}

int main() {
    LRUCache<int, std::string> cache(5);
    testLRUCachePerformance(cache, 1000000, 100);
    return 0;
}