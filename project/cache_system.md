# 缓存系统

> 本文根据场景的缓存策略实现缓存系统，常见的缓存策略包括：LRU、LFU 和 ARC。

## 介绍

缓存系统是一种用来**提高数据访问速度**和**减轻后端负载**的机制。它通过**存储经常访问的数据副本**，当系统再次需要这些数据时，可以更快地获取它们，而不必重新从原始来源（比如数据库、磁盘、远程服务等）读取。

设计缓存需要牺牲一部分服务器内存，通过减少对磁盘或数据的直接访问来提升响应速度。由于服务器内存有限，所以当缓存的数据超过一定的限制后，需要制定合理的缓存淘汰策略来删除部分缓存。

### 使用场景

假设一个网站需要提高性能，缓存可以放在浏览器，反向代理服务器，还可以放在应用程序和分布式缓存系统中。

![cache](./cache.png)

从用户请求数据到数据返回，数据经过了浏览器、CDN、代理服务器、应用服务区器和数据库的各个环节，都可以运用到缓存技术。

缓存的顺序：用户请求->HTTP缓存->CDN缓存->代理服务器缓存->进程内缓存->分布式缓存->数据库

## 缓存淘汰策略

> 访问很少的数据在完成访问后还残留在缓存中，会造成缓存空间的浪费，称为缓存污染。通过合理设计缓存策略，可以有效降低缓存污染的风险。

在缓存空间有限的情况下，淘汰策略决定了哪些数据会被移除以腾出空间。常见的缓存淘汰策略有：

- **LRU（Least Recently Used）**：移除最近最少使用的数据，适用于数据访问频率较为均衡的场景。
- **LFU（Least Frequently Used）**：移除访问频率最低的数据，更能体现数据的长期价值，但实现相对复杂。
- **FIFO（First-In, First-Out）**：先进入缓存的数据先被淘汰，算法简单，但可能不符合实际使用场景。

### FIFO

先进先出算法是一种非常简单、直观的数据淘汰策略，其基本原理是：最早进入缓存的数据优先被淘汰，就像队列中最先进入的人先被服务或出队一样。

FIFO 不考虑数据访问的频繁程度或者依然处于活跃状态，会导致高频数据被提前淘汰。

可能导致 Belady 异常，指的是在增加缓存容量的情况下，反而可能导致更高的缺页率。

### LRU

最近最少算法在缓存满的时候，优先淘汰最久未被访问的数据。

每当数据被访问时，需要更新数据的访问时间。常见的实现方式是双向链表+哈希表，通过哈希表快速定位缓存项，双向链表维持数据访问顺序，可以实现 O(1) 时间复杂度的访问、更新和删除操作。

LRU 在高并发、大数据量常见，频繁更新链表开销较大。

部分场景下无法反映数据的访问频率，造成高频数据被淘汰。例如，过去一小时中的 59 分钟都在访问同一个缓存数据，但最后一分钟访问了大量其他数据，造成前 59 分钟高频访问的数据被淘汰。

典型 LRU 实现：

```cpp
#include <iostream>
#include <memory>
#include <unordered_map>

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

    size_t capacity;
    CacheNodePtr dummy;   // 哨兵节点
    std::unordered_map<Key, CacheNodePtr> keyNodeMap;   // key-node哈希表，用来映射链表节点
};
```

通过双向链表+哈希表实现的 LRU 算法可以满足缓存需求，热点数据较多时有较高的命中率，但是在某些场景下仍有不足：

1. 循环重复数据可能会被逐步清空，几乎无法命中。
2. 缓存污染：加载冷数据可能会导致热点数据被淘汰，降低缓存利用率。
3. 最近最少使用并不一定是冷数据，在某些时刻可能恰巧没有访问到。
4. 多线程高并发场景，锁粒度太大。

**LRU-k**

LRU-k 算法是对 LRU 的改进，基础 LRU 算法的访问数据进入缓存队列只需要进行一次 put 操作，而 LRU-k 需要被访问 k 次才能被放入缓存中。

LRU-k 除了缓存队列外，还需要一个数据访问历史队列，用来保存未在缓存队列数据的访问次数，当数据被访问次数超过 k 次，才将缓存数据添加到缓存队列中。

```cpp
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
```



### LFU





## 数据更新与一致性策略



## 缓存更新与刷新策略



## 参考资料

1. [【架构师面试-缓存与搜索-1】-缓存与缓存置换策略源码实现_自适应缓存替换算法-CSDN博客](https://blog.csdn.net/chongfa2008/article/details/121956961)
2. [146. LRU 缓存 - 力扣（LeetCode）](https://leetcode.cn/problems/lru-cache/solutions/2456294/tu-jie-yi-zhang-tu-miao-dong-lrupythonja-czgt/)
3. [LRU算法及其优化策略——算法篇LRU算法全称是最近最少使用算法（Least Recently Use），广泛的应用于缓 - 掘金](https://juejin.cn/post/6844904049263771662)