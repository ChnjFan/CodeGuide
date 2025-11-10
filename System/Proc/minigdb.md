# MiniGDB：掌握 GDB 核心原理

MiniGDB 在 Linux x86 架构环境下实现的简易调试器，实现以下功能：

- 启动并调试新进程：加载可执行文件，通过创建子进程运行。
- 附加已运行进程：控制正在运行中的进程进行调试。
- 管理断点：指定内存地址设置和删除端点。
- 控制执行流程：单步执行或指令级单步执行，或继续运行到断点。
- 检查程序状态：查看内存或寄存器内容。

[TOC]

## 启动并调试新进程

调试器运行后，通过 fork 函数创建子进程并允许父进程调试。

```cpp
void Debugger::launchProcess() {
  pid_t pid = fork();
  if (0 == pid) {//子进程
    ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);//允许父进程调试
    execl(exec_proc_file, exec_proc_name, nullptr);//执行目标程序
    exit(1);
  }
  else {
    exec_pid = pid;//保存子进程 PID
  }
}
```

调用 `fork()` 创建一个子进程，该函数会返回两次，父进程返回子进程的 PID，子进程返回 0。调用后父子进程将继续向后执行。

`ptrace` 接口是 GDB 的核心，基本上所有调试功能都需要调用该接口，可以让调试器进程观察和控制另一个进程，并检查和修改调试进程的内存和寄存器。

`ptrace` 接口通过第一个参数 `request` 实现不同操作。

`PTRACE_TRACEME` 由子进程调用，调用后父进程可以监控和控制子进程，被跟踪进程执行 `execl` 等操作会立即暂停，并发送 `SIGTRAP` 信号给父进程。

`execl` 接口调用执行指定被调试进程，会用新程序地换当前进程，包括进程的代码段、数据段和堆栈等，同时保持 PID 不变。

### 信号处理

调试器大部分时间都阻塞在等待子进程的状态变化。

```cpp
void Debugger::waitForSignal() {
  int wait_status;
  waitpid(exec_pid, &wait_status, 0);

  if (WIFSTOPPED(wait_status)) {//暂停类信号
    int signal = WSTOPSIG(wait_status);//获取暂停信号具体信号值
    if (SIGTRAP == signal) {//断点、单步执行和 execl 后的初始暂停触发
      m_registers.readAll();
      uint64_t pc = m_registers.getProgramCounter();
      Breakpoint* bp = getBreakpointAtAddress(pc - 1);
      if (bp && bp->isEnabled()) {
        //命中 int3 断点时，CPU 执行完该指令 RIP 会指向下一条指令
        //所以要手动将 RIP 置回断点所在地址，后续恢复原始指令继续执行
        m_registers.setProgramCounter(pc - 1);
        m_registers.writeAll();
      }
    }
  }
}
```

`waitpid` 用于等待子进程状态变化的系统调用，主要用于父进程监控子进程的退出或状态改变。

- `WIFSTOPPED`：子进程被暂停。
- `WSTOPSIG`：获取暂停子进程的信号。

`SIGTRAP` 信号处理的关键：

- 当软件断点被命中时，RIP（程序计数器）会指向断点指令（0xcc）的下一条指令。
- 断点查询时，如果 RIP 的上一条指令是已启用的断点，就需要将 RIP 重置到断点指令，后续回复该指令并继续执行。

## 附加已运行进程

调用 `ptrace` 的 `PTRACE_ATTACH` 请求，可以允许正在运行中的指定 PID 的进程被当前进程跟踪。

```cpp
void Debugger::attachProcess() {
    if (ptrace(PTRACE_ATTACH, exec_pid, nullptr, nullptr) < 0) {
        throw std::runtime_error("无法附加到进程: " + std::string(strerror(errno)));
    }
}
```

当进程 A 调用 `ptrace(PTRACE_ATTACH, pid, ...)` 时，会将进程 ID 为 `pid` 的进程 B 附加为自己的被跟踪进程，建立跟踪关系：

- 被跟踪进程 B 会立即收到 `SIGSTOP` 信号并暂停执行
- 跟踪进程 A 成为 B 的 “临时父进程”（但 B 的实际父进程 ID 不变）
- 被跟踪进程 B 暂停后，内核会向跟踪进程 A 发送子进程状态改变的通知。

## 断点管理

断点允许程序在执行到指定位置时自动暂停，实现方式主要分为软件断点和硬件断点：

