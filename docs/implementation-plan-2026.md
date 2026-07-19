# tin 项目重构实施计划

> 依据：`docs/code-review-2026.md` 评审报告
> 目标：在维持现有功能（Go 1.6 协程运行时）的前提下，系统性修复评审报告中的硬伤
> 约束：遵循最新 Google C++ Style Guide；保留 `TcpConn` 的 `shared_ptr` 自动资源管理；WSL2 构建通过
> 日期：2026-07-19

---

## 目录

1. [总则与改造原则](#一总则与改造原则)
2. [P0：正确性与架构硬伤（立即修复）](#二p0正确性与架构硬伤立即修复)
3. [P1：内存安全与简化（核心重构）](#三p1内存安全与简化核心重构)
4. [P2：模块边界与一致性（工程化）](#四p2模块边界与一致性工程化)
5. [P3：类型安全与清理（长期优化）](#五p3类型安全与清理长期优化)
6. [阶段验收标准](#六阶段验收标准)
7. [风险与回滚策略](#七风险与回滚策略)
8. [工作量估算](#八工作量估算)

---

## 一、总则与改造原则

### 1.1 改造原则

| 原则 | 说明 |
|---|---|
| **渐进式** | 每个阶段独立可编译、可运行、可提交。不搞大爆炸式重构 |
| **兼容过渡** | 引入新 API 时保留旧 API 并标记 `deprecated`，至少一个版本后再删除 |
| **测试先行** | P0 阶段引入测试框架；后续每项重构必须有对应测试覆盖 |
| **不破坏 `shared_ptr`** | `TcpConn`/`TCPListener` 的 `shared_ptr` 所有权模型保留不动（见评审 3.3） |
| **构建即验证** | 每个阶段结束在 WSL2 下执行完整构建（Ninja + Jumbo Build）确认无回归 |

### 1.2 分阶段总览

```
P0 (正确性)  ──►  P1 (内存/简化)  ──►  P2 (边界/一致)  ──►  P3 (类型/清理)
  1~2 周            2~3 周              2~3 周              1~2 周
```

---

## 二、P0：正确性与架构硬伤（立即修复）

### P0-1 修复 `Env::Deinitialize()` 内存泄漏

**问题**：`tin/runtime/env.cc:50` 的 `Deinitialize()` 只 `delete timer_q`，`sched` 和 `glet_tls` 泄漏。

**变更文件**：`tin/runtime/env.cc`、`tin/runtime/env.h`

**当前代码**：
```cpp
// tin/runtime/env.cc
void Env::Deinitialize() {
  delete timer_q;
}

// 全局裸指针
Env* rtm_env = NULL;
Scheduler* sched = NULL;
TimerQueue* timer_q = NULL;
thread_local Greenlet* glet_tls = NULL;
```

**目标代码**：
```cpp
// tin/runtime/env.cc
void Env::Deinitialize() {
  delete timer_q;     // timer_q 先停（已在 OnMainExit 中 Join）
  delete sched;       // 补充释放
  delete glet_tls;    // 补充释放
  timer_q = nullptr;
  sched = nullptr;
  glet_tls = nullptr;
}
```

**注意**：
- 释放顺序：`timer_q` → `sched` → `glet_tls`（依赖关系：sched 依赖 timer_q，glet_tls 依赖 sched）
- `OnMainExit()` 中已调用 `ThreadPoll::GetInstance()->JoinAll()` 和 `timer_q->Join()`，确保线程退出后再 delete
- `rtm_env` 本身在 `DeInitializeEnv()` 之后由调用方释放（暂不在本步处理，留待 P1-3）

**验证**：
- `examples/echo` 正常启动、处理连接、`Ctrl+C` 退出无崩溃
- Valgrind / ASan 检查无 "definitely lost"（`sched`/`glet_tls` 相关）

---

### P0-2 引入单元测试框架 + 核心路径测试

**问题**：零测试，重构无法验证正确性。

**变更**：新增 `tests/` 目录与 CMake 配置。

**目录结构**：
```
tin/
├── tests/
│   ├── CMakeLists.txt
│   ├── test_main.cc              # gtest main 入口
│   ├── sync/
│   │   ├── mutex_test.cc         # Mutex 加解锁、递归禁止
│   │   ├── wait_group_test.cc    # WaitGroup 计数
│   │   └── chan_test.cc          # Channel Push/Pop/Close
│   ├── net/
│   │   ├── ip_address_test.cc    # IP 解析
│   │   └── tcp_echo_test.cc      # 端到端 echo（spawn server + client）
│   └── runtime/
│       ├── scheduler_test.cc     # Spawn + Sched 调度
│       └── timer_test.cc         # 定时器精度
└── CMakeLists.txt                # 根 CMakeLists 添加 add_subdirectory(tests) + enable_testing()
```

**根 `CMakeLists.txt` 新增**：
```cmake
# 可选，默认 ON 便于开发
option(TIN_BUILD_TESTS "Build tin unit tests" ON)
if(TIN_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
```

**`tests/CMakeLists.txt`**：
```cmake
# 依赖 third_party/abseil-cpp 已提供的 gtest
find_package(GTest CONFIG REQUIRED)

add_executable(tin_tests
  test_main.cc
  sync/mutex_test.cc
  sync/wait_group_test.cc
  sync/chan_test.cc
  net/ip_address_test.cc
  runtime/scheduler_test.cc
  runtime/timer_test.cc
)
target_link_libraries(tin_tests PRIVATE tin GTest::gtest GTest::gtest_main)
gtest_discover_tests(tin_tests)
```

**首批必须覆盖的路径**：
1. `Mutex` — Lock/Unlock 配对、`MutexGuard` RAII
2. `Channel<int>` — Push/Pop 顺序、Close 后行为
3. `Spawn` — 协程能被调度执行、参数传递正确
4. `Timer` — 定时器在预期误差内触发
5. `TcpConn` echo — 本地回环 echo 一条数据

**验证**：
```bash
cd build && ninja tin_tests && ctest --output-on-failure
```

---

### P0-3 `Result<T>`/`Status` 类型设计（新 API，与 errno 并存）

**问题**：全局 errno 模式是最大架构硬伤（评审 2.x）。本步仅**设计并引入类型**，不立即替换所有调用点。

**新增文件**：`tin/status.h`、`tin/status.cc`、`tin/result.h`

**`tin/status.h`**（仿 LevelDB `Status`）：
```cpp
#ifndef TIN_STATUS_H_
#define TIN_STATUS_H_

#include <string>
#include "tin/error/error.h"  // 复用现有 TIN_* 错误码

namespace tin {

// 不可变值类型，显式传递，替代全局 errno。
// 与 leveldb::Status 设计一致：小巧（1 指针宽）、可拷贝、可忽略。
class Status {
 public:
  Status() noexcept = default;  // 隐式 OK
  static Status OK() { return Status(); }
  static Status FromErrno(int code);

  bool ok() const { return code_ == 0; }
  int code() const { return code_; }
  bool IsEOF() const { return code_ == TIN_EOF; }
  bool IsTimeout() const { return code_ == TIN_ETIMEOUT_INTR; }
  bool IsClosed() const { return code_ == TIN_OBJECT_CLOSED; }

  std::string ToString() const;

 private:
  explicit Status(int code) : code_(code) {}
  int code_ = 0;
};

}  // namespace tin
#endif  // TIN_STATUS_H_
```

**`tin/result.h`**：
```cpp
#ifndef TIN_RESULT_H_
#define TIN_RESULT_H_

#include <utility>
#include "tin/status.h"

namespace tin {

// Result<T> = Status + 值。值语义，不可隐式忽略错误。
template <typename T>
class Result {
 public:
  Result(T value) : value_(std::move(value)) {}         // 成功
  Result(Status status) : status_(std::move(status)) {} // 失败（value 未初始化）

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }
  T& value() { return value_; }
  const T& value() const { return value_; }

 private:
  Status status_;
  T value_;
};

}  // namespace tin
#endif  // TIN_RESULT_H_
```

**过渡策略**：为新 API 增加 `ReadV2`/`WriteV2`（或 `ReadResult`）返回 `Result<size_t>`，旧 `Read` 保留并标记 `[[deprecated]]`：
```cpp
class TcpConn {
 public:
  [[deprecated("Use ReadResult")]] int Read(void* buf, int nbytes);
  Result<size_t> ReadResult(void* buf, int nbytes);  // 新 API
};
```

**验证**：
- `status_test.cc`：OK / FromErrno / ToString / IsEOF 等
- 编译通过，不影响现有代码

---

## 三、P1：内存安全与简化（核心重构）

### P1-1 运行时核心对象用 `unique_ptr` 管理所有权

**问题**：评审 3.1，`env.cc` 中裸 `new`/`delete` 泛滥，`Env` 自身也裸 `new`。

**变更文件**：`tin/runtime/env.h`、`tin/runtime/env.cc`

**目标**：
```cpp
// tin/runtime/env.h
class Env {
 private:
  std::unique_ptr<Scheduler> sched_;
  std::unique_ptr<TimerQueue> timer_q_;
  // glet_tls 是 thread_local，保持裸指针由 thread 析构管理，或用 unique_ptr
};

// 全局指针改为 unique_ptr（在 env.cc 中定义）
extern std::unique_ptr<Env> rtm_env;
extern Scheduler* sched;            // 保留为非拥有指针，指向 rtm_env->sched_.get()
extern TimerQueue* timer_q;
```

**`env.cc` 变更**：
```cpp
std::unique_ptr<Env> rtm_env;

int InitializeEnv(EntryFn fn, int argc, char** argv, Config* new_conf) {
  rtm_env = std::make_unique<Env>();
  rtm_env->Initialize(fn, argc, argv, new_conf);
  return 0;
}

void DeInitializeEnv() {
  rtm_env->Deinitialize();
  rtm_env.reset();   // 自动释放 Env 及其成员
}
```

**注意**：
- `sched`/`timer_q` 全局指针被运行时各处直接引用（`sched->xxx()`），改为指向 `rtm_env` 内部的非拥有裸指针，避免全量替换调用点
- 全局裸指针在 `Initialize()` 中赋值，在 `Deinitialize()` 中置 `nullptr`

**验证**：echo 启停无崩溃；ASan 无泄漏；测试全绿。

---

### P1-2 `TcpConn` 保留 `shared_ptr`，改用 `make_shared` 消除二次分配

**问题**：评审 3.3.2(1)，当前 `MakeTcpConn(new TcpConnImpl(...))` 导致对象与控制块分离分配。

**约束**：保留 `shared_ptr` 所有权模型不动。

**变更文件**：`tin/net/tcp_conn.h`、`tin/net/listener.cc`、`tin/net/dialer.cc`

**当前代码**：
```cpp
// tcp_conn.h
class TcpConn {
 public:
  TcpConn(TcpConnImpl* conn) : impl_(conn) {}
 private:
  std::shared_ptr<TcpConnImpl> impl_;
};
inline TcpConn MakeTcpConn(TcpConnImpl* conn) { return {conn}; }

// listener.cc
conn = new TcpConnImpl(newfd);
return MakeTcpConn(conn);

// dialer.cc
return MakeTcpConn(new TcpConnImpl(netfd));
```

**目标代码**：
```cpp
// tcp_conn.h
class TcpConn {
 public:
  TcpConn() = default;  // 空连接
  explicit TcpConn(std::shared_ptr<TcpConnImpl> impl) : impl_(std::move(impl)) {}
  // operator-> 保留
 private:
  std::shared_ptr<TcpConnImpl> impl_;
};

// 移除 MakeTcpConn，改用工厂
inline TcpConn MakeTcpConn(NetFD* netfd) {
  return TcpConn(std::make_shared<TcpConnImpl>(netfd));
}

// listener.cc
TcpConn TCPListenerImpl::Accept() {
  NetFD* newfd = nullptr;
  int err = netfd_->Accept(&newfd);
  SetErrorCode(TinTranslateSysError(err));
  if (err != 0) {
    delete newfd;   // 失败时释放 fd
    return TcpConn();
  }
  return MakeTcpConn(newfd);
}

// dialer.cc — 同样改为 MakeTcpConn(netfd)；修复 netfd==NULL 时仍构造的 bug
```

**顺带修复的 bug**：`dialer.cc:32` 当前在 `netfd == NULL`（dial 失败）时仍 `new TcpConnImpl(NULL)`，应返回空 `TcpConn`。

**验证**：
- echo 正常工作
- `tcp_conn_test.cc`：构造、析构、拷贝、`operator->` 调用
- 确认无二次分配（可选：通过 ASan 的分配计数）

---

### P1-3 回调中用 `weak_ptr` 防循环引用

**问题**：评审 3.3.2(2)，`enable_shared_from_this` 若在 timer/netpoller 回调中捕获 `shared_from_this()`，会导致连接泄漏。

**审查范围**：`tin/net/tcp_conn.cc`、`tin/net/netfd.cc`、`tin/runtime/net/netpoll*.cc`、`tin/runtime/timer/timer_queue.cc`

**检查规则**：所有注册到 timer / netpoller 的回调，不得捕获 `shared_from_this()`，应捕获 `weak_ptr`：

```cpp
// 反例（连接泄漏）
timer_q->AddTimer(when, [self = shared_from_this()]() {
  self->OnTimeout();
});

// 正例
timer_q->AddTimer(when, [weak = std::weak_ptr<TcpConnImpl>(shared_from_this())]() {
  if (auto self = weak.lock()) {
    self->OnTimeout();
  }
});
```

**验证**：
- echo 长连接场景：建立 1000 连接后全部关闭，确认 fd 数量回落
- `/proc/self/fd` 数量监控（Linux）

---

### P1-4 用 `std::atomic` 替换 `tin::atomic`（435 行手写原子操作）

**问题**：评审 9.2，`tin/sync/atomic.h` 435 行手写原子操作，`std::atomic` 全能做且更安全。

**变更文件**：`tin/sync/atomic.h`、所有引用 `tin::atomic::` 的文件

**策略**：将 `tin::atomic::` 命名空间内的函数改为对 `std::atomic` 的薄包装（保持调用点不变），随后逐步内联删除。

**第一阶段（兼容包装）**：
```cpp
// tin/sync/atomic.h — 改写为 std::atomic 包装
namespace tin::atomic {

template <typename T>
inline bool acquire_cas(volatile T* ptr, T oldval, T newval) {
  return std::atomic_compare_exchange_strong_explicit(
      reinterpret_cast<std::atomic<T>*>(const_cast<T*>(ptr)),
      &oldval, newval,
      std::memory_order_acquire, std::memory_order_relaxed);
}

inline void memory_barrier() {
  std::atomic_thread_fence(std::memory_order_seq_cst);
}
// ... 其余函数同理
}  // namespace tin::atomic
```

**第二阶段（逐步替换调用点）**：搜索所有 `tin::atomic::` 调用，直接改用 `std::atomic`，最后删除 `atomic.h`。

**验证**：
- 全量编译通过
- `atomic_test.cc`：CAS、load/store 在多线程下正确
- ThreadSanitizer (TSan) 无报警

---

### P1-5 用 `std::mutex`/`absl::Mutex` 替换自造 mutex

**问题**：评审 7.1，自造 `Mutex`/`RawMutex` 未经验证，弱内存模型架构易出 bug。

**变更文件**：`tin/sync/mutex.h`、`tin/sync/mutex.cc`、`tin/runtime/raw_mutex.h`、调用点

**策略**：
- 用户侧 `tin::Mutex` → 内部改为 `absl::Mutex`（保持 `Lock`/`Unlock`/`MutexGuard` 接口）
- `runtime::RawMutex` → 保留（runtime 内部不能用 `absl::Mutex` 因为 absl 可能依赖 runtime？需确认；若 absl 独立则也可替换）

**`mutex.h` 改写**：
```cpp
#include <absl/synchronization/mutex.h>

class Mutex {
 public:
  void Lock() { mu_.Lock(); }
  void Unlock() { mu_.Unlock(); }
 private:
  absl::Mutex mu_;
};
```

**注意**：
- `FdMutex`（net 层）涉及协程 park/unpark，不能简单替换，保持现状
- runtime 内部的 `RawMutex` 若在调度器核心路径且 absl::Mutex 可能反向依赖 runtime，则保留

**验证**：`mutex_test.cc` 多协程并发加解锁；TSan 无报警。

---

## 四、P2：模块边界与一致性（工程化）

### P2-1 物理分离公共头文件（`include/tin/`）

**问题**：评审 6.x，公共头直接依赖 runtime 内部。

**目标结构**：
```
tin/
├── include/tin/          # 仅公共 API 头文件
│   ├── tin.h
│   ├── net/
│   │   ├── tcp_conn.h    # PIMPL，不暴露 TcpConnImpl
│   │   └── listener.h
│   ├── sync/
│   │   ├── mutex.h
│   │   ├── wait_group.h
│   │   └── chan.h
│   ├── io/
│   │   └── io.h
│   └── status.h
├── src/                  # 实现细节（原 tin/ 目录）
│   ├── runtime/
│   ├── net/
│   └── sync/
└── tests/
```

**PIMPL 改造 `TcpConn`**：
```cpp
// include/tin/net/tcp_conn.h — 用户只看到这个
namespace tin::net {
class TcpConn {
 public:
  TcpConn();
  Result<size_t> Read(void* buf, int nbytes);
  Result<size_t> Write(const void* buf, int nbytes);
  void Close();
  // ... 显式转发方法，不用 operator->
 private:
  class Impl;  // 前向声明
  std::shared_ptr<Impl> impl_;
};
}
```

**注意**：此步工作量大，建议分模块逐步迁移，先迁 `sync/` 再迁 `net/`。

**验证**：编译 echo 只需 `#include "tin/net/tcp_conn.h"`，不拉入 `runtime/` 头。

---

### P2-2 统一命名风格

| 项 | 当前 | 目标 | 影响文件 |
|---|---|---|---|
| 命名空间 | `namespace tin { namespace net {} }` | `namespace tin::net {}` | 全部头文件 |
| `NULL` | `NULL` (40+ 处) | `nullptr` | 全部 `.cc/.h` |
| `typedef` | `typedef Greenlet G;` | `using G = Greenlet;` | `util.h`、`p.h`、`ip_address.h` |
| include guard | `#pragma once` 混用 | 统一 `#ifndef TIN_XXX_H_` | 全部头文件 |
| 文件名大小写 | `NetPoll.h` vs `netpoll.h` | 统一 `snake_case.h` | `runtime/net/` |

**操作**：用脚本批量替换 `NULL`→`nullptr`、`typedef`→`using`，手动处理命名空间和文件改名。

**验证**：全量编译 + 测试。

---

### P2-3 修复拼写错误

| 当前 | 目标 | 位置 |
|---|---|---|
| `ThreadPoll` | `ThreadPool` | `runtime/threadpool.h/cc` + 所有引用 |
| `Broascast` | `Broadcast` | `sync/cond.h` |
| `EnableStackPprotection` | `EnableStackProtection` | `config/config.h` |
| `reader_sem` | `reader_sem_` | `sync/rwmutex.h` |

**注意**：`ThreadPoll`、`Broascast`、`EnableStackPprotection` 是公开 API，改名需提供兼容宏或 `deprecated` 别名过渡一个版本：
```cpp
[[deprecated("Use ThreadPool")]] using ThreadPoll = ThreadPool;
```

**验证**：编译 + 测试 + grep 确认无残留旧名。

---

### P2-4 清理调试代码与注释残留

- 删除 `tin/communication/chan.h:27` 的 `// std::cout << "Channel constructor " ...`
- 删除 `tin/runtime/spawn.h:23-70` 被注释的旧 `Spawn` 重载
- 删除 `tin/runtime/runtime.cc` 中 `Throw` 的 `std::cout`，改用 `LOG(FATAL)`
- 全局 grep `#include <iostream>` 在非示例代码中的使用，清理

---

## 五、P3：类型安全与清理（长期优化）

### P3-1 消除 `uintptr_t` 当指针用（GUintptr 等）

**问题**：评审 4.1，`PollDescriptor::rg`/`wg` 用 `uintptr_t` 存指针或常量，类型系统失明。

**目标**：引入 `variant` 或 tagged union：
```cpp
// 理想：用 std::variant 表达"要么是 ready 标志，要么是 G 指针"
struct PollDescriptor {
  std::variant<std::monostate, G*, ReadyTag> rg;
  std::variant<std::monostate, G*, ReadyTag> wg;
};
```

**注意**：此改动深入 runtime 核心，风险高，需在 P0/P1 测试完善后进行，且需性能基准对比。

---

### P3-2 替换 `void*` + 函数指针为 `std::function`/lambda

**问题**：评审 4.2/4.3，`UnlockFunc`、`TimerCallback`、`GreenletFunc` 全是 C 风格回调。

**目标**：
```cpp
// 当前
typedef bool(*UnlockFunc)(void* arg1, void* arg2);
typedef void (*TimerCallback)(void* arg, uintptr_t seq);
typedef void* (*GreenletFunc)(intptr_t);

// 目标
using UnlockFunc = std::function<bool()>;
using TimerCallback = std::function<void(uintptr_t seq)>;
using GreenletFunc = std::function<void()>;
```

**注意**：`GreenletFunc` 改动涉及 `zcontext` 栈切换入口签名，需同步修改汇编（`zcontext/src/asm/`），风险较高。

---

### P3-3 `Channel` 不对指针类型做 `delete`

**问题**：评审 3.4，`Channel<T*>` 在 `Close()` 时 `delete` 元素，所有权焊死在类型上。

**目标**：移除 `ClearQueue` 的 `delete` 特化，改为要求用户使用 `Channel<std::unique_ptr<T>>` 或 `Channel<std::shared_ptr<T>>`。

```cpp
// 删除
template <typename T>
void ClearQueue(std::deque<T>& queue, std::true_type) {
  for (auto iter = queue.begin(); iter != queue.end(); ++iter) {
    delete *iter;
  }
}
// 统一用普通清理
template <typename T>
void ClearQueue(std::deque<T>& queue, std::false_type) {
  queue.clear();
}
```

**破坏性变更**，需在 changelog 标注。

---

### P3-4 删除 `all.h`

**问题**：评审 6.3，kitchen-sink 头文件导致编译爆炸。

**操作**：
1. 更新 README，推荐按模块 include
2. `all.h` 内容改为 `#error "all.h is removed; include specific headers"` 一个版本
3. 最终删除

---

## 六、阶段验收标准

### 6.1 P0 验收

- [ ] `Env::Deinitialize()` 释放 `sched`/`glet_tls`，ASan 无泄漏
- [ ] `tests/` 目录存在，`ctest` 全绿（至少 5 个测试文件）
- [ ] `tin/status.h`、`tin/result.h` 编译通过，`status_test.cc` 通过
- [ ] echo 示例正常工作
- [ ] WSL2 Ninja + Jumbo 构建成功

### 6.2 P1 验收

- [ ] 运行时核心对象用 `unique_ptr`，`rtm_env` 用 `make_unique`
- [ ] `TcpConn` 使用 `make_shared`，保留 `shared_ptr`
- [ ] 回调无 `shared_from_this()` 捕获（grep 验证）
- [ ] `tin::atomic::` 全部改为 `std::atomic` 包装或直接替换
- [ ] `tin::Mutex` 基于 `absl::Mutex`
- [ ] TSan 无报警
- [ ] 全部测试绿

### 6.3 P2 验收

- [ ] `include/tin/` 公共头目录建立，echo 编译不拉入 `runtime/`
- [ ] `NULL`→`nullptr`、`typedef`→`using` 全量替换
- [ ] 拼写错误修复（含 `deprecated` 别名过渡）
- [ ] 调试代码清理干净
- [ ] 全部测试绿

### 6.4 P3 验收

- [ ] `GUintptr` 改用类型安全容器
- [ ] `void*` 回调改用 `std::function`
- [ ] `Channel<T*>` 不再 `delete`
- [ ] `all.h` 删除
- [ ] 全部测试绿 + 性能无回退

---

## 七、风险与回滚策略

### 7.1 风险矩阵

| 阶段 | 改动 | 风险等级 | 主要风险 |
|---|---|---|---|
| P0-1 | 补 `delete sched/glet_tls` | 🟢 低 | 释放顺序错误导致 UAF |
| P0-2 | 引入测试框架 | 🟢 低 | CMake 配置问题 |
| P0-3 | 引入 `Status`/`Result` | 🟢 低 | 纯新增，不碰旧代码 |
| P1-1 | `unique_ptr` 管理核心对象 | 🟡 中 | 全局指针初始化时序 |
| P1-2 | `make_shared` 改造 TcpConn | 🟡 中 | 构造函数签名变更影响调用点 |
| P1-3 | `weak_ptr` 防循环引用 | 🟡 中 | 漏改导致连接泄漏回归 |
| P1-4 | 替换 `tin::atomic` | 🟡 中 | 内存序语义差异 |
| P1-5 | 替换自造 mutex | 🔴 高 | runtime 核心路径死锁/活锁 |
| P2-1 | PIMPL + `include/` 分离 | 🔴 高 | 大规模文件移动，构建系统重写 |
| P2-2 | 命名风格统一 | 🟢 低 | 机械替换 |
| P2-3 | 拼写修复 | 🟡 中 | 公开 API 破坏兼容 |
| P3-1 | 消除 `uintptr_t` | 🔴 高 | runtime 核心数据结构重写 |
| P3-2 | 替换 C 回调 | 🔴 高 | 涉及汇编栈切换入口 |
| P3-3 | Channel 不 delete | 🟡 中 | 破坏性 API 变更 |

### 7.2 回滚策略

- **每个子任务独立 commit**：`P0-1`、`P1-2` 等各自一个提交，便于 `git revert`
- **每个阶段一个分支**：`refactor/p0`、`refactor/p1`，合并前在分支上完整验证
- **高风险项（🔴）单独评审**：P1-5、P2-1、P3-1/3-2 需在 PR 中附带性能基准对比与 TSan/ASan 报告
- **构建验证脚本**：每个阶段结束执行
  ```bash
  cd build && cmake --build . --target tin echo simple tin_tests && ctest
  ```

---

## 八、工作量估算

| 阶段 | 子任务数 | 估算工时 | 备注 |
|---|---|---|---|
| **P0** | 3 | 1~2 周 | 含测试框架搭建 |
| **P1** | 5 | 2~3 周 | 含原子/mutex 替换的风险控制 |
| **P2** | 4 | 2~3 周 | PIMPL 分离是大头 |
| **P3** | 4 | 1~2 周 | 高风险项需谨慎 |
| **合计** | 16 | 6~10 周 | 含测试编写与验证 |

---

## 附录：关键文件索引

| 评审问题 | 涉及文件 |
|---|---|
| errno 模式 | `tin/error.h`、`tin/error/error.h`、`tin/runtime/runtime.h`、`tin/runtime/runtime.cc` |
| 内存泄漏 | `tin/runtime/env.cc`、`tin/runtime/env.h` |
| shared_ptr | `tin/net/tcp_conn.h`、`tin/net/listener.h`、`tin/net/listener.cc`、`tin/net/dialer.cc` |
| Channel delete | `tin/communication/chan.h` |
| 手写原子 | `tin/sync/atomic.h` |
| 自造 mutex | `tin/sync/mutex.h`、`tin/runtime/raw_mutex.h` |
| uintptr_t | `tin/runtime/guintptr.h`、`tin/runtime/net/netpoll.h` |
| void* 回调 | `tin/runtime/unlock.h`、`tin/runtime/timer/timer_queue.h`、`tin/runtime/greenlet.h` |
| 拼写错误 | `tin/runtime/threadpoll.h`、`tin/sync/cond.h`、`tin/config/config.h` |
| 调试残留 | `tin/runtime/spawn.h`、`tin/communication/chan.h` |
