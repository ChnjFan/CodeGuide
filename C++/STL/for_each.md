# for_each

`std::for_each` 是 C++ STL 中的通用遍历算法，定义于 `<algorithm>` 头文件，核心作用是**对容器（或迭代器范围）的每个元素执行指定操作**，支持串行 / 并行执行（C++20 后结合执行器）。

[TOC]

## 核心定义

`std::for_each` 是 `<algorithm>` 头文件中最基础的遍历算法，核心语义是：对迭代器区间 `[first, last)` 中的每个元素，执行一次指定的可调用对象。

```cpp
//C++98
template <class InputIt, class UnaryFunction>
UnaryFunction for_each(InputIt first, InputIt last, UnaryFunction f);
//C++20：支持执行策略（并行遍历）
template <class ExecutionPolicy, class ForwardIt, class UnaryFunction>
UnaryFunction for_each(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last, UnaryFunction f);
```

