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

    void remove(Key key) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = keyNodeMap.find(key);
        if (it != keyNodeMap.end()) {
            removeNode(it->second);
            keyNodeMap.erase(it);
        }
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

template<typename Key, typename Value>
class LRUKCache : public LRUCache<Key, Value> {
public:
    LRUKCache(size_t capacity, size_t k, size_t historyCapacity) : LRUCache<Key, Value>(capacity), k(k), historyList(std::make_unique<LRUCache<Key, int>>(historyCapacity)) {}

    bool get(Key key, Value& value) {
        int count = historyList->get(key);   // 获取历史访问缓存队列中key对应的访问次数
        historyList->put(key, count + 1);   // 将key对应的访问次数加1，并更新历史访问缓存队列
        return LRUCache<Key, Value>::get(key, value);
    }

    void put(Key key, Value value) {
       if (LRUCache<Key, Value>::get(key) != "") {   // 如果缓存中存在key对应的缓存节点，则更新缓存节点的值并将其移动到最近使用的位置
            LRUCache<Key, Value>::put(key, value);
        }
        int count = historyList->get(key);   // 获取历史访问缓存队列中key对应的访问次数
        historyList->put(key, count + 1);
        if (count >= k) {   // 如果key对应的访问次数大于等于k，则将其从历史访问缓存队列中移除
            historyList->remove(key);
            LRUCache<Key, Value>::put(key, value);
       }
    }

private:
    size_t k;   // 缓存节点的访问次数阈值，当缓存节点的访问次数超过k时，该节点将被视为最近使用的节点。
    std::unique_ptr<LRUCache<Key, int>> historyList;   // 历史访问缓存队列
};

// 测试LRUCache的性能和命中率
void testLRUCachePerformance(size_t testSize, size_t keyRange) {
    LRUCache<int, std::string> cache(5);
    LRUKCache<int, std::string> kCache(5, 2, 10);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, keyRange - 1);

    size_t hitCount = 0;
    size_t missCount = 0;

    size_t khitCount = 0;
    size_t kmissCount = 0;

    // 记录开始时间
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < testSize; ++i) {
        int key = dis(gen);
        std::string value = "Value" + std::to_string(key);
        cache.put(key, value);
        kCache.put(key, value);
    }

    for (size_t i = 0; i < testSize; ++i) {
        int key = dis(gen);
        std::string value = "Value" + std::to_string(key);
        if (cache.get(key, value)) {
            ++hitCount;
        } else {
            ++missCount;
        }

        if (kCache.get(key, value)) {
            ++khitCount; 
        } else {
            ++kmissCount;
        }
    }

    // 记录结束时间
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Test completed in " << elapsed.count() << " seconds." << std::endl;
    std::cout << "LRU Cache hits: " << hitCount << std::endl;
    std::cout << "LRU Cache misses: " << missCount << std::endl;
    double hitRate = static_cast<double>(hitCount) / (hitCount + missCount);
    std::cout << "LRU Cache hit rate: " << hitRate * 100 << "%" << std::endl;

    std::cout << "LRU-k Cache hits: " << khitCount << std::endl;
    std::cout << "LRU-k Cache misses: " << kmissCount << std::endl;
    hitRate = static_cast<double>(khitCount) / (khitCount + kmissCount);
    std::cout << "LRU-k Cache hit rate: " << hitRate * 100 << "%" << std::endl;
}

int main() {
    testLRUCachePerformance(100000, 10);
    return 0;
}