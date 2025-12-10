# 右值引用和移动语义

右值引用和移动语义是 C++11 中非常重要的特性，主要用来优化程序性能、减少不必要的拷贝操作。

[TOC]

## 右值引用

- 左值：可以取地址、有名字的表达式，代表一个内存位置。
- 右值：不能取地址且没有名字的表达式，通常是临时对象或字面量，马上要被销毁，只能放在赋值号右边。

```cpp
int a = 10; // a是左值，10是右值，字面量
std::string("temp"); // 右值，临时对象
a+b; // 右值，临时结果，不能取地址
std::string str = "hello"; // str是左值，"hello"是右值字面量
```

右值引用是 C++11 新增的一种引用类型，**专门用来绑定到右值上**，语法是`&&`。

```cpp
int&& r1 = 10; // 右值引用绑定到纯右值（字面量）
std::string&& r2 = std::string("hello"); // 右值引用绑定到临时对象
int && r2 = r1;	// error 不能绑定左值
```

右值引用的作用是**延长右值的生命周期**：原本临时的右值（比如`std::string("hello")`）在表达式结束后就会被销毁，绑定到右值引用后，其生命周期会和右值引用的生命周期一致。

### std::move

`std::move` 是 C++11 标准库的模版函数，**将一个左值（或右值）强制转换为对应的右值引用类型**.

```cpp
int x = 10;
int&& rref = std::move(x);	// 将左值转换为右值引用类型
```

`std::move` 本身并不会做移动操作，它的作用是告诉编译器将对象作为临时对象处理，真正的移动发生在移动构造函数或移动赋值运算符（参数是右值引用）。

⚠️注意事项：

- **原对象状态**：调用 `std::move` 后原左值对象处于有效但未定义的状态，对象可以正常被析构，也可以重新赋值，但是不能访问原对象的资源，否则会导致未定义行为。
- **右值使用 `std::move` 是多余的**，右值本身已经可以绑定到右值引用。

对 `const` 对象使用 `std::move` 无效，退化为拷贝：

```cpp
const MyString s("hello");
MyString s2 = std::move(s); // 使用拷贝构造函数
```

## 移动语义

移动语义是右值引用最核心的应用，通过实现移动构造函数解决拷贝构造的性能损耗。

```cpp
class MyString {
public:
    // 构造函数
    MyString(const char* str = "") {
        if (str == nullptr) str = "";
        m_size = strlen(str);
        m_data = new char[m_size + 1];
        strcpy(m_data, str);
        std::cout << "构造函数：分配内存" << std::endl;
    }

    // 拷贝构造函数（深拷贝）
    MyString(const MyString& other) {
        m_size = other.m_size;
        m_data = new char[m_size + 1]; // 重新分配内存
        strcpy(m_data, other.m_data);  // 拷贝数据
        std::cout << "拷贝构造函数：深拷贝" << std::endl;
    }

    // 析构函数
    ~MyString() {
        if (m_data) {
            delete[] m_data;
            m_data = nullptr;
            std::cout << "析构函数：释放内存" << std::endl;
        }
    }

private:
    char* m_data; // 动态分配的内存
    size_t m_size;
};

// 测试函数：返回一个MyString临时对象
MyString createString() {
    MyString str("hello world");
    return str;
}

int main() {
    MyString s = createString(); // 调用拷贝构造？
    return 0;
}
```

`createString()` 返回一个临时对象（右值），赋值时会调用拷贝构造函数：分配新内存并拷贝数据。

### 移动构造函数

移动构造函数对右值不做深拷贝，直接转移其资源，原对象则为空状态。

移动构造函数的参数是右值引用。

```cpp
// 移动构造函数（参数是右值引用）
MyString(MyString&& other) noexcept {
    // 直接“偷”走other的资源
    m_data = other.m_data;
    m_size = other.m_size;

    // 将other置为空，避免析构时释放资源
    other.m_data = nullptr;
    other.m_size = 0;

    std::cout << "移动构造函数：转移资源" << std::endl;
}
```

STL（如 `std::vector`）在扩容时，如果移动构造函数标记了 `noexcept` （表示不会抛出异常）就会用移动而不是拷贝构造函数。如果没有 `noexcept` 为了保证异常安全，容器会退回到拷贝构造，移动语义失效。

[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#rc-move-noexcept) 明确要求：移动构造函数和移动赋值运算符必须标记为 `noexcept`。

移动构造函数的使用场景：

```cpp
// 返回命名局部对象
MyString s = createString(); // 编译器有限RVO，其次考虑移动构造函数
// 右值赋值
MyString s1("hello");
MyString s2 = std::move(s1); // 显示移动
// 容器插入
std::vector<MyString> vec;
vec.push_back(MyString("hello")); // 临时对象自动移动
```

### 移动赋值运算符

移动赋值运算符需要注意将对象资源释放后再转移，防止内存泄漏：

```cpp
// 移动赋值运算符
MyString& operator=(MyString&& other) noexcept {
    if (this == &other) { // 防止自赋值
        return *this;
    }

    // 释放当前对象的资源
    delete[] m_data;

    // 转移other的资源
    m_data = other.m_data;
    m_size = other.m_size;

    // 置空other
    other.m_data = nullptr;
    other.m_size = 0;

    std::cout << "移动赋值运算符：转移资源" << std::endl;
    return *this;
}
```

## 拷贝消除

copy elision 是 C++ 标准规定的**编译器优化技术**，核心目的是**避免不必要的拷贝 / 移动构造**，直接复用对象内存，提升性能，且优化结果不会改变程序可观察行为（符合 “as-if” 规则）。

### 返回值优化

RVO（Return Value Optimization）是编译器的一项优化：函数返回局部对象时，编译器直接在调用方目标对象的内存地址上构造对象，避免两次拷贝操作或移动构造操作。

```cpp
std::string create_str() {
    return std::string("hello"); // 无拷贝，直接在调用方接收地址构造
}
std::string s = create_str(); // 直接复用返回对象的内存
```

### 具名返回值优化

NRVO（Named RVO）是对 RVO 的扩展，函数返回非匿名临时对象时，编译器直接在目标地址构造该非匿名对象。

```cpp
std::string create_str() {
    std::string tmp("hello"); // 具名局部对象
    return tmp; // 无拷贝，直接将 tmp 构造在 s 的地址
}
std::string s = create_str();
```

如果函数返回值返回 `std::move` 右值引用，编译器认为是返回一个引用，不会进行 RVO 优化。

```cpp
std::string create_str() {
    std::string tmp("hello");
    return std::move(tmp); // 返回右值引用，不会优化
}
```

### C++17 强制拷贝消除

C++17 对于纯右值（prvalue）强制进行消除拷贝和移动。

```cpp
MyString s = MyString("hello"); // 强制在 s 的内存位置直接构造
```

在 C++11/14 中，上面的语句首先会创建临时对象 `MyString("hello")`，然后调用移动构造函数将临时对象移动到 `s` 后再销毁临时对象。

