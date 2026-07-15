# 单例模式

单例模式保证一个类在整个程序的生命周期中只有一个对象，并且提供统一的访问接口给全局使用。

像一些全局配置类，模型注册表、数据库连接池，全局只有一份资源，通过单例模式统一管理和配置，避免重复创建销毁造成性能损耗，资源冲突。

```cpp
template <typename T>
class Singleton {
public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    
    static std::shared_ptr<T> getInstance() {
        static std::once_flag initFlag;
        std::call_once(initFlag, [&](){
            instance = std::shared_ptr<Singleton<T>>(new T);
        });
        return instance;
    }
protected:
    Singleton() = default;
    static std::shared_ptr<T> instance;
};

template <typename T>
std::shared_ptr<T> Singleton<T>::instance = nullptr;

class MyClass : public Singleton<MyClass> {
public:
    ~MyClass() = default;
private:
    friend class Singleton<Myclass>;
    MyClass() = default;
};
```

