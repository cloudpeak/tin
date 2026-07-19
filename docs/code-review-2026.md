# tin 项目代码评审报告

> 评审基准：Google C++ Style Guide (最新版) + LevelDB 工程实践
> 评审范围：`tin/` 源码树（不含 `third_party/`）
> 日期：2026-07-19
> 背景：tin 是模仿 Go 1.6 runtime 的 C++ 协程并发框架

---

## 目录

1. [总体评价](#一总体评价)
2. [错误处理：全局 errno 模式](#二错误处理全局-errno-模式最严重的架构硬伤)
3. [内存管理与所有权](#三内存管理与所有权)
4. [类型安全与 C 风格残留](#四类型安全与-c-风格残留)
5. [命名与编码风格](#五命名与编码风格)
6. [头文件与模块边界](#六头文件与模块边界)
7. [并发原语设计](#七并发原语设计)
8. [接口设计](#八接口设计)
9. [代码结构组织](#九代码结构组织)
10. [测试与可维护性](#十测试与可维护性)
11. [与 LevelDB 的差距总结](#十一与-leveldb-的差距总结)

---

## 一、总体评价

tin 成功地将 Go 1.6 的 GMP 调度器、netpoller、channel 等核心机制移植到 C++，工程量可观。但从工程质量角度看，它更像是"Go runtime 的 C 翻译"而非"用 C++ 最佳实践重新设计的并发框架"。与 LevelDB 这类标杆项目相比，tin 在**错误处理、类型安全、所有权语义、模块边界、测试覆盖**五个维度存在系统性差距。

**核心矛盾**：tin 把 Go 的设计（全局状态、errno 模式、interface duck-typing）原封不动搬进 C++，但这些模式在 C++ 中是反模式。Go 有 GC 和 runtime 检查兜底，C++ 没有。

---

## 二、错误处理：全局 errno 模式（最严重的架构硬伤）

### 2.1 问题

tin 的错误处理完全模仿 Go 的 `errno` 模式，但把它绑定到了**协程级全局状态**：

```cpp
// tin/error.h — 用户侧 API
void SetErrorCode(int error_code);
int GetErrorCode();
bool ErrorOccured();
const char* GetErrorStr();
```

实现是每个 greenlet 存一个 `int error_code_`：

```cpp
// tin/runtime/runtime.cc
void SetErrorCode(int error_code) { runtime::GetG()->SetErrorCode(error_code); }
int  GetErrorCode()               { return runtime::GetG()->GetErrorCode(); }
```

调用者必须遵循**隐式协议**：每次调用 IO 函数后，先检查返回值，再调 `GetErrorCode()`：

```cpp
// examples/echo/echo.cc
int n = conn->Read(buf.get(), kIOBufferSize);
int err = tin::GetErrorCode();   // <-- 隐式契约：必须紧接着调用
if (err != 0) { ... }
```

### 2.2 为什么这是硬伤

| 问题 | 说明 |
|---|---|
| **返回值与错误码分离** | `Read()` 返回 `int`（字节数），错误码在另一处获取。调用者很容易忘记检查 `GetErrorCode()`，或在不该读的时候误读上一次的错误码 |
| **错误码是可变的全局状态** | `GetErrorCode()` 读取当前 greenlet 的 error_code。任何中间调用（包括日志、Spawn 内部）都可能覆盖它。一旦在 `Read` 和 `GetErrorCode` 之间插入了任何可能设置 error_code 的操作，错误就丢失了 |
| **无法表达"成功但部分读取"** | `Read` 返回 `n > 0` 但同时也可能设置了错误码（`TIN_EOF` + 部分数据）。用户必须同时检查返回值和 errno，心智负担极高 |
| **不可组合** | 一个函数调了两个 IO 操作，第二个失败覆盖第一个的错误码，调用者永远拿不到第一个 |
| **线程/greenlet 切换风险** | 如果 `GetErrorCode()` 在 greenlet 切换之后调用，读到的是切换后的 greenlet 的 error_code |

### 2.3 LevelDB 怎么做

LevelDB 用 `Status` 对象，**值语义、不可变、显式传递**：

```cpp
// leveldb
Status s = db->Get(ReadOptions(), key, &value);
if (!s.ok()) { /* s.ToString(), s.IsNotFound(), ... */ }
```

- 错误和操作绑定在同一个返回值里，不可能"忘记检查"
- `Status` 是值类型，拷贝安全，不会互相覆盖
- 可以精确区分 `NotFound` / `Corruption` / `IOError` / `NotSupported`

### 2.4 建议方向

引入 `tin::Result<T>` 或 `tin::Status` 类型，逐步替代 errno 模式：

```cpp
// 理想 API
Result<size_t> result = conn->Read(buf, kIOBufferSize);
if (!result.ok()) {
    LOG(ERROR) << result.error().ToString();
    // 或者: result.status().IsTimeout(), IsEOF(), ...
}
size_t n = result.value();
```

这是**破坏性变更**，但可以保留旧 API 作为 deprecated 过渡。

---

## 三、内存管理与所有权

### 3.1 裸 `new`/`delete` 泛滥，无 `unique_ptr`

运行时核心对象全部用裸 `new` 创建、裸 `delete` 销毁：

```cpp
// tin/runtime/env.cc
sched = new Scheduler;
timer_q = new TimerQueue;
glet_tls = new Greenlet;
// ...
void Env::Deinitialize() {
    delete timer_q;   // 只删了 timer_q，sched 和 glet_tls 泄漏
}
```

`Env::Deinitialize()` 只 `delete timer_q`，**`sched` 和 `glet_tls` 完全没有释放**——这是内存泄漏。

### 3.2 所有权语义不清

```cpp
// tin/net/tcp_conn.h
class TcpConnImpl {
 private:
  NetFD* netfd_;   // 谁拥有 netfd_? 裸指针，不知道
  ...
};
```

`NetFD*` 是裸指针，`TcpConnImpl` 是否拥有它？析构时该不该 `delete`？看代码才知道 `NetFD` 由 `NetFDCommon` 层管理，但类型签名上完全看不出来。

### 3.3 `TcpConn` 使用 `shared_ptr` —— 合理且应保留的设计

```cpp
// tin/net/tcp_conn.h
class TcpConn {
  std::shared_ptr<TcpConnImpl> impl_;
};

class TCPListener {
  std::shared_ptr<TCPListenerImpl> impl_;
};
```

**结论先行**：`TcpConn` 使用 `shared_ptr` 是**合理的设计决策，应当保留**。这与 LevelDB "几乎不用 `shared_ptr`" 的经验并不冲突——两者面对的场景不同。下面详细说明。

#### 3.3.1 为什么 `TcpConn` 的 `shared_ptr` 是恰当的

| 维度 | 说明 |
|---|---|
| **资源权重** | 一条 TCP 连接是重量级资源（涉及 socket fd、内核缓冲区、可能的 TLS 状态、网络往返）。相比连接本身的建立/销毁开销，`shared_ptr` 的一次原子引用计数操作（约 1~5ns）完全可以忽略。为网络连接这种"昂贵且长生命周期"的对象做自动引用计数，是业界通行做法（brpc、muduo、libuv 均如此）。 |
| **真正的共享所有权** | 在协程并发框架中，一条连接天然会被多处持有：accept 循环创建后传入 `Spawn` 的闭包、读协程、写协程、超时定时器回调、连接注册表等。这不是"伪共享"，而是**真实的共享所有权**。`shared_ptr` 精确表达了"最后一个引用释放时关闭连接"的语义，无需手动 `Close()`（虽然 echo 示例显式调了 `Close()`，但注释也指出析构会兜底）。 |
| **值语义便于使用** | `TcpConn` 可拷贝、可按值传递，模仿了 Go 的 `net.Conn`（接口值本身可自由拷贝）。用户写 `void HandleClient(TcpConn conn)` 然后 `Spawn(&HandleClient, conn)` 非常自然，无需思考所有权转移。这是框架易用性的核心——降低用户心智负担。 |
| **`enable_shared_from_this` 已就位** | `TcpConnImpl` 已继承 `std::enable_shared_from_this<TcpConnImpl>`，为内部回调安全地获取 `shared_ptr` 提供了基础，设计上已考虑了共享所有权场景。 |

#### 3.3.2 在保留 `shared_ptr` 前提下的改进建议

虽然 `shared_ptr` 的选择正确，但当前实现仍有可优化之处，**且这些优化不改变 `shared_ptr` 的所有权模型**：

**（1）用 `std::make_shared` 替代 `new` + 构造，消除二次分配**

当前工厂函数接收裸指针，导致 `shared_ptr` 的控制块与对象分离分配：

```cpp
// 现状：两次堆分配（一次 new TcpConnImpl，一次 shared_ptr 控制块）
// tin/net/listener.cc
conn = new TcpConnImpl(newfd);
return MakeTcpConn(conn);   // shared_ptr(T*) 内部再分配控制块

// tin/net/dialer.cc
return MakeTcpConn(new TcpConnImpl(netfd));
```

应改为 `std::make_shared`，将对象与控制块合并为单次分配，既减少分配次数又提升缓存局部性：

```cpp
// 建议：单次分配，且构造期不暴露裸指针
return TcpConn(std::make_shared<TcpConnImpl>(newfd));
```

这需要把 `TcpConn` 的构造函数从 `TcpConn(TcpConnImpl*)` 改为接收 `std::shared_ptr<TcpConnImpl>`，并移除 `MakeTcpConn` 工厂函数（或让其内部调用 `make_shared`）。

**（2）防范循环引用——回调中使用 `weak_ptr` 观察连接**

`enable_shared_from_this` 既是能力也是风险。若 `TcpConnImpl` 内部把 `shared_from_this()` 捕获进定时器、netpoller 回调而不释放，连接将永远无法析构（连接泄漏，fd 泄漏）。审查规则：

- netpoller / timer 注册的回调应捕获 `weak_ptr<TcpConnImpl>`，在回调内 `lock()` 后再操作，使 poller 不延长连接生命周期
- 只有"业务协程闭包"这种应当延长生命周期的场景才持有 `shared_ptr`

```cpp
// 反例：定时器持有 shared_ptr → 连接泄漏
timer_q->AddTimer([self = shared_from_this()]() {
  self->Close();   // 永远不会触发，因为 timer 持有 self
});

// 正例：定时器持有 weak_ptr
timer_q->AddTimer([weak = std::weak_ptr<TcpConnImpl>(shared_from_this())]() {
  if (auto self = weak.lock()) self->Close();
});
```

**（3）连接注册表应使用 `weak_ptr`**

若未来引入连接管理器（如 `ConnectionRegistry`），注册表应存 `weak_ptr<TcpConnImpl>` 或 `unordered_map<fd, weak_ptr<...>>`，使注册表成为"观察者"而非"持有者"，避免注册表阻止空闲连接释放。

**（4）`TCPListener` 可单独评估**

`TCPListener` 通常单实例、单所有者（accept 循环独占），理论上 `unique_ptr` 更贴切。但为了与 `TcpConn` 保持一致的值语义 API（可拷贝、可按值传入 `Spawn`），保留 `shared_ptr` 也可接受。若要严格区分，可让 `TCPListener` 不可拷贝（`unique_ptr` pimpl），强制用户用引用传递——但这会牺牲 API 一致性。建议**保持现状**，一致性优先。

#### 3.3.3 与 LevelDB 经验的辨析

LevelDB 几乎不用 `shared_ptr`，是因为它面对的是**单线程顺序访问的 KV 存储**，对象生命周期在调用栈上清晰可控。tin 面对的是**多协程并发的网络框架**，连接对象横跨多个执行流、多个回调，生命周期无法用调用栈表达。两者场景不同，不能机械套用。结论：`TcpConn` 的 `shared_ptr` **不是缺陷，而是该场景下的正确选择**；真正需要修复的是 3.1 的裸 `new`/`delete` 泄漏和 3.3.2 的 `make_shared` 优化。

### 3.4 `Channel` 对指针类型做 `delete`

```cpp
// tin/communication/chan.h
void ClearQueue(std::deque<T>& queue, std::true_type) {
    for (auto iter = queue.begin(); iter != queue.end(); ++iter) {
        delete *iter;   // Channel 对 T* 类型元素做 delete!
    }
}
```

`Channel<T*>` 在 `Close()` 时会 `delete` 所有元素。这把所有权语义焊死在了类型标签上——如果元素是 `unique_ptr` 管理的、或是栈对象的指针，就是 double-free / use-after-free。

### 3.5 LevelDB 怎么做

- 所有堆对象用 `std::unique_ptr` 持有，所有权清晰
- 几乎不用 `shared_ptr`——但这是因为 LevelDB 是**单线程顺序访问的 KV 存储**，对象生命周期在调用栈上清晰可控
- iterator 不拥有数据，只持裸指针 + 调用者保证生命周期
- 析构函数保证释放所有资源（RAII）

**辨析**：LevelDB "不用 `shared_ptr`" 的经验不能机械照搬到网络框架。tin 的 `TcpConn` 面对多协程并发、跨执行流的共享所有权，`shared_ptr` 是恰当选择（详见 3.3）。tin 真正应向 LevelDB 学习的是：**运行时核心对象（`Scheduler`/`TimerQueue`/`Greenlet`）用 `unique_ptr` 管理以消除裸 `new`/`delete` 泄漏**，而非否定 `TcpConn` 的 `shared_ptr`。

---

## 四、类型安全与 C 风格残留

### 4.1 `uintptr_t` 当万能指针用

这是 tin 从 Go runtime 继承的最糟糕的模式之一：

```cpp
// tin/runtime/guintptr.h — 把指针编码成 uintptr_t 存储和传递
class GUintptr {
  uintptr_t integer_;
};

// tin/runtime/net/netpoll.h
const uintptr_t kPdReady = 1;
const uintptr_t kPdWait = 2;

// tin/runtime/net/netpoll.cc
bool NetPollBlockCommit(void* arg1, void* arg2) {
    uintptr_t gp = reinterpret_cast<uintptr_t>(arg1);
    uintptr_t* gpp = reinterpret_cast<uintptr_t*>(arg2);
    return atomic::release_cas(gpp, kPdWait, gp);
}
```

`PollDescriptor::rg` 和 `wg` 字段是 `uintptr_t`，但实际存储的要么是常量 `kPdReady`/`kPdWait`，要么是 `G*` 指针编码后的值。类型系统完全失明——编译器无法检查你是否把一个 `kPdReady` 当指针 dereference 了。

### 4.2 `void*` + 函数指针模拟闭包

```cpp
// tin/runtime/unlock.h
typedef bool(*UnlockFunc)(void* arg1, void* arg2);

class UnLockInfo {
    UnlockFunc f_;
    void* arg1_;
    void* arg2_;
};

// tin/runtime/scheduler.cc
Park(NetPollBlockCommit, gp, gpp);  // 把 G* 和 uintptr_t* 传进 void*
```

这是 C 风格的回调模式，完全丧失类型安全。`arg1` 和 `arg2` 是 `void*`，传错类型编译器不会报错。C++20 有 `std::function`、lambda、概念（concepts），完全不需要这种模式。

### 4.3 C 风格回调函数

```cpp
// tin/runtime/timer/timer_queue.h
typedef void (*TimerCallback)(void* arg, uintptr_t seq);

// tin/runtime/greenlet.h
typedef void* (*GreenletFunc)(intptr_t);
```

裸函数指针 + `void*`/`intptr_t` 参数，无法捕获状态、无法类型检查。`GreenletFunc` 返回 `void*` 但 `Greenlet::entry_` 的返回值被存在 `void* retval_` 里且从未使用。

### 4.4 `Sudog` 结构体——Go 内部数据结构的直接翻译

```cpp
// tin/runtime/semaphore.h
struct Sudog {
    G* gp;
    uint32_t* selectdone;
    Sudog* next;
    Sudog* prev;
    void* elem;
    int32_t nrelease;
    Sudog* waitlink;
    uint32_t* address;
    uint32_t wakedup;
};
```

`Sudog` 是 Go runtime 内部的等待队列节点，连字段名都原样保留。这是实现细节泄漏——用户不应该看到这种内部结构。而且所有字段 public，没有不变量保护。

### 4.5 拼写错误已经固化到 API

```cpp
// tin/config/config.h
void EnableStackPprotection(bool enable);  // 两个 p: Pprotection

// tin/sync/cond.h
void Broascast();  // Broadcast 拼成 Broascast

// tin/sync/rwmutex.h
uint32_t reader_sem;   // 其他都是 sem_ 后缀，这个没有下划线
```

`EnableStackPprotection` 和 `Broascast` 已经是公开 API，改名需要破坏兼容性。

---

## 五、命名与编码风格

### 5.1 与 Google C++ Style Guide 的偏差

| 规则 | Google Style | tin 现状 | 举例 |
|---|---|---|---|
| 命名空间 | `namespace tin::net {}` 统一 | **混用** `namespace tin { namespace net {} }` 和 `namespace tin::net {}` | `tcp_conn.h` 用旧式，`listener.h` 用新式 |
| 文件名 | `snake_case.cc` / `snake_case.h` | 大部分符合，但有 `NetPoll.h`（PascalCase）和 `netpoll.cc`（snake_case）并存 | `tin/runtime/net/NetPoll.h` vs `netpoll_epoll.cc` |
| 常量 | `kCamelCase` | 符合 | `kSecond`, `kDefaultStackSize` |
| 类型 | `PascalCase` | 符合 | `Greenlet`, `TcpConn` |
| 函数 | `PascalCase`（非访问器）| **混用** PascalCase 和 snake_case | `NetPoll()` vs `find_runnable`（Go 风格） |
| 成员变量 | `snake_case_`（尾下划线） | **大部分不符合**，无尾下划线 | `m.h`: `p_`, `nextp_` 有下划线，但 `timer_queue.h`: `i`, `when`, `period`, `f`, `arg` 全是 Go 风格无下划线 |
| 宏 | `UPPER_SNAKE_CASE` | 符合 | `TIN_EOF`, `ABSL_ARRAYSIZE` |

### 5.2 `typedef` 而非 `using`

```cpp
// tin/runtime/util.h
typedef Greenlet G;                    // 应为: using G = Greenlet;

// tin/runtime/p.h
typedef class P AliasP;               // 应为: using AliasP = P;

// tin/net/ip_address.h
typedef std::vector<IPAddress> IPAddressList;  // 应为: using IPAddressList = ...
```

Google Style Guide 明确推荐 `using` 而非 `typedef`。

### 5.3 `NULL` 而非 `nullptr`

全项目大量使用 `NULL` 而非 C++11 的 `nullptr`：

```cpp
// tin/runtime/env.cc
Env* rtm_env = NULL;
Scheduler* sched = NULL;
TimerQueue* timer_q = NULL;
```

`grep -r "NULL" tin/` 有 40+ 处。`NULL` 在函数重载时可能匹配到 `int` 而非指针，`nullptr` 不会有此问题。

### 5.4 `#pragma once` 与 `#ifndef` 混用

```cpp
// tin/tin.h
#pragma once          // 没有 include guard

// tin/time.h
#ifndef TIN_TIME_H_   // 传统 include guard
#define TIN_TIME_H_

// tin/runtime/raw_mutex.h
#pragma once          // 又用 #pragma once
```

同一个项目混用两种 include guard 机制。Google Style Guide 推荐统一使用 `#ifndef`（可移植性更好），但至少应该**统一**。

### 5.5 注释中的代码残留

```cpp
// tin/runtime/spawn.h — 70 行被注释掉的旧代码
/*
template <typename Functor>
void Spawn(Functor functor) {
  DoSpawn(base::Bind(functor));
}
// ... 还有 6 个被注释掉的重载
*/
```

被注释掉的代码应该删除。版本控制会记住历史。

```cpp
// tin/communication/chan.h
// std::cout << "Channel constructor " << rand() << std::endl;
```

调试代码残留在头文件中，应删除。

---

## 六、头文件与模块边界

### 6.1 实现细节暴露在公共头文件中

用户 `#include "tin/net/tcp_conn.h"` 会被迫看到：

```cpp
// tin/net/tcp_conn.h — 用户头文件
#include "tin/net/sys_socket.h"     // 暴露 winsock2.h / sys/socket.h
#include "tin/time/time.h"
#include "tin/io/io.h"

class TcpConnImpl : public std::enable_shared_from_this<TcpConnImpl>,
                    public tin::io::IOReadWriter {
```

`sys_socket.h` 把 `winsock2.h` 或 `sys/socket.h` 拉进用户的编译单元。用户只是想用 `TcpConn`，不应该被迫处理平台 socket 头文件。

### 6.2 `tin::runtime::` 内部头文件被用户间接引入

```cpp
// tin/sync/atomic_flag.h（公共头文件）
#include "tin/sync/atomic.h"
// tin/sync/atomic.h 虽然 namespace 是 tin::atomic，
// 但 tin/communication/chan.h（公共头）会引入：
#include "tin/runtime/raw_mutex.h"     // 内部头泄漏
#include "tin/runtime/semaphore.h"     // Sudog 结构体暴露
```

用户 `#include "tin/sync/wait_group.h"` 会间接拉入 `tin/runtime/semaphore.h`，看到 `Sudog` 这种 Go runtime 内部结构。

### 6.3 `all.h` kitchen-sink 头文件

虽然已经标记为 deprecated，但 `all.h` 仍然存在且被 README 引用。它一次性拉入 abseil log、所有 net、sync、runtime 头文件，导致：

- 编译时间爆炸（任何修改触发全量重编译）
- 依赖泄漏（用户被迫依赖 abseil、base）

### 6.4 LevelDB 怎么做

LevelDB 只有极少的公共头文件（`leveldb/db.h`, `leveldb/write_batch.h` 等），所有实现细节（`skip_list.h`, `table.cc`）都在 `port/` 或 `.cc` 文件里。用户永远不会看到 `SKIP_LIST_H` 这种内部头。头文件之间依赖最小化，编译一个 "hello leveldb" 只需包含 `<leveldb/db.h>`。

---

## 七、并发原语设计

### 7.1 自造 mutex 的正确性与优化方向

```cpp
// tin/sync/mutex.h
class Mutex {
 private:
  int32_t state_;
  uint32_t sema_;
};
```

tin 自己实现了 `Mutex`、`RWMutex`、`Cond`、`RawMutex`、`Note`、`SyncSema`——全部基于自己的 semaphore 和 atomic。这是从 Go runtime 翻译来的（Go runtime 必须自造因为它是 runtime 本身）。

**决策：保留自造同步原语，不允许用标准库（`std::mutex`、`std::condition_variable`、`absl::Mutex` 等）替代。** tin 作为一个协程运行时，其同步原语需要与调度器深度协作（如 park/unpark 协程而非阻塞 OS 线程），标准库锁会破坏协程的 M:N 调度语义。因此自造是架构上的必要选择，而非历史遗留。

**但需优化**：
- 自造 mutex 目前没有经过形式化验证，在弱内存模型架构（ARM）上存在风险——应补充内存序（memory order）注释，并用 ThreadSanitizer 验证
- 应利用 `std::atomic`（标准库的原子类型，而非锁）来替换手写的 compiler barrier / inline asm，使底层原子操作更可靠
- `owner_` 死锁检测字段应实现或删除（见 7.2）

### 7.2 `RawMutex` 的 `owner_` 字段未实现死锁检测

```cpp
// tin/runtime/raw_mutex.h
class RawMutex {
 private:
  uintptr_t key;
  M* owner_;   // 存了 owner 但从未在 Lock/Unlock 中设置或检查
};
```

`owner_` 字段存在但从未被赋值或使用，是死代码。

### 7.3 `Note` 语义模糊

```cpp
// tin/runtime/raw_mutex.h
class Note {
  void Wakeup();
  void Sleep();
  void Clear();
  bool TimedSleep(int64_t ns);
  bool TimedSleepG(int64_t ns);  // G 版本？文档呢？
};
```

`Note` 是 Go runtime 的内部同步原语，直接暴露在 `tin/runtime/` 中。`TimedSleep` 和 `TimedSleepG` 的区别没有注释说明。

### 7.4 `ThreadPoll`（注意拼写：ThreadPoll 而非 ThreadPool）

```cpp
// tin/runtime/threadpoll.cc
ThreadPoll::ThreadPoll()
  : num_threads_(64) {   // 硬编码 64 个线程
```

- 文件名和类名是 `ThreadPoll`（轮询）而非 `ThreadPool`（线程池）——拼写错误
- 硬编码 64 个线程，不根据 `hardware_concurrency()` 调整
- 用 `absl::Notification` 当信号量，注释写着 `// consider replace with conditional variable`

### 7.5 `sysmon` 硬编码轮询间隔

```cpp
// tin/runtime/sysmon.cc
absl::SleepFor(absl::Milliseconds(8));  // 硬编码 8ms
if (last_poll + 10 < now) { ... }       // 硬编码 10ms
```

Go 的 sysmon 会动态调整（从 20µs 指数退避到 10ms），tin 是固定 8ms，在空闲时浪费 CPU。

---

## 八、接口设计

### 8.1 `Read`/`Write` 的返回值语义混乱

```cpp
// tin/io/io.h
class Reader {
  virtual int Read(void* buf, int nbytes) = 0;
};

// tin/net/tcp_conn.h
// note: Read full or partial on success, or read partial on failure.
// return value : indicate n bytes written. n >= 0.
// don't handle error based on return value.
// detail error, see tin::GetErrorCode()
int Read(void* buf, int nbytes);
```

- 返回 `int` 而非 `size_t`/`ssize_t`，在 64 位平台上大缓冲区可能溢出
- 注释说"不要根据返回值判断错误"——那返回值的语义是什么？成功返回字节数，失败也返回字节数（可能 > 0），错误在 errno 里
- `n >= 0` 意味着永不返回 -1，那 EOF 怎么表示？答：在 errno 里设 `TIN_EOF`，但返回值可能是 0 或部分字节数

对比 LevelDB 的 `SequentialFile::Read`：

```cpp
// leveldb/env.h
virtual Status Read(size_t n, Slice* result, char* scratch) = 0;
// 返回 Status，数据通过 Slice* result 输出。清晰。
```

### 8.2 `Spawn` 的参数传递

```cpp
// tin/runtime/spawn.h
template <typename Functor, typename... Args>
void Spawn(Functor functor, Args&&... args) {
    auto boundFunction = absl::bind_front(functor, std::forward<Args>(args)...);
    std::function<void()> closure = [boundFunction]() mutable { boundFunction(); };
    DoSpawn(closure);
}
```

- `absl::bind_front` + `std::function` 双重类型擦除，效率低
- 参数按值捕获（`bind_front` 默认值传递），对于 `TcpConn`（shared_ptr）可以接受，但对于大对象会拷贝
- 没有返回值——Go 的 goroutine 也没有返回值，但 C++ 用户可能期望 `Spawn` 返回一个 `future` 或 `JoinHandle`

### 8.3 `TcpConn` 的 `operator->` 模式

```cpp
// tin/net/tcp_conn.h
class TcpConn {
  TcpConnImpl* operator->() { return impl_.get(); }
};

// 用法: conn->Read(buf, n);  // 实际调用 impl_->Read()
```

这让 `TcpConn` 看起来像智能指针，但它是值类型（可拷贝）。`operator->` 暴露了实现类 `TcpConnImpl` 的所有 public 方法，无法控制接口表面。LevelDB 的 `DB*` 是抽象基类，只暴露纯虚接口方法。

> 说明：此处批评的是**接口表面控制**（`operator->` 泄漏 impl 类型），与 3.3 讨论的**所有权模型**（`shared_ptr` 是否恰当）是两个独立维度。3.3 已确认 `shared_ptr` 应保留；本节建议的是在保留 `shared_ptr` 的前提下，考虑用 PIMPL + 显式转发方法（而非 `operator->`）来收窄公共接口。

### 8.4 `Config` 类缺乏验证

```cpp
// tin/config/config.h
void SetMaxProcs(int max_procs) {
    max_procs_ = max_procs;
    max_machine_ = max_procs_ * 4;  // 副作用：隐式修改另一个字段
}
```

- `SetMaxProcs(0)` 或 `SetMaxProcs(-1)` 不会报错
- `SetMaxProcs` 隐式修改 `max_machine_`，但 `SetMaxMachines` 可以独立设置——调用顺序影响结果
- `SetStackSize(0)` 会导致 `Greenlet::Create` 用 0 栈大小，必然崩

### 8.5 `Throw`/`Panic` 用 C++ 异常

```cpp
// tin/runtime/runtime.cc
void Throw(const char* str) {
    std::cout << str << std::endl;  // 用 cout 而非 LOG(FATAL)
    throw(str);                      // throw(const char*) — 抛裸字符串
}

void Panic(const char* str) {
    throw(str);                      // 连日志都没有
}
```

- `throw(str)` 抛的是 `const char*`，不是 `std::exception` 派生类，catch 端很难处理
- 协程栈切换 + C++ 异常的交互行为未定义——`zcontext` 切换栈后抛异常可能不会正确 unwind
- 用 `std::cout` 输出错误信息，而不是 `LOG(FATAL)` / `ABSL_LOG(FATAL)`

---

## 九、代码结构组织

### 9.1 目录结构缺乏层次分离

```
tin/
├── tin.h, tin.cc          # 顶层 API
├── runtime.h, time.h ...  # 聚合头文件与源码混在一起
├── runtime/               # 实现细节，但用户被迫 include
│   ├── env.h              # 暴露全局变量 rtm_env, sched, timer_q
│   ├── semaphore.h        # 暴露 Sudog
│   ├── raw_mutex.h        # 暴露 RawMutex, Note
│   └── util.h             # 暴露 GetG(), SetG(), GpCast()
├── sync/                  # 公共 API，但依赖 runtime/
│   ├── atomic.h           # 435 行手写原子操作
│   └── chan.h             # 依赖 runtime/semaphore.h
├── net/                   # 公共 API，但依赖 runtime/raw_mutex.h
└── communication/         # 公共 API，依赖 sync/ 和 runtime/
```

公共 API（`sync/`, `net/`）直接依赖实现细节（`runtime/`），没有 PIMPL 或接口隔离。用户 include 任何公共头文件，都会拉入 `runtime/` 内部头。

### 9.2 `atomic.h` 435 行手写原子操作

```cpp
// tin/sync/atomic.h — 435 行
namespace tin::atomic {
inline void memory_barrier() { ... }
template <typename T>
inline std::atomic<T>* atomic_addr(volatile T* p) { ... }
inline bool acquire_cas(volatile intptr_t* ptr, ...) { ... }
// ... 还有几十个函数
}
```

这是从 Go 的 `runtime/atomic_*.go` 翻译来的，但 C++ 有 `std::atomic`。这 435 行代码做的事情 `std::atomic` 全都能做，而且有标准保证、编译器优化、内存模型正确性。

### 9.3 同一概念多个实现

| 概念 | tin 实现 | 数量 | 说明 |
|---|---|---|---|
| 互斥锁 | `Mutex`, `RawMutex`, `FdMutex` | 3 个 | 均为自造，保留不替换；需统一接口规范 |
| 信号量 | `SemAcquire/SemRelease`, `SyncSema`, `Note` | 3 个 | 均为自造，保留不替换；需补文档 |
| 条件变量 | `Cond` | 1 个（但 `ThreadPoll` 用 `absl::Notification` 代替） | `Cond` 保留；`ThreadPoll` 的混用需统一 |
| 原子操作 | `tin::atomic::`, `AtomicFlag`, `std::atomic` | 3 套 | 底层原子操作可用 `std::atomic` 替换手写 inline asm |

`Mutex` 是给用户用的，`RawMutex` 是 runtime 内部用的，`FdMutex` 是 net 层用的——它们都是互斥锁，但没有统一的接口规范。LevelDB 用一个 `port::Mutex`（底层是 `std::mutex`），但 tin 作为协程运行时不能使用标准库锁（会阻塞 OS 线程而非 park 协程），因此保留多套自造锁是架构要求。改进方向是：统一接口规范、补充内存序注释、用 TSan 验证正确性，而非替换为标准库。

### 9.4 文件命名不一致

```
tin/runtime/net/
├── NetPoll.h        # PascalCase
├── netpoll.h        # snake_case
├── netpoll.cc
├── netpoll_epoll.cc
├── pollops.h        # snake_case 但缩写
├── poll_descriptor.h
```

`NetPoll.h` 和 `netpoll.h` 在同一目录下，仅大小写不同——在大小写不敏感的文件系统（Windows/macOS 默认）上会冲突。

### 9.5 LevelDB 怎么做

```
leveldb/
├── include/leveldb/     # 只有公共头文件，用户只 include 这里
│   ├── db.h
│   ├── env.h
│   └── ...
├── db/                  # 实现细节，用户看不到
│   ├── db_impl.cc
│   └── ...
├── port/                # 平台抽象层
│   ├── port_stdcxx.h
│   └── ...
└── table/               # 内部模块
```

- **公共 API 和实现物理分离**：`include/` 目录只有纯接口，实现全在 `.cc` 里
- **每个类一个职责**：`DB`, `Iterator`, `WriteBatch` 边界清晰
- **平台抽象统一**：`port::Mutex`, `port::CondVar` 一套，不暴露内部原语
- **无全局状态**：所有状态在 `DBImpl` 实例里

---

## 十、测试与可维护性

### 10.1 零测试

项目没有任何单元测试。只有 `examples/echo` 和 `examples/simple` 两个手动运行的示例。

没有测试意味着：
- 重构无法验证正确性
- 并发 bug 无法复现
- 回归无法检测

### 10.2 无文档（API 级别）

- 没有 Doxygen 注释
- 没有 `docs/` 下的 API 参考（`api-design-review.md` 是设计评审，不是用户文档）
- README 只有构建说明和示例代码，没有 API 文档
- 公共头文件注释稀少

### 10.3 LevelDB 怎么做

- `db_test.cc` 一个文件 3000+ 行测试
- 每个公共 API 都有对应的 `_test.cc`
- 有故障注入测试（`env_mem_env_test.cc`）
- `doc/` 目录有索引文档
- `include/leveldb/` 下的每个头文件都有详细注释

---

## 十一、与 LevelDB 的差距总结

| 维度 | LevelDB | tin | 差距 |
|---|---|---|---|
| **错误处理** | `Status` 值类型，显式传递 | 全局 errno，隐式获取 | 🔴 根本性差距 |
| **所有权** | `unique_ptr` 为主，RAII | 裸 `new`/`delete` 泄漏；`TcpConn` 的 `shared_ptr` 合理（应保留） | 🔴 严重（泄漏） |
| **类型安全** | 强类型，无 `void*` | `uintptr_t`/`void*` 大量使用 | 🔴 严重 |
| **模块边界** | `include/` vs 实现，PIMPL | 公共头直接依赖 runtime 内部 | 🟡 中等 |
| **并发原语** | `port::Mutex` 一套，基于 `std::` | 自造 3 套 mutex + 手写原子操作（保留，协程运行时不能用标准库锁） | 🟡 中等（需补内存序注释 + TSan 验证） |
| **命名一致性** | 严格遵守 Google Style | 命名空间混用、`NULL`、`typedef`、拼写错误 | 🟡 中等 |
| **测试** | 3000+ 行测试，故障注入 | 零测试 | 🔴 严重 |
| **文档** | 头文件注释 + doc/ | 仅 README + 设计评审 | 🟡 中等 |
| **全局状态** | 无，所有状态在实例中 | `rtm_env`/`sched`/`timer_q` 全局变量 | 🟡 中等（已有 Runtime 类计划） |
| **调试代码** | 无 | `#include <iostream>` 残留、注释掉的代码 | 🟢 轻微 |

### 优先级建议

| 优先级 | 任务 | 影响 |
|---|---|---|
| **P0** | 引入 `Result<T>`/`Status` 替代 errno 模式 | 消除最大的架构硬伤 |
| **P0** | 修复 `Env::Deinitialize()` 的内存泄漏（`sched`/`glet_tls` 未释放） | 正确性 |
| **P0** | 添加单元测试框架 + 核心路径测试 | 可维护性 |
| **P1** | 用 `std::atomic` 替换 `tin::atomic`（435 行手写原子操作） | 简化 + 正确性 |
| **P1** | 优化自造 mutex：补内存序注释、实现/删除 `owner_` 死锁检测、TSan 验证（保留自造，不替换为标准库） | 正确性 + 可维护性 |
| **P1** | 运行时核心对象（`Scheduler`/`TimerQueue`/`Greenlet`）用 `unique_ptr` 管理所有权，消除裸 `new`/`delete` | 内存安全 |
| **P1** | `TcpConn` 保留 `shared_ptr`，但改用 `std::make_shared` 消除二次分配；回调中用 `weak_ptr` 防循环引用 | 内存安全（保留共享所有权） |
| **P1** | 将 `void*` + 函数指针替换为 `std::function`/lambda | 类型安全 |
| **P2** | 物理分离公共头文件（`include/tin/`）与实现 | 模块边界 |
| **P2** | 统一命名风格（`nullptr`、`using`、`namespace tin::net`） | 一致性 |
| **P2** | 修复 `ThreadPoll` → `ThreadPool`、`Broascast` → `Broadcast` 等拼写 | 专业度 |
| **P3** | 消除 `uintptr_t` 当指针用（GUintptr 等 Go runtime 模式） | 类型安全 |
| **P3** | 删除 `all.h`，文档推荐模块化 include | 编译效率 |
