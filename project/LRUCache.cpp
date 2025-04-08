#include <iostream>
#include <memory>

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
    LRUCache(size_t capacity) {
        dummy->setNext(dummy);
        dummy->setPrev(dummy);
        this->capacity = capacity;
    }

private:
    size_t capacity;
    std::shared_ptr<CacheNode<Key, Value>> dummy;   // 哨兵节点
    std::unordered_map<Key, std::shared_ptr<CacheNode<Key, Value>>> keyNodeMap;   // key-node哈希表
};

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}