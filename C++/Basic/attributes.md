# 属性标签

属性标签（attributes）在其他语言中又叫注解（annotations）。

[TOC]

注解标签的语法：

```cpp
[[attribute]] types/functions/enums/etc
```

标签可用于修饰任意类型、函数或者枚举，C++17 之后也可用于修饰命名空间和枚举类（限定作用域的枚举）。

## noreturn

`[[noreturn]]` 是 C++11 引入的**属性（attribute）**，核心作用是告诉编译器：被该属性修饰的函数**永远不会正常返回**，即不会执行到函数体的 `}` 结束位置，也不会通过 `return` 返回。

```cpp
[[noreturn]] void terminate();
```

主要在设计系统函数使用，例如 **std::abort()** 和 **std::exit()**。

## deprecated

`[[deprecated]]` 是 C++14 引入的属性，用于表示一个函数或者类型等已经被弃用。

如果继续使用编译器会提示告警，给代码加个慎用、已过时的提醒标签。

## fallthrough

`[[fallthrough]] `  是 C++17 引入的属性，用于 `switch-case` 语句，当前 `case` 分支执行完毕后，要 “贯穿” 到下一个 `case` 分支继续执行，同时抑制编译器因 “隐式贯穿” 产生的警告。

```cpp
switch (type)
{
case 1:
    func1();
    //这个位置缺少break语句，且没有fallthrough标注，
    //可能是一个逻辑错误，编译时编译器可能会给出警告，以提醒修改之
case 2:
    func2();
    //这里也缺少break语法，但是使用了fallthrough标注，
    //说明是开发者有意为之，编译器不会给出任何警告   
[[fallthrough]];
case 3:
    func3();
}
```

## nodiscard

`[[nodiscard]]` 是 C++17 引入的属性，**提醒编译器检查函数返回值是否被使用**，避免程序员无意中忽略关键的返回值（比如错误码、状态信息、资源句柄等）。

```cpp
[[nodiscard]] int connect(const char* address, short port) { /*实现省略*/ }

int main() {
    connect("127.0.0.1", 8888);// 编译警告
    return 0;
}
```

## maybe_ununsed

`[[maybe_unused]]` 是 C++17 引入的属性，标注的变量 / 函数 / 参数 / 类成员等 “可能未被使用”，无需为此抛出警告。

C++17 之前通过定义宏来显示调用未使用的变量来消除编译告警。

```cpp
#define UNREFERENCED_PARAMETER(x) x

int connect(const char*address, short port) {
    UNREFERENCED_PARAMETER(address);
    UNREFERENCED_PARAMETER(port);
    /* 业务逻辑 */
}

int connect_maybe_unused([[maybe_unused]] const char*address,
                         [[maybe_unused]] short port) { /* 业务逻辑 */ }
```

