# enable_shared_from_this

`enable_shared_from_this` 让一个被 `std::shared_ptr` 管理的对象，能在自己的成员函数中安全获取指向自身的 `shared_ptr`，替代裸指针 `this`。避免同一个裸指针被多个独立控制块管理，导致重复释放问题。

## 解决问题

场景：`std::shared_ptr` 管理的对象内部成员函数中，返回自身的对象给外部使用。

如果直接使用 `this` 指针构造并返回，那么就会有两个独立的智能指针指向同一块内存，析构时会出现内存重复释放的问题。

```cpp
struct SomeOne {
    std::shared_ptr<SomeOne> getSelf() {
        return std::shared_ptr<SomeOne>(this);
    }
};
int main() {
    auto a = std::make_shared<SomeOne>(); // 控制块 A 引用计数=1
    auto b = a.getSelf();	// 使用 this 建立了控制块 B 引用计数=1
    // a、b 同时指向同一个对象，各自维护一份引用计数，析构时重复释放
}
```

通过继承 CRTP `std::enable_shared_from_this<SomeOne>` 后，调用方法 `shared_from_this` 获取当前对象的智能指针。

```cpp
struct SomeOne : public std::enable_from_this<SomeOne> {
    std::shared_ptr<SomeOne> getSelf() {
        return shared_from_this();
    }
};
```

## 实现原理

