# tin 代码设计审查报告（2026-07-20）

> **审查视角**：Google C++ Style Guide、代码大全、网络栈架构、协程运行时设计、正交设计、命名规范、类关系设计
> **审查范围**：`include/tin/`（公共 API）、`tin/`（内部实现）、`examples/`
> **约束**：不增加功能；`GUintptr`、`PollDescriptor` 裸指针不改（lock-free 算法约束）
> **基线**：P0–P2 已完成，P3 部分完成

---

## 目录

1. [命名一致性](#一命名一致性)
2. [类设计与类关系](#二类设计与类关系)
3. [正交设计与职责分离](#三正交设计与职责分离)
4. [内存安全与资源管理](#四内存安全与资源管理)
5. [现代 C++ 实践](#五现代-c-实践)
6. [API 设计与接口边界](#六api-设计与接口边界)
7. [错误处理体系](#七错误处理体系)
8. [并发与同步原语](#八并发与同步原语)
9. [头文件依赖与编译隔离](#九头文件依赖与编译隔离)
10. [问题汇总优先级表](#十问题汇总优先级表)

---

## 一、命名一致性

### 1.1 头文件保护宏与 `#pragma once` 混用

**严重度**：🟡 中

项目中 `#pragma once` 和 `#ifndef` 守卫混用，甚至在同一文件中同时出现两种：

| 文件 | 当前 | 问题 |
|---|---|---|
| `include/tin/error/error.h` | `#pragma once` | 无 `#ifndef` 守卫 |
| `include/tin/config/config.h` | `#ifndef` + `#pragma once` | 两者同时存在，冗余 |
| `include/tin/sync/mutex.h` | `#pragma once` | 无 `#ifndef` 守卫 |
| `include/tin/net/tcp_conn.h` | `#ifndef TIN_NET_TCP_CONN_H_` | 正确 |
| `include/tin/status.h` | `#ifndef TIN_STATUS_H_` | 正确 |

**建议**：Google C++ Style Guide 明确规定**使用 `#ifndef` 守卫，不使用 `#pragma once`**。统一替换为 `#ifndef` 守卫，命名格式 `TIN_<PATH>_<FILE>_H_`。

### 1.2 命名空间风格混用

**严重度**：🟡 中

```cpp
// 风格 A（旧式嵌套）— 大量使用
// include/tin/net/tcp_conn.h, ip_address.h, ip_endpoint.h, dialer.h, ...
namespace tin {
namespace net {
...
}  // namespace net
}  // namespace tin

// 风格 B（C++17 简洁式）— 部分使用
// include/tin/net/listener.h, tin/net/tcp_conn_impl.h, tin/runtime/m.h, ...
namespace tin::net {
...
}  // namespace tin::net
```

同一项目、同一目录下两种风格并存，影响可读性。

**建议**：统一为 `namespace tin::net { }`（C++17 风格），批量替换。

### 1.3 类名大小写不统一

**严重度**：🟡 中

| 类名 | 风格 | 位置 |
|---|---|---|
| `TcpConn` | `Tcp` 前缀 + `Conn` | `net/tcp_conn.h` |
| `TCPListener` | `TCP` 全大写前缀 + `Listener` | `net/listener.h` |
| `IPAddress` | `IP` 全大写前缀 + `Address` | `net/ip_address.h` |
| `IPEndPoint` | `IP` 全大写前缀 + `EndPoint` | `net/ip_endpoint.h` |
| `IOBuffer` | `IO` 全大写前缀 + `Buffer` | `io/io_buffer.h` |
| `IOReadWriter` | `IO` 全大写前缀 | `io/io.h` |

`TcpConn` 用 `Tcp`（首字母大写），`TCPListener` 用 `TCP`（全大写），同属 `net` 模块但风格不一致。Google Style 规定缩写在类名中**首字母大写即可**（如 `TcpConn`、`UrlTable`），不全部大写。

**建议**：统一为 `TcpListener`、`IpAddress`、`IpEndpoint`、`IoBuffer`、`IoReadWriter`。由于这些是公共 API，需提供 `deprecated` 别名过渡：

```cpp
[[deprecated("Use TcpListener")]] using TCPListener = TcpListener;
```

### 1.4 拼写错误残留

**严重度**：🟡 中

| 位置 | 当前 | 正确 | 备注 |
|---|---|---|---|
| `tin/runtime/runtime.h:22` | `ErrorOccured()` | `ErrorOccurred` | 双 r |
| `tin/runtime/guintptr.h:28` | `ingeter` 参数名 | `integer` | 构造函数参数 |
| `tin/runtime/scheduler.h:30` | `maximium` 参数名 | `maximum` | `GlobalRunqGet` 参数 |
| `include/tin/config/default.h:13` | `kStackAllignment` | `kStackAlignment` | 常量名 |
| `include/tin/error/error.h:126` | `"unkown error"` | `"unknown error"` | `TinErrorName` 返回值 |

P2-3 修复了 `ThreadPoll`→`ThreadPool`、`Broascast`→`Broadcast` 等，但上述遗漏。

### 1.5 `CACHELINE_SIZE` 宏定义带尾部分号

**严重度**：🔴 低（但会导致编译错误）

```cpp
// include/tin/config/default.h:15
#define CACHELINE_SIZE 64;  // ← 尾部分号！
```

使用时 `ALIGNAS(CACHELINE_SIZE)` 展开为 `ALIGNAS(64;)` 会导致编译错误。应删除分号，并改用 `constexpr`：

```cpp
constexpr int kCacheLineSize = 64;
```

### 1.6 文件名大小写不统一

**严重度**：🟢 低

P2-2 提到统一为 `snake_case.h`，但实际仍存在不一致：

| 文件 | 风格 |
|---|---|
| `threadpoll.h`（已修复拼写但）| snake_case ✓ |
| `netpoll_epoll.cc` | snake_case ✓ |
| `IPAddress` 对应文件 `ip_address.h` | snake_case ✓ |
| `IOBuffer` 对应文件 `io_buffer.h` | snake_case ✓ |

文件名已基本统一，但**类名与文件名映射关系不一致**：`TCPListener` → `listener.h`（缩写了），`TcpConn` → `tcp_conn.h`（没缩写）。

---

## 二、类设计与类关系

### 2.1 `IOBuffer` 设计混乱

**严重度**：🔴 高

`include/tin/io/io_buffer.h` 存在多个设计问题：

```cpp
class IOBuffer {
 public:           // ← 第一个 public
  IOBuffer();
  explicit IOBuffer(size_t size);
  ~IOBuffer();

  std::string str() const;
  using iterator = char*;
  // ... iterator/buffered/free/empty/full/clear ...

  int Write(const void* ptr, size_t size);
  // ... GetWritablePtr/GetReadablePtr/Read/Reset/ReserveMore/... ...

  void Swap(IOBuffer* other);

 private:          // ← private
  char* storage_;
  int write_idx_;
  int read_idx_;
  int storage_size_;

 public:           // ← 第二个 public！move 构造/赋值放在 private 后面
  IOBuffer(IOBuffer&& rvalue) { ... }
  void operator=(IOBuffer&& rvalue) { ... }  // ← 返回 void 而非 IOBuffer&
};
```

**问题清单**：
1. **两个 `public` 段**，move 操作被隔离在 private 之后，代码结构混乱
2. `operator=(IOBuffer&&)` 返回 `void`，违反 C++ 惯例（应返回 `IOBuffer&`）
3. 手动管理 `char* storage_`（裸 `new[]`/`delete[]`），且 `operator=` 中写的是 `delete storage_`（应为 `delete[] storage_`）—— **是未定义行为**
4. move 构造/赋值未标记 `noexcept`
5. `int` 用于索引和大小，应使用 `size_t`
6. 与 `bufio::Reader` 功能高度重叠（都有 `read_idx_`/`write_idx_`/`storage_`/`buffered()`/`free()`/`empty()`/`full()`）

**建议**：
- 用 `std::vector<char>` 替换 `char* storage_`
- move 操作返回 `IOBuffer&` 并标记 `noexcept`
- 合并 `public` 段
- 或者直接删除 `IOBuffer`，统一使用 `bufio::Reader`（见 2.2）

### 2.2 `BufferedReader` 与 `bufio::Reader` 职责重叠

**严重度**：🔴 高

项目中存在两个缓冲读取器实现：

| 类 | 文件 | 基类 | 功能 |
|---|---|---|---|
| `BufferedReader` | `include/tin/bufio/buffered_reader.h` | 无 | `ReadFull` + `IOBuffer` |
| `bufio::Reader` | `include/tin/bufio/bufio.h` | `tin::io::Reader` | `Read` + `ReadSlice` + `ReadLine` + `ReadByte` + `Peek` |

两者都管理一块 `storage_` + `read_idx_` + `write_idx_`，但接口完全不同。`BufferedReader` 不继承 `io::Reader`，无法与 `io::ReadFull` 等泛型函数配合使用。

**建议**：
- 直接删掉  `BufferedReader` 和 `IOBuffer`， buffered_reader.cc buffered_reader.h

### 2.3 `Channel` 与 `Queue` 设计重复

**严重度**：🟡 中

| 类 | 底层机制 | 实现方式 |
|---|---|---|
| `Channel<T>` | `runtime::SemAcquire`/`SemRelease` + `RawMutex` | 信号量计数 |
| `QueueImpl<T>` | `Cond` + `Mutex` | 条件变量等待 |

两者都是线程安全的 FIFO 队列，都有 `Push`/`Pop`/`Close`，但底层实现完全不同。`Channel` 用信号量，`QueueImpl` 用条件变量。`Channel` 还继承了 `enable_shared_from_this` 但未使用。

**设计问题**：
- `Channel` 和 `Queue` 没有共同接口，无法互换
- `Channel::Push`/`Pop` 方法名与 `QueueImpl::Enqueue`/`Dequeue` 不统一
- `Chan<T>` 和 `Queue<T>` 都是 `shared_ptr` 包装器，但 `Chan` 用 `operator->` 暴露内部，`Queue` 也是——这种模式让用户可以直接访问内部方法，破坏封装

**建议**：
-删掉queue.cc queue.h

### 2.4 `Greenlet` 回调字段冗余

**严重度**：🟡 中

```cpp
// tin/runtime/greenlet.h
class Greenlet {
  std::function<void()>  cb_;        // ← 字段 1
  GreenletFunc entry_;               // ← 字段 2 (void* (*)(intptr_t))
  std::function<void()> closure_;    // ← 字段 3
  intptr_t args_;
  void* retval_;
};
```

三个回调相关字段（`cb_`、`entry_`、`closure_`），职责不清。`entry_` 是 C 风格函数指针（zcontext ABI 要求），`closure_` 是用户闭包，`cb_` 用途不明。`retval_` 在协程返回 `void` 的模型中似乎无用。

**建议**：
- 明确字段语义，先不做任何删除
- 添加注释说明 `entry_` 是 zcontext 入口（不可改）、`closure_` 是用户闭包

### 2.5 `Sudog` 结构体全公开

**严重度**：🟡 中

```cpp
// tin/runtime/semaphore.h
struct Sudog {
  G* gp;
  uint32_t * selectdone;  // ← 指针后多余空格
  Sudog* next;
  Sudog* prev;
  void* elem;
  int32_t nrelease;
  Sudog* waitlink;
  uint32_t* address;
  uint32_t  wakedup;      // ← uint32_t 后多余空格

  Sudog() { ... }         // ← 构造函数手动置空，应用成员初始化列表
};
```

**问题**：
1. 所有成员公开（`struct` 默认），无封装
2. 命名不一致：`selectdone`（无下划线后缀）vs `wakedup`（无下划线后缀），而 Google Style 要求成员变量加后缀 `_`
3. 手动构造函数初始化，应使用成员默认初始化器
4. `selectdone` 应为 `select_done_` 或 `select_done`

**建议**： 先不做任何改动
```cpp
class Sudog {
 public:
  G* gp = nullptr;
  uint32_t* select_done = nullptr;
  Sudog* next = nullptr;
  // ...
};
```

### 2.6 `M` 类中的 `AliasM` 别名无意义

**严重度**：🟢 低

```cpp
// tin/runtime/m.h
using AliasM = M;  // ← 什么用途？

// tin/runtime/p.h
using AliasP = P;  // ← 同上
```

这些别名没有任何使用场景，应删除。

### 2.7 `Work` / `GletWork` 层次保留旧 errno 模型

**严重度**：🟡 中

```cpp
// tin/runtime/threadpoll.h
class GletWork : public Work {
  int LastError() const { return last_error_; }
  void SaveLastError(int err) { last_error_ = err; }
 private:
  int last_error_;
  G* gp_;
};
```

`GletWork` 仍然通过 `LastError`/`SaveLastError` 传递错误码，这是旧的 errno 模式。新代码应返回 `Status` 或 `Result<T>`。

**建议**：长期将 `GletWork` 的错误传递改为 `Status`，但这是内部实现，优先级可降低。

---

## 三、正交设计与职责分离

### 3.1 生命周期 API 双轨并存

**严重度**：🟡 中

`include/tin/tin.h` 同时暴露了两套生命周期 API：

```cpp
// 自由函数 API（手动管理）
void Initialize();
void PowerOn(EntryFn fn, int argc, char** argv, Config* new_conf);
int WaitForPowerOff();
void Deinitialize();

// RAII API（自动管理）
class Runtime {
 public:
  Runtime();
  ~Runtime();
  int Run(EntryFn entry, int argc, char** argv);
};
```

`examples/echo/echo.cc` 使用的是自由函数 API（手动 4 步），而 `Runtime` RAII 类只需 1 步。两套 API 并存会让用户困惑应该用哪个。

**建议**：
- 标记自由函数 API 为 `[[deprecated("Use Runtime class")]]`
- 在文档中推荐 `Runtime` RAII 类
- echo 示例改用 `Runtime` 类

### 3.2 `Spawn` 声明重复

**严重度**：🟡 中

`Spawn` 及其辅助函数在两处重复声明：

| 位置 | 文件 |
|---|---|
| 公共 API | `include/tin/runtime.h` |
| 内部头 | `tin/runtime/spawn.h` |

两者代码**完全相同**：
```cpp
template <typename Functor, typename... Args>
void Spawn(Functor functor, Args&&... args) { ... }
```

**建议**：`tin/runtime/spawn.h` 删除重复声明，改为 `#include "tin/runtime.h"`，或反过来——公共头 `include/tin/runtime.h` include 内部 `spawn.h`（但会破坏 PIMPL）。最佳方案是只在 `include/tin/runtime.h` 中声明，内部代码也使用公共头。

### 3.3 时间头文件分裂

**严重度**：🟢 低

```cpp
// include/tin/time.h      ← 声明 Now(), MonoNow(), Sleep() 等函数
// include/tin/time/time.h  ← 仅定义 kNanosecond, kMicrosecond 等常量
```

用户需要同时 include 两个头文件才能使用时间功能。`time.h` include `time/time.h`，但这层间接增加了认知负担。

**建议**：合并为单个 `include/tin/time.h`，常量和函数声明放在一起。

### 3.4 `config.h` 与 `config/config.h` 分裂

**严重度**：🟢 低

```cpp
// include/tin/config.h       ← 仅 include config/default.h + config/config.h
// include/tin/config/config.h ← Config 类定义
// include/tin/config/default.h ← 常量定义
```

`config.h` 是一个纯转发头文件，多了一层间接。且 `config/config.h` 中同时有 `#ifndef` + `#pragma once`。

**建议**：合并为 `include/tin/config.h`，直接包含 `Config` 类和默认常量。

### 3.5 `tcp.h` 聚合头文件

**严重度**：🟢 低

```cpp
// include/tin/net/tcp.h — 聚合头
#include "tin/net/tcp_conn.h"
#include "tin/net/listener.h"
#include "tin/net/dialer.h"
#include "tin/net/resolve.h"
```

Google Style 不鼓励 "omnibus" 头文件（P3-4 已删除 `all.h`）。`tcp.h` 作为模块聚合头可以保留，但建议在文档中注明它是便利头，非必需。

---

## 四、内存安全与资源管理

### 4.1 `IOBuffer::operator=` 中 `delete` 而非 `delete[]`

**严重度**：🔴 高（未定义行为）

```cpp
// include/tin/io/io_buffer.h:88
void operator=(IOBuffer&& rvalue) {
  delete storage_;     // ← BUG: 应为 delete[] storage_
  storage_ = rvalue.storage_;
  // ...
}
```

`storage_` 通过 `new char[size]` 分配，必须用 `delete[]` 释放。当前代码是**未定义行为**。

### 4.2 `tin.cc` 中 `Config*` 裸指针手动管理

**严重度**：🟡 中

```cpp
// tin/tin.cc
namespace {
Config* conf = nullptr;  // 文件作用域裸指针
}

void Initialize() {
  conf = new tin::Config;     // 手动 new
  *conf = DefaultConfig();
}

void Deinitialize() {
  delete conf;                // 手动 delete
}
```

如果 `Initialize()` 被调用两次或 `Deinitialize()` 被漏调，会导致泄漏或双重释放。

**建议**：改为 `std::unique_ptr<Config>` 或直接用值语义 `Config conf;`。

### 4.3 `TcpConnImpl` / `TCPListenerImpl` 持有 `NetFD*` 裸指针

**严重度**：🟡 中

```cpp
// tin/net/tcp_conn_impl.h
class TcpConnImpl {
 private:
  NetFD* netfd_;  // 析构函数中 delete netfd_;
};

// tin/net/listener_impl.h
class TCPListenerImpl {
 private:
  NetFD* netfd_;  // 析构函数中 delete netfd_;
};
```

两个 Impl 类都通过裸指针 + 析构函数 `delete` 管理 `NetFD` 生命周期。虽然 `NetFD` 是多态类型（继承自 `NetFDCommon`），但 `std::unique_ptr<NetFD>` 完全适用（只要 `NetFD` 有虚析构函数）。

**建议**：改为 `std::unique_ptr<NetFD> netfd_`，消除手动 `delete`。

### 4.4 `Greenlet::name_` 固定长度 C 字符串

**严重度**：🟡 中

```cpp
// tin/runtime/greenlet.h
char name_[32];  // 固定 32 字节
```

用固定 32 字节 `char[]` 存储名称，容易截断、不安全。

**建议**：改为 `std::string name_;` 或 `absl::.fixed_array<char>`（如果对内存布局有严格要求）。

### 4.5 `PollDescriptor` 手动引用计数

**严重度**：🟢 低（受 lock-free 约束不改）

```cpp
// tin/runtime/net/poll_descriptor.h
struct PollDescriptor : public RefCountedThreadSafe {
  // 手动 AddRef/Release
};

inline PollDescriptor* NewPollDescriptor() {
  PollDescriptor* descriptor = new PollDescriptor;
  descriptor->AddRef();
  return descriptor;
}
```

手动 `new` + `AddRef()` 模式容易忘记调用 `Release()`。但根据项目约束，`PollDescriptor` 的裸指针设计是 lock-free 算法要求的，不改。

---

## 五、现代 C++ 实践

### 5.1 C 头文件包含方式

**严重度**：🟡 中

多处使用 C 风格头文件而非 C++ 风格：

| 当前 | 应改为 | 位置 |
|---|---|---|
| `#include <stdlib.h>` | `#include <cstdlib>` | `mutex.h`, `wait_group.h`, `cond.h`, `unlock.h`, `greenlet.h`, `spawn.h`, `atomic_flag.h`, `platform.h`, `unique_id.h` |
| `#include <stdint.h>` | `#include <cstdint>` | `time/time.h`, `config/default.h`, `rwmutex.h` |
| `#include <stddef.h>` | `#include <cstddef>` | `ip_address.h` |
| `#include <semaphore>` | C++20 `<semaphore>` | `m.h`（合理） |

Google Style 要求 C++ 源文件中使用 `<cstdlib>` 而非 `<stdlib.h>`。

### 5.2 `throw(const char*)` 抛出原始字符串

**严重度**：🟡 中

```cpp
// tin/runtime/runtime.cc:59
void Throw(const char* str) {
  LOG(FATAL) << str;
  throw(str);  // ← 抛出 const char*，且 LOG(FATAL) 已是 [[noreturn]]
}

void Panic(const char* str) {
  throw(str);  // ← 抛出 const char*
}
```

**问题**：
1. `LOG(FATAL)` 在 abseil 中是 `[[noreturn]]`，`throw` 是死代码
2. 抛出 `const char*` 而非自定义异常类，catch 端需要 `catch(const char*)`，不安全
3. `Panic` 没有日志记录，直接抛异常

**建议**：
```cpp
class PanicException : public std::runtime_error {
 public:
  explicit PanicException(const std::string& msg) : std::runtime_error(msg) {}
};

[[noreturn]] void Throw(const char* str) {
  LOG(FATAL) << str;  // 已是 noreturn
}

[[noreturn]] void Panic(const char* str) {
  std::string msg = str ? str : "panic";
  LOG(ERROR) << "panic: " << msg;
  throw PanicException(msg);
}
```

### 5.3 `memset` 替换为值初始化

**严重度**：🟢 低

```cpp
// tin/runtime/env.cc:113
struct sigaction sigpipe_action;
memset(&sigpipe_action, 0, sizeof(sigpipe_action));
```

**建议**：
```cpp
struct sigaction sigpipe_action{};  // 值初始化
```

### 5.4 `Config` 成员未初始化

**严重度**：🟡 中

```cpp
// include/tin/config/config.h
class Config {
 private:
  int max_procs_;           // ← 无默认值
  int max_machine_;         // ← 无默认值
  int stack_size_;          // ← 无默认值
  int os_thread_stack_size_;
  bool ignore_sigpipe_;
  bool enable_stack_protection_;
};
```

`Config` 没有构造函数初始化成员。默认构造的 `Config` 对象成员值未定义。`DefaultConfig()` 函数手动设置每个字段，但如果用户直接 `Config conf;` 就会得到未初始化值。

**建议**：使用默认成员初始化器：
```cpp
class Config {
 private:
  int max_procs_ = 1;
  int max_machine_ = 4;
  int stack_size_ = kDefaultStackSize;
  int os_thread_stack_size_ = kDefaultOSThreadStackSize;
  bool ignore_sigpipe_ = true;
  bool enable_stack_protection_ = false;
};
```

### 5.5 `tin::atomic` 命名不一致

**严重度**：🟡 中

```cpp
// 同一命名空间内：
inline int32_t relaxed_Inc32(...)  // ← 大写 I（驼峰式）
inline int32_t Inc32(...)          // ← 大写 I
inline void relaxed_store32(...)   // ← 小写 s（蛇形）
inline void store32(...)           // ← 小写 s
inline bool acquire_cas32(...)     // ← 小写 c
```

`Inc32` 用驼峰，`store32`/`cas32` 用蛇形。应统一为蛇形：`relaxed_inc32`。

### 5.6 `TIN_ERRNO_MAX` 值错误

**严重度**：🟡 中

```cpp
// include/tin/error/error.h:101
enum tin_errno_t {
  // ...
  TIN_ERRNO_MAX = TIN__EOF - 1  // TIN__EOF = -6000, 所以 MAX = -6001
};
```

`TIN_ERRNO_MAX` 的值是 `-6001`，作为 "最大值" 语义上不正确。

---

## 六、API 设计与接口边界

### 6.1 `TcpConn::SetDeadline` 参数语义不明

**严重度**：🟡 中

```cpp
// include/tin/net/tcp_conn.h
void SetDeadline(int64_t t);
void SetReadDeadline(int64_t t);
void SetWriteDeadline(int64_t t);
```

`t` 是什么？绝对时间戳？相对超时？纳秒？毫秒？公共 API 无任何文档说明。从 `echo.cc` 的用法 `conn.SetDeadline(kRWDeadline)`（`kRWDeadline = 20 * tin::kSecond`）来看，似乎是**相对超时（纳秒）**，但 `dialer.cc` 中 `deadline` 被当作绝对值与 `UINT64_MAX` 比较。

**建议**：
- 明确文档：`// t: absolute deadline in nanoseconds since epoch (0 = no deadline)`
- 或引入 `Deadline` 类型别名：`using Deadline = int64_t; // nanoseconds since epoch`

### 6.2 `IPEndPoint` 在公共 API 中泄露 `socklen_t`

**严重度**：🟡 中

```cpp
// include/tin/net/ip_endpoint.h
bool ToSockAddr(struct sockaddr* address, socklen_t* address_length) const;
bool FromSockAddr(const struct sockaddr* address, socklen_t address_length);
```

`TcpConn` 的 `GetSockOpt`/`SetSockOpt` 已将 `socklen_t` 替换为 `int` 避免泄露平台头文件，但 `IPEndPoint` 仍然直接使用 `socklen_t`。且该文件 `#include "tin/net/sys_addrinfo.h"` 会拉入平台 socket 头文件。

**建议**：`IPEndPoint` 也应 PIMPL 化或将 `socklen_t` 改为 `int`。

### 6.3 `MakeTcpConn` 工厂函数泄露 `NetFD`

**严重度**：🟡 中

```cpp
// include/tin/net/tcp_conn.h
TcpConn MakeTcpConn(class NetFD* netfd);  // ← 前向声明 NetFD 在公共头中
```

虽然用了前向声明（不泄露 `NetFD` 定义），但 `NetFD` 是内部类型，不应出现在公共 API 签名中。`MakeTcpConn` 仅被 `dialer.cc` 和 `listener.cc` 调用，应移至内部头文件。

**建议**：将 `MakeTcpConn` 声明移至 `tin/net/tcp_conn_impl.h`，公共头不暴露。

### 6.4 `IPAddress` 构造函数过多

**严重度**：🟢 低

```cpp
class IPAddress {
  IPAddress();
  explicit IPAddress(const std::vector<uint8_t>& address);
  IPAddress(const IPAddress& other);
  template <size_t N> IPAddress(const uint8_t(&address)[N]);
  IPAddress(const uint8_t* address, size_t address_len);
  IPAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);
  IPAddress(uint8_t b0, ..., uint8_t b15);  // 16 参数
};
```

7 个构造函数，包括一个 16 参数的。这违反了"构造函数不要太多"的设计原则。

**建议**：保留必要构造函数，用工厂方法替代：
```cpp
static IPAddress FromIPv4(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);
static IPAddress FromIPv6(uint8_t const (&bytes)[16]);
static IPAddress FromBytes(const uint8_t* data, size_t len);
```

### 6.5 `QueueImpl` 暴露内部锁

**严重度**：🟡 中

```cpp
// include/tin/communication/queue.h
class QueueImpl {
 public:
  // intrusive methods, be careful!
  void Lock() const;
  void UnLock() const;                    // ← 大写 L，不一致
  bool IsClosedLocked() const;
  std::deque<T>* MutableQueueLocked();
  void NotifyConsumedLocked(int n);
};
```

这些 "intrusive" 方法暴露了内部锁和队列，破坏封装。且 `UnLock` 的大小写不一致（应为 `Unlock`）。

**建议**：如果确实需要外部加锁访问，提供明确的迭代器接口或访问器，而非暴露 `std::deque*`。

---

## 七、错误处理体系

### 7.1 三套错误模型并存

**严重度**：🔴 高

项目中同时存在三套错误处理模型：

| 模型 | 位置 | 用法 |
|---|---|---|
| **新模型：`Status`/`Result<T>`** | `include/tin/status.h`, `result.h` | `Result<size_t> Read(...)` |
| **旧模型：per-greenlet errno** | `tin/runtime/runtime.h` | `SetErrorCode()`, `GetErrorCode()`, `ErrorOccured()` |
| **C 风格：int 返回码** | `NetFD` 内部 | `int err = netfd_->Read(...)` |

公共 API（`TcpConn`, `TCPListener`, `Dialer`）已使用 `Status`/`Result`，但内部层（`NetFD`, `Env`, `TimerQueue`）仍用 `int err` 返回码，`runtime.h` 仍声明 errno 函数。

**建议**：
- 标记 `SetErrorCode`/`GetErrorCode`/`ErrorOccured` 为 `[[deprecated]]`
- 内部层逐步迁移到 `Status` 返回值
- 最终统一为 `Status`/`Result<T>` 模型

### 7.2 `Status::ToString()` 与 `Status::ErrorName()` 实现相同

**严重度**：🟡 中

```cpp
// tin/status.cc
std::string Status::ToString() const {
  if (ok()) return "OK";
  return TinErrorName(code_);
}

std::string Status::ErrorName() const {
  if (ok()) return "OK";
  return TinErrorName(code_);  // ← 与 ToString 完全相同
}
```

两个方法实现完全一样。`ToString()` 应返回更详细的信息（包含错误描述），`ErrorName()` 只返回错误名。

**建议**：`ToString()` 应格式化为 `"ErrorName: description"`，利用 `TIN_ERRNO_MAP` 中的描述字符串。

### 7.3 `SysErrorTranslator` RAII 析构转换

**严重度**：🟢 低

```cpp
// include/tin/error/error.h
class SysErrorTranslator {
 public:
  explicit SysErrorTranslator(int* error_code) : error_code_(error_code) {}
  ~SysErrorTranslator() {
    *error_code_ = TinTranslateSysError(*error_code_);
  }
 private:
  int* error_code_;
};
```

这个 RAII 类在析构时修改 `int*` 指向的值。虽然巧妙，但语义不直观——用户需要知道它在析构时做转换。

**建议**：添加文档注释，或改为显式函数调用 `TranslateSysError(&err)`。

---

## 八、并发与同步原语

### 8.1 `Channel::IsClosed()` 缺少 `const` 限定

**严重度**：🟡 中

```cpp
// include/tin/communication/chan.h
bool IsClosed() {  // ← 非 const
  return atomic::acquire_load32(&closed_) != 0;
}
```

`IsClosed()` 是只读查询，应标记 `const`。但 `atomic::acquire_load32` 接受 `const volatile uint32_t*`，`closed_` 成员需要标记 `mutable` 或 `volatile`。

**建议**：
```cpp
bool IsClosed() const {
  return atomic::acquire_load32(&closed_) != 0;
}
// 成员: mutable volatile uint32_t closed_;
```

### 8.2 `Cond::waiters_` 使用 `tin::atomic` 而非 `std::atomic`

**严重度**：🟢 低

```cpp
// include/tin/sync/cond.h
class Cond {
  uint32_t waiters_;  // ← 通过 tin::atomic:: 操作
};

// cond.cc
void Cond::Wait() {
  atomic::Inc32(&waiters_, 1);  // ← tin::atomic 包装
  // ...
}
```

而 `WaitGroup` 使用 `std::atomic<uint64_t>`，`FdMutex` 使用 `std::atomic<uint64_t>`。同一项目中原子操作风格不统一。

**建议**：`Cond::waiters_` 改为 `std::atomic<uint32_t>`，直接使用 `fetch_add`/`load`/`compare_exchange`。

### 8.3 `QueueImpl::Size()` / `Empty()` 加锁但暗示轻量

**严重度**：🟢 低

```cpp
size_t Size() const {
  MutexGuard guard(&lock_);  // ← 加锁
  if (closed_) return 0;
  return queue_.size();
}

bool Empty() const {
  MutexGuard guard(&lock_);  // ← 加锁
  // ...
}
```

`Size()` 和 `Empty()` 会加锁阻塞，但方法名暗示它们是轻量查询。用户可能在高频路径调用而不自知。

**建议**：添加文档注释 `// This method acquires the internal lock. O(1) but may block.`，或提供 `SizeApprox()` 无锁版本。

### 8.4 `RawMutex` / `Note` 成员命名不加 `_` 后缀

**严重度**：🟢 低

```cpp
// tin/runtime/raw_mutex.h
class RawMutex {
  uintptr_t key;    // ← 无 _ 后缀
  M* owner_;
};

class Note {
  uintptr_t key;    // ← 无 _ 后缀
};
```

Google Style 要求类成员变量加 `_` 后缀（或前缀，但项目已统一用后缀）。`key` 应为 `key_`。

同样，`PollDescriptor` 的成员 `fd`、`closing`、`seq`、`rg`、`wg`、`user` 均缺少 `_` 后缀（但受 lock-free 约束可能不改）。

---

## 九、头文件依赖与编译隔离

### 9.1 公共头 `chan.h` / `queue.h` 依赖 runtime 内部

**严重度**：🔴 高

```cpp
// include/tin/communication/chan.h
#include "tin/sync/atomic.h"
#include "tin/runtime/raw_mutex.h"    // ← runtime 内部！
#include "tin/runtime/semaphore.h"    // ← runtime 内部！
```

P2-1 的目标是公共头不拉入 `runtime/` 头文件，但 `chan.h` 和 `queue.h` 仍然直接依赖 `runtime/raw_mutex.h` 和 `runtime/semaphore.h`。这意味着用户 `#include "tin/communication/chan.h"` 会拉入整个 runtime 内部实现。

**建议**：对 `Channel` / `QueueImpl` 进行 PIMPL 化，将 `RawMutex` 和 `SemAcquire`/`SemRelease` 的使用隐藏在 `.cc` 文件中。

### 9.2 `ip_endpoint.h` 依赖平台 socket 头

**严重度**：🟡 中

```cpp
// include/tin/net/ip_endpoint.h
#include "tin/net/sys_addrinfo.h"  // ← 拉入 <sys/socket.h> 或 <winsock2.h>
struct sockaddr;  // 前向声明
```

公共头文件拉入平台 socket 头，违背了 P2-1 的编译隔离目标。

**建议**：PIMPL 化 `IPEndPoint`，或移除 `ToSockAddr`/`FromSockAddr` 到内部工具函数。

### 9.3 `atomic.h` 435 行在公共头中

**严重度**：🟡 中

`include/tin/sync/atomic.h` 有 435 行包装代码，作为公共头文件会显著影响编译速度。虽然它现在已改为 `std::atomic` 的薄包装，但庞大的函数体仍在头文件中。

**建议**：长期目标是移除 `tin::atomic` 命名空间，直接使用 `std::atomic`（P1-4 第二阶段）。短期可将非内联部分移到 `.cc`。

---

## 十、问题汇总优先级表

### 🔴 高优先级（正确性问题 / 未定义行为）

| 编号 | 问题 | 位置 | 修复难度 |
|---|---|---|---|
| 4.1 | `IOBuffer::operator=` 用 `delete` 而非 `delete[]` | `io/io_buffer.h:88` | 极低 |
| 1.5 | `CACHELINE_SIZE` 宏带尾分号 | `config/default.h:15` | 极低 |
| 2.1 | `IOBuffer` 整体设计混乱（双 public 段、move 返回 void） | `io/io_buffer.h` | 中 |
| 2.2 | `BufferedReader` 与 `bufio::Reader` 职责重叠 | `bufio/` | 中 |
| 7.1 | 三套错误模型并存 | 全局 | 高 |
| 9.1 | 公共头 `chan.h`/`queue.h` 依赖 runtime 内部 | `communication/` | 中 |

### 🟡 中优先级（设计改进 / 一致性）

| 编号 | 问题 | 位置 | 修复难度 |
|---|---|---|---|
| 1.1 | `#pragma once` 与 `#ifndef` 混用 | 全局 | 低 |
| 1.2 | 命名空间风格混用 | 全局 | 低 |
| 1.3 | 类名大小写不统一（`TCPListener` vs `TcpConn`） | `net/` | 中（API 变更） |
| 1.4 | 拼写错误残留（5 处） | 多处 | 低 |
| 2.3 | `Channel` 与 `Queue` 设计重复 | `communication/` | 中 |
| 2.4 | `Greenlet` 回调字段冗余 | `runtime/greenlet.h` | 低 |
| 2.5 | `Sudog` 全公开 + 命名不一致 | `runtime/semaphore.h` | 低 |
| 2.7 | `GletWork` 保留旧 errno 模型 | `runtime/threadpoll.h` | 中 |
| 3.1 | 生命周期 API 双轨并存 | `tin.h` | 低 |
| 3.2 | `Spawn` 声明重复 | `runtime.h` / `spawn.h` | 低 |
| 4.2 | `tin.cc` 中 `Config*` 裸指针 | `tin.cc` | 低 |
| 4.3 | `TcpConnImpl`/`TCPListenerImpl` 持有裸指针 | `net/` | 低 |
| 4.4 | `Greenlet::name_` 固定长度 C 字符串 | `runtime/greenlet.h` | 低 |
| 5.1 | C 头文件包含方式（`<stdlib.h>` → `<cstdlib>`） | 多处 | 低 |
| 5.2 | `throw(const char*)` 不安全 | `runtime/runtime.cc` | 低 |
| 5.4 | `Config` 成员未初始化 | `config/config.h` | 低 |
| 5.5 | `tin::atomic` 命名不一致（`Inc32` vs `store32`） | `sync/atomic.h` | 低 |
| 5.6 | `TIN_ERRNO_MAX` 值错误 | `error/error.h:101` | 低 |
| 6.1 | `SetDeadline` 参数语义不明 | `net/tcp_conn.h` | 低 |
| 6.2 | `IPEndPoint` 泄露 `socklen_t` | `net/ip_endpoint.h` | 中 |
| 6.3 | `MakeTcpConn` 泄露 `NetFD` | `net/tcp_conn.h` | 低 |
| 6.5 | `QueueImpl` 暴露内部锁 | `communication/queue.h` | 中 |
| 7.2 | `Status::ToString()` 与 `ErrorName()` 相同 | `status.cc` | 低 |
| 8.1 | `Channel::IsClosed()` 缺少 `const` | `communication/chan.h` | 低 |
| 8.2 | `Cond::waiters_` 原子操作风格不统一 | `sync/cond.h` | 低 |
| 9.2 | `ip_endpoint.h` 依赖平台 socket 头 | `net/ip_endpoint.h` | 中 |
| 9.3 | `atomic.h` 435 行在公共头中 | `sync/atomic.h` | 中 |

### 🟢 低优先级（风格优化）

| 编号 | 问题 | 位置 |
|---|---|---|
| 1.6 | 文件名与类名映射不一致 | `net/` |
| 2.6 | `AliasM` / `AliasP` 无意义别名 | `runtime/m.h`, `p.h` |
| 3.3 | 时间头文件分裂 | `time.h` / `time/time.h` |
| 3.4 | `config.h` 与 `config/config.h` 分裂 | `config/` |
| 3.5 | `tcp.h` 聚合头文件 | `net/tcp.h` |
| 5.3 | `memset` 替换为值初始化 | `runtime/env.cc` |
| 6.4 | `IPAddress` 构造函数过多 | `net/ip_address.h` |
| 7.3 | `SysErrorTranslator` 语义不直观 | `error/error.h` |
| 8.3 | `Size()`/`Empty()` 加锁但暗示轻量 | `communication/queue.h` |
| 8.4 | `RawMutex`/`Note` 成员命名无 `_` 后缀 | `runtime/raw_mutex.h` |

---

## 附录：建议的修复顺序

```
阶段 1（立即）：4.1, 1.5 — 修复 UB 和编译错误
阶段 2（低成本批量）：1.1, 1.2, 1.4, 5.1, 5.5, 8.4 — 风格统一
阶段 3（中等成本）：2.1, 2.2, 4.2, 4.3, 5.2, 5.4, 7.2 — 设计改进
阶段 4（较高成本）：2.3, 7.1, 9.1 — 架构级重构
阶段 5（长期）：1.3, 6.2, 9.2 — 公共 API 变更
```
