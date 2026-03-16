# 设计原则

设计原则是**设计模式的底层思想**，面向对象设计的五大原则 SOLID，所有设计模式都是为了遵守这些原则。

[TOC]

## 单一职责原则 SRP

一个类 / 模块，只负责一件事，只有一个引起它变化的原因。

**核心思想**

- 职责越单一，越稳定、易维护、易复用。
- 避免 “上帝类”：什么都做，牵一发而动全身。

## 开闭原则 OCP

对扩展开放，对修改关闭。

- 新增功能时，只新增代码不修改原有代码。

通过定义接口和抽象类，扩展实现不会修改已有代码。

## 里氏替换原则 LSP

子类必须能完全替换父类，且不破坏程序正确性。

- 子类可以扩展，不能重写父类已实现的核心方法。
- 子类的前置条件不能强于父类，父类方法要求的输入的条件，子类不能要求更严格。
- 子类的后置条件不能弱于父类，父类方法承诺的输出结果（异常、返回值范围等），子类不能降低标准。

## 接口隔离原则 ISP

接口要小，避免实现类被迫实现一堆用不上的方法。

## 依赖倒置原则 DIP

依赖倒置原则（Dependence Inversion Principle）是设计模式中最核心的原则之一，也是实现 “开闭原则” 的关键。

核心定义：

- 高层模块不依赖低层模块，二者都依赖抽象。
- 面向接口编程，而不是面向具体实现编程。

### DIP 核心步骤

**步骤一：识别高低层模块和稳定易变部分。**

- 高层模块：核心业务逻辑，如订单创建、用户下单，稳定。
- 低层模块：具体实现，如数据库操作、缓存、第三方接口调用，易变。

核心是为易变的低层模块抽象出稳定的接口或抽象类。

**步骤二：定义抽象接口**

抽象接口只定义做什么，不定义怎么做，接口要贴合高层模块的需求。

**步骤三：高层模块依赖抽象而非具体实现**

高层模块依赖抽象，不直接 `new` 低层具体类。

**步骤四：低层模块实现抽象接口**

所有具体实现都遵循抽象接口的规范，替换实现高层模块无感知。

**步骤五：通过 “注入” 解耦对象创建（关键）**

避免高层模块自己创建低层实现对象（如 `new MySQLDB()`），而是通过**构造函数注入、方法注入、工厂 / 容器注入**的方式传递实现类，彻底解耦。

## 应用场景

### 数据库访问层 DAO

DAO 数据访问对象是将**数据访问逻辑**和**业务逻辑**分离的设计模式。

- 分装对数据库的所有操作，对外提供同一的接口。
- 让业务层只关注做什么，不用关心怎么做。

```bash
┌───────────────┐
│ 业务层（Service） │ 高层模块：处理核心业务逻辑（如订单创建、用户登录），依赖DAO接口
└───────┬───────┘
        │ 依赖抽象接口
┌───────▼───────┐
│    DAO层       │ 核心层：定义DAO接口 + 实现具体数据访问逻辑
│  ┌─────────┐  │
│  │ DAO接口 │   │ 抽象：定义增删改查方法（IOrderDAO、IUserDAO）
│  └────┬────┘  │
│       │ 实现   │
│  ┌────▼────┐  │
│  │ DAO实现  │  │ 具体：对接MySQL/PG/Redis（MySQLOrderDAO、PGOrderDAO）
│  └─────────┘  │
└───────┬───────┘
        │ 操作数据源
┌───────▼───────┐
│ 数据源（DB）    │ 低层模块：MySQL、PostgreSQL、Redis、文件等
└───────────────┘
```

反例：业务层直接依赖 MySQL 具体实现，切换到其他数据库时所有业务代码都需要改：

```cpp
// 反例：高层业务依赖低层具体实现
class OrderService { // 高层：订单业务
private:
    MySQLDAO dao; // 直接依赖MySQL具体类
public:
    void createOrder(Order order) {
        dao.insertOrder(order); // 耦合MySQL
    }
};
```

正例：解耦业务逻辑和数据访问逻辑，数据库访问逻辑修改后业务逻辑不需要变。

```cpp
// 步骤1：定义抽象接口（稳定）
class IOrderDAO { // 抽象：只定义行为，不关心具体数据库
public:
    virtual ~IOrderDAO() = default;
    virtual void insertOrder(const Order& order) = 0;
    virtual Order getOrderById(int id) = 0;
};

// 步骤2：低层实现抽象接口（易变）
class MySQLOrderDAO : public IOrderDAO {
public:
    void insertOrder(const Order& order) override {
        // MySQL具体实现：执行INSERT语句
        std::cout << "MySQL插入订单：" << order.id << std::endl;
    }
    Order getOrderById(int id) override { /* MySQL查询逻辑 */ }
};

class PostgreOrderDAO : public IOrderDAO {
public:
    void insertOrder(const Order& order) override {
        // PostgreSQL具体实现
        std::cout << "PostgreSQL插入订单：" << order.id << std::endl;
    }
    Order getOrderById(int id) override { /* PG查询逻辑 */ }
};

// 步骤3：高层依赖抽象 + 注入实现
class OrderService {
private:
    IOrderDAO* dao; // 依赖抽象，而非具体类
public:
    // 构造函数注入：外部传递具体实现
    OrderService(IOrderDAO* daoImpl) : dao(daoImpl) {}
    
    void createOrder(Order order) {
        dao->insertOrder(order); // 调用抽象接口，无耦合
    }
};

// 调用示例（业务层无需修改，仅切换注入的实现）
int main() {
    // 切换数据库：只需改这一行
    IOrderDAO* mysqlDAO = new MySQLOrderDAO();
    // IOrderDAO* pgDAO = new PostgreOrderDAO();
    
    OrderService service(mysqlDAO);
    service.createOrder({1, "手机", 2999});
    
    delete mysqlDAO;
    return 0;
}
```

### 支付场景

通过抽象支付接口，新增支付方式不需要修改支付业务。

```cpp
// 抽象支付接口
class IPayment {
public:
    virtual ~IPayment() = default;
    virtual bool pay(double amount) = 0;
};
// 具体支付方式实现
class WechatPayment : public IPayment {
public:
    bool pay(double amount) override {
        std::cout << "WechatPayment pay " << amount << std::endl;
        return true;
    }
};

class AliPayPayment : public IPayment {
public:
    bool pay(double amount) override {
        std::cout << "AliPayPayment pay " << amount << std::endl;
        return true;
    }
};

// 高层业务模块，支付服务
class PaymentService {
public:
    PaymentService(std::unique_ptr<IPayment> payment) : payment_(payment) {}
    ~PaymentService() = default;

    bool processPayment(double amount) {
        assert(amount > 0);
        // 调用抽象接口，不关心具体支付方式
        return payment_->pay(amount);
    }
private:
    std::unique_ptr<IPayment> payment_;// 构造函数注入支付方式
};

class PaymentFactory {
public:
    static std::unique_ptr<IPayment> createPayment(const std::string& payment_type) {
        if (payment_type == "wechat") {
            return std::make_unique<WechatPayment>();
        } else if (payment_type == "alipay") {
            return std::make_unique<AliPayPayment>();
        } else {
            return nullptr;
        }
    }
};

int main()
{
    // 创建支付服务
    std::unique_ptr<IPayment> payment = PaymentFactory::createPayment("wechat");
    PaymentService payment_service(payment);

    // 处理支付
    bool success = payment_service.processPayment(100.0);
    if (success) {
        std::cout << "Payment successful" << std::endl;
    } else {
        std::cout << "Payment failed" << std::endl;
    }

    return 0;
}
```

