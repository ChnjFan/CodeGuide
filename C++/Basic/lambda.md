# lambda 表达式

C++11 正式引入 **Lambda 表达式**，它是 C++ 最实用、最常用的特性之一，核心用途：**就地定义匿名函数、简化代码、配合 STL 算法、捕获外部变量**。

[TOC]

## 基本语法

Lambda 是一个可以定义在函数内部的「匿名函数」，能直接捕获外部变量，代码更短、更直观。

```cpp
[捕获列表] (参数列表) 可选->返回值类型 { 函数体 }
```

### 捕获列表

捕获列表定义 Lambda 表达式如何访问外部作用于的局部变量和成员变量。

| 写法     | 名称         | 作用                         |
| -------- | ------------ | ---------------------------- |
| `[]`     | 无捕获       | 不能使用任何外部局部变量     |
| `[var]`  | 值拷贝捕获   | 单独捕获某个变量，拷贝一份   |
| `[&var]` | 引用捕获     | 单独捕获某个变量，拿别名     |
| `[=]`    | 全部值捕获   | 外部所有变量全部**拷贝**进来 |
| `[&]`    | 全部引用捕获 | 外部所有变量全部**引用**进来 |

值拷贝捕获将外部变量复制一份到 Lambda 内部，默认只读不能修改，如果要修改需要加 `mutable`。

```cpp
int a = 10;
auto f = [a]() mutable {
    a = 20;     // 只改内部副本
    cout << a;  // 20
};
f();
cout << a; // 外部仍为 10，互不影响
```

引用捕获让 Lambda 内部拿到变量的引用，可以直接修改。

⚠️**注意**：引用捕获一定要注意引用变量的生命周期如果比 Lambda 的生命周期小，会造成空悬引用的访问，程序崩溃。值捕获 this 指针同样可能在 Lambda 执行期间对象已经被销毁，访问野指针。

```cpp
auto badFunc()
{
    int x = 10;
    return [&x](){ cout << x; }; 
    // x 函数结束销毁，Lambda 拿着野引用 → 崩溃
}
```

C++17 中提供了对象拷贝捕获，通过 `[*this]` 捕获拷贝整个对象。

多线程异步操作中，最常用的方法是通过值捕获智能指针防止对象被释放。

```cpp
class MyClass : public std::enable_shared_from_this<MyClass>
{
public:
    void func() {
        auto self = shared_from_this();
        // 值捕获 self，延长对象生命周期
        thread([self]() {
            // 安全访问
        }).detach();
    }
};
```

## Lambda 实现

lambda 在 C+++ 中本质上是在编译期生成的一个**匿名类对象**，通过重载可调用操作符 `operator()` 实现调用执行。

```cpp
int main() { 
  class __lambda_6_15 {
    public: 
    inline /*constexpr */ int operator()(int a, int b) const {
      return a + b;
    }
    using retType_6_15 = int (*)(int, int);// 无捕获 Lambda 可以隐式转换为函数指针
    inline constexpr operator retType_6_15 () const noexcept {
      return __invoke;
    };
    private: 
    static inline /*constexpr */ int __invoke(int a, int b) {
      return __lambda_6_15{}.operator()(a, b);
    }
  };
  __lambda_6_15 ld = __lambda_6_15{};
  std::cout.operator<<(ld.operator()(1, 2));// Lambda 实际是执行调用运算符
  return 0;
}
```

我们通过 cppinsights 运行可以看到，C++ 编译期将 Lambda 表达式生成了一个类，并且重载了 `operator()` 来实现 Lambda 的函数体。

重载的  `operator()` 默认是 `const`，不可修改自身的内部状态，值传递默认是不可变的。

如果有捕获的变量，则在类中通过成员变量来保存。

```cpp
class __lambda_7_15
{
	public: 
	inline /*constexpr */ int operator()(int a, int b) const {
  		return a + b;
	}
	// private: 
	int cc;
	int & dd;
};
```

