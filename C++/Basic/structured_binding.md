# 结构化绑定

结构化绑定是 C++17 引入的语法糖，核心作用是用一行代码，直接把**数组、结构体、pair/tuple 等复合类型的成员「解包」到独立变量中**，告别手动 `.first`、`.second`、`[]` 索引。

[TOC]

## std::pair

`std::pair` 是 C++ 标准库中的**模板类**，核心作用是**把两个任意类型的变量打包成一个独立对象**，用来表示一对关联的数据。

```cpp
// 1. 创建pair：存储一个int和一个string
std::pair<int, std::string> id_name(101, "张三");
// 2. 直接访问成员
std::cout << id_name.first << std::endl;   // 输出：101
std::cout << id_name.second << std::endl;  // 输出：张三
// 3. 修改值
id_name.second = "李四";
```

- 作为函数返回值，**同时返回两个结果**（替代多返回值的繁琐写法）；
- 配合 `std::map` 使用：map 的每个元素本质都是 `std::pair<key, value>`；
- 临时打包两个关联数据，简化传参、存储。

## std::tuple

C++11 引入 `std::tuple`，是对 `std::pair` 的通用拓展，可以支持任意元素。

1. **异构存储**：可同时存放**不同类型**的元素（int、string、自定义类、指针等）；
2. **编译期定长**：元素数量在编译时确定，**无法动态增删**；
3. **有序访问**：通过**编译期索引**取值（`std::get<索引>`）；
4. **轻量高效**：无额外内存开销，替代临时结构体。

```cpp
std::tuple<std::string, std::string, int, int, std::string> userInfo("Tom", "123456", 0, 25, "Pudong Street");
    
std::string username = std::get<0>(userInfo);
std::string password = std::get<1>(userInfo);
int gender = std::get<2>(userInfo);
int age = std::get<3>(userInfo);
std::string address = std::get<4>(userInfo);
```

## 结构化绑定

C++17 引入结构化绑定，允许声明多个变量同时绑定到一个复合对象的成员或元素上，直接使用变量名访问，无需中间对象，避免像 `std::pair` 或 `std::tuple` 使用 `.first`/`.second` 或索引的方式来访问成员，让代码更易读、更少出错。

```cpp
auto [变量1, 变量2, 变量3, ...] = 复合对象;
```

- `[]` 内是你要定义的**变量名列表**
- 数量必须和复合对象的**元素 / 成员数量完全匹配**
- 支持 `auto`、`auto&`、`const auto&`、`auto&&` 四种绑定方式

```cpp
// 原生数组
int arr[] = {10, 20, 30};
auto [a, b, c] = arr;
// std::pair / std::tuple 不用再写 p.first / p.second
pair<int, string> p = {1, "hello"};
auto [id, name] = p; 
// 不用 get<0>(t)、get<1>(t)
tuple<int, double, bool> t = {100, 3.14, true};
auto [x, y, z] = t;
```

结构化绑定使用 `auto []` 是对变量进行拷贝，如果要修改或高效访问可以使用左值引用的方式 `auto& []` 绑定到原对象。

### 结构化绑定限制

结构化绑定不能使用 `constexpr` 修饰或被申明为 `static`。

**结构化绑定不是声明新的独立变量，而是为已有对象（结构体、元组、数组等）的成员 / 元素创建的「别名 / 引用」**，它没有独立的内存存储和变量实体，这是无法被 `constexpr`/`static` 修饰的根本原因。