- 软件断点：在指定的内存地址，将原始机器指令的第一个字节替换为特殊单字节指令，当 CPU 执行到这条特殊指令时，会触发一个软件中断，产生一个 `SIGTRAP` 信号。
- 硬件断点：CPU 硬件支持，通过寄存器（x86 的 DR0-DR7）保存断点地址，当 PC（指令指针）匹配到寄存器地址时，CPU 直接产生一个调试异常。

在 x86/x86_64 架构下，软件断点为 `int 3` 指令，机器码为 0xcc。执行到 `int 3` 指令后产生 `SIGTRAP` 信号，操作系统内核将这个信号发送给被调试进程，被调试进程被 ptrace 跟踪，所以信号会被调试器捕获。

### 断点启动

断点启动就是替换断点地址的指令为 `int 3` 指令，让 CPU 执行到这个地址时触发 `SIGTRAP` 信号。

```cpp
void Breakpoint::enable() {
    // 1. 读取断点地址处的8字节数据
    // ptrace读取的是long类型（64位系统上是8字节）
    long data = ptrace(PTRACE_PEEKDATA, m_pid, m_address, nullptr);
    // 2. 保存原始的第一个字节（低8位）
    // 这个字节是原始指令的第一个字节，后面需要恢复
    m_saved_byte = static_cast<uint8_t>(data & 0xFF);
    // 3. 构造新的数据：保持高56位不变，将低8位替换为0xcc（int3指令）
    // 0xcc是x86/x86-64架构的int3指令，用于触发断点异常
    uint64_t int3_instruction = 0xcc;
    uint64_t data_with_int3 = ((data & ~0xFF) | int3_instruction);
    // 4. 将修改后的数据写回目标进程的内存
    ptrace(PTRACE_POKEDATA, m_pid, m_address, data_with_int3);
    m_enabled = true;
}
```

调用 `ptrace` 接口的 `PTRACE_PEEKDATA` 请求查看指定内存地址的数据，读取的内存是 long 类型。

保存原始内存地址，后续继续执行还要将断点内存修改回来。

将指定内存地址的数据修改为 int 3 指令（0xcc）后，通过 `PTRACE_POKEDATA` 请求来写入内存中。

### 断点禁用

与断点启动类似，将断点地址的 `int 3` 指令恢复为原来的数据，程序就可以正常执行原始指令。

断点禁用发生在以下情况：

- 用户删除断点
- 单步执行时临时禁用断点执行原始指令

```cpp
void Breakpoint::disable() {
    // 1. 读取断点地址处的当前数据
    long data = ptrace(PTRACE_PEEKDATA, m_pid, m_address, nullptr);
    // 2. 构造恢复后的数据：保持高56位不变，将低8位恢复为原始字节
    uint64_t restored_data = ((data & ~0xFF) | m_saved_byte);
    // 3. 将恢复后的数据写回目标进程的内存
    ptrace(PTRACE_POKEDATA, m_pid, m_address, restored_data);
    m_enabled = false;
}
```

## 控制执行流程

被调试进程执行通过调用 `ptrace` 接口的请求字段控制：

- PTRACE_SINGLESTEP：单步执行
- PTRACE_CONT：继续执行

控制流程执行到断点时，需要处理断点指令，临时禁用断点后单步执行断点指令，然后恢复断点。

```cpp
void Debugger::stepOverBreakpoint() {
    m_registers.readAll();
    uint64_t pc = m_registers.getProgramCounter();

    Breakpoint* bp = getBreakpointAtAddress(pc);
    if (bp && bp->isEnabled()) {
        // 1. 禁用断点
        bp->disable();
        // 2. 单步执行
        ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
        waitForSignal();
        // 3. 重新启用断点
        bp->enable();
    }
}
```

## 寄存器读写

寄存器的读写通过 `ptrace` 接口的 `PTRACE_GETREGS` 和 `PTRACE_SETREGS` 请求实现。

```cpp
void Registers::readAll() {
    if (ptrace(PTRACE_GETREGS, m_pid, nullptr, &m_regs) < 0) {
        throw std::runtime_error("无法读取寄存器: " + std::string(strerror(errno)));
    }
}
void Registers::writeAll() {
    if (ptrace(PTRACE_SETREGS, m_pid, nullptr, &m_regs) < 0) {
        throw std::runtime_error("无法写入寄存器: " + std::string(strerror(errno)));
    }
}
```

寄存器结构体为 `user_regs_struct`，通过建立寄存器名跟结构体偏移值来获取或写入某个寄存器的值。
