# Spawn 调用链深度分析与优化方案

> **状态**: 待审核  
> **日期**: 2026-07-20  
> **范围**: `tin::Spawn` 全调用链，涉及 Greenlet 生命周期、调度器 lock-free 算法、命名规范  
> **约束**: 不得影响调度器的 lock-free 算法（`GUintptr`、`P::runq_`、`schedlink_` 等必须保持 POD 语义和裸指针布局）

---

## 一、Lock-free 算法约束分析（前提）

在讨论优化之前，必须明确哪些东西 **不能动**。

### 1.1 `GUintptr` —— lock-free 运行队列的基石

```cpp
// tin/runtime/guintptr.h
class ALIGNAS(sizeof(void*)) GUintptr {
  uintptr_t integer_;   // 存储 Greenlet* 的整数值
};
```

**用途**：`P::runq_[256]`（本地运行队列）和 `Scheduler::runq_head_/runq_tail_`（全局运行队列）都使用 `GUintptr` 存储 Greenlet 指针。

**lock-free 约束**：
- `GUintptr` 必须是 **POD 类型**（注释明确标注 `"memset on GUintptr should be OK"`）
- 必须与 `uintptr_t` 二进制兼容（`ALIGNAS(sizeof(void*))`），因为 `atomic::cas` 直接操作其 `Address()` 返回的 `uintptr_t*`
- 不能给它加虚函数、不能加非平凡析构、不能改布局

**结论**：`GUintptr` **完全不能改**。

### 1.2 `P::runq_` —— work-stealing 调度队列

```cpp
// tin/runtime/p.h
class P {
  uint32_t runq_head_;       // 原子操作
  uint32_t runq_tail_;       // 原子操作
  GUintptr runq_[256];       // 环形缓冲区
  GUintptr run_next_;        // 原子 CAS（单槽快路径）
};
```

**lock-free 算法**（移植自 Go runtime）：
- `RunqPut`：CAS `run_next_`，失败则放入 `runq_[]` 环形队列
- `RunqGet`：CAS `run_next_`，失败则从 `runq_[]` 环形队列取
- `RunqSteal`：从其他 P 偷一半（`RunqGrab`），CAS `runq_head_`
- `RunqPutSlow`：队列满时，批量迁移到全局队列（用 `schedlink_` 串成链表）

**约束**：
- `runq_[]` 元素类型必须是 `GUintptr`（POD，可 memset）
- `RunqPutSlow` 通过 `gp->SetSchedLink()` 将 Greenlet 串成链表，`GlobalRunqBatch` 再用 `SchedLink()` 遍历——这要求 `Greenlet` 的 `schedlink_` 字段必须存在且可被 `GpCastBack(uintptr_t)` 还原

**结论**：`P::runq_` 和 `RunqPut/RunqGet/RunqSteal` **完全不能改**。

### 1.3 `schedlink_` —— 全局队列的链表节点

```cpp
// greenlet.h
class Greenlet {
  GUintptr schedlink_;   // 用于全局运行队列的侵入式链表
};

// scheduler.cc
void Scheduler::GlobalRunqPut(G* gp) {
  gp->SetSchedLink(nullptr);
  if (!runq_tail_.IsNull()) {
    runq_tail_.Pointer()->SetSchedLink(gp);  // 链表尾插
  } else {
    runq_head_ = gp;
  }
  runq_tail_ = gp;
  runq_size_++;
}
```

**约束**：
- `schedlink_` 必须是 `GUintptr` 类型（POD）
- `SetSchedLink(G*)` 和 `SchedLink()` 返回 `uintptr_t` 的接口不能改
- `RunqPutSlow` 中 `batch[i]->SetSchedLink(batch[i + 1])` 依赖此字段构建链表

**结论**：`schedlink_` 字段及其 getter/setter **不能改**。

### 1.4 `Greenlet*` → `G*` → `GUintptr` 类型转换链

```cpp
// util.h
using G = Greenlet;                         // 类型别名
inline uintptr_t GpCast(G* gp) { return reinterpret_cast<uintptr_t>(gp); }
inline G* GpCastBack(uintptr_t gp) { return reinterpret_cast<G*>(gp); }
```

整个调度器通过 `GpCast`/`GpCastBack` 在 `Greenlet*` 和 `uintptr_t` 之间转换，用于：
- `zcontext` 的 `intptr_t` 参数传递
- `GUintptr` 的存取
- `SwitchG` 的参数传递

**约束**：`Greenlet` 必须能被 `reinterpret_cast` 为 `uintptr_t` 并还原。这意味着 `Greenlet` **不能有虚函数**（虚函数表指针会改变对象布局，导致 `reinterpret_cast<Greenlet*>(uintptr_t)` 不安全）。

> ⚠️ **关键约束**：`Greenlet` 不能加虚函数！当前的 `virtual ~M()` 在 `M` 类上（不是 `Greenlet`），所以没有问题。但优化时绝不能给 `Greenlet` 加 `virtual`。

### 1.5 约束总结

| 组件 | 约束 | 能否修改 |
|------|------|:---:|
| `GUintptr` | POD，与 `uintptr_t` 二进制兼容 | ❌ 不能改 |
| `P::runq_[]` / `run_next_` | lock-free CAS 操作的存储 | ❌ 不能改 |
| `P::RunqPut/Get/Steal/Grab` | Go work-stealing 算法 | ❌ 不能改 |
| `Greenlet::schedlink_` | `GUintptr`，侵入式链表节点 | ❌ 不能改 |
| `Greenlet` 布局 | 必须可 `reinterpret_cast<uintptr_t>` | ❌ 不能加虚函数 |
| `Scheduler::GlobalRunq*` | 侵入式链表操作，依赖 `schedlink_` | ❌ 不能改 |
| `GpCast/GpCastBack` | `reinterpret_cast` 转换 | ❌ 不能改 |
| `zcontext` ABI | `void* (*)(intptr_t)` 函数签名 | ❌ 不能改 |

**可以安全修改的部分**：
- `Spawn` / `DoSpawn` / `RuntimeSpawn` 函数签名和实现
- `Greenlet::Create` 的参数列表（不涉及 lock-free）
- `Greenlet::Proc` 的异常处理（不涉及 lock-free）
- `Greenlet::closure_` / `entry_` / `cb_` 字段（不参与 lock-free 操作）
- `dead_queue_` 的类型（非 lock-free，仅在 `OnSwitch` 中单线程访问）
- 命名重构（不改变语义）
- `SpawnSimple` 的内部实现（不涉及 lock-free）

---

## 二、当前调用链详解

### 2.1 完整调用图

```
用户代码:
  tin::Spawn(&HandleClient, conn)
      │
      │  ── include/tin/runtime.h ──
      ▼
  Spawn<Functor, Args...>(functor, args...)          ← 模板，完美转发
      │  auto closure = [fn = forward(functor),
      │                  ...args = forward(args)]() mutable {
      │    std::invoke(fn, args...);
      │  };
      ▼
  DoSpawn(std::function<void()> closure)              ← inline，按值传递
      │  RuntimeSpawn(&closure);                       ← ⚠️ 取栈上局部变量地址
      ▼
  RuntimeSpawn(std::function<void()>* closure)         ← 指针参数
      │
      │  ── tin/runtime/greenlet.cc ──
      ▼
  Greenlet::Create(nullptr, closure, false, 0, false, 0)
      │  new Greenlet                                  ← 裸 new
      │  std::swap(glet->closure_, *closure)           ← ⚠️ C++03 swap hack
      │  NewStack(kFixedStack/kProtectedFixedStack)    ← 分配栈内存
      │  make_zcontext(stack, size, StaticProc)        ← 创建协程上下文
      │  GetP()->RunqPut(glet.get(), true)             ← ★ lock-free CAS 入队
      │  sched->WakePIfNecessary()                     ← 原子操作唤醒空闲 P
      │  glet.release()                                ← 裸指针返回（所有权转移给调度器）
      ▼
  （调度器选中该 greenlet，SwitchG 切换上下文）
      │
      │  ── greenlet.cc ──
      ▼
  Greenlet::StaticProc(intptr_t args)                  ← zcontext ABI 入口
      │  Greenlet* glet = reinterpret_cast<Greenlet*>(args);
      ▼
  Greenlet::Proc()
      │  if (closure_) closure_();                     ← 执行用户闭包
      │  M()->AddToDeadQueue(this)                     ← 加入 M 的死亡队列
      │  Park()                                        ← 永久挂起（never return）
      │
      │  ── scheduler.cc ──
      ▼
  Scheduler::OnSwitch(curg)
      │  curg->M()->GetUnlockInfo()->Run()             ← 执行 unlock 回调
      │  curg->M()->ClearDeadQueue()                   ← delete gp（裸 delete）
```

### 2.2 涉及的数据结构

```
┌──────────────────────────────────────────────────────────────────────┐
│  Greenlet (= G)                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │  GUintptr schedlink_     ← ★ lock-free，侵入式链表节点        │  │
│  │  M* m_                    ← 当前绑定的 OS 线程                │  │
│  │  M* lockedm_              ← LockOSThread 绑定的 M             │  │
│  │  std::function<void()> cb_         ← ⚠️ 遗留死代码            │  │
│  │  GreenletFunc entry_      ← C ABI 入口（G0 用）               │  │
│  │  std::function<void()> closure_    ← 用户闭包                 │  │
│  │  intptr_t args_           ← C ABI 入口的参数                  │  │
│  │  void* retval_            ← C ABI 入口的返回值               │  │
│  │  std::string name_        ← 调试名字                         │  │
│  │  unique_ptr<Stack> stack_ ← 协程栈                           │  │
│  │  zcontext_t context_      ← 协程上下文                       │  │
│  │  int state_               ← GLET_RUNNING/RUNNABLE/...        │  │
│  │  int32_t flags_           ← kGletFlagG0 等                  │  │
│  │  int error_code_          ← 错误码                           │  │
│  │  Timer* timer_            ← 懒初始化定时器                   │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────┐
│  P (Logical Processor)                                               │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │  uint32_t runq_head_     ← ★ lock-free CAS                   │  │
│  │  uint32_t runq_tail_     ← ★ lock-free CAS                   │  │
│  │  GUintptr runq_[256]     ← ★ lock-free 环形队列              │  │
│  │  GUintptr run_next_      ← ★ lock-free CAS（单槽快路径）     │  │
│  │  P* link_                ← idle 链表                         │  │
│  │  M* m_                   ← 当前绑定的 M                      │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────┐
│  M (OS Thread)                                                       │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │  std::list<G*> dead_queue_  ← 已退出的 greenlet（待 delete）   │  │
│  │  Note park_                 ← M 的 park/wakeup                 │  │
│  │  P* p_                      ← 当前持有的 P                     │  │
│  │  G* curg_                   ← 当前运行的 greenlet              │  │
│  │  G* g0_                     ← M 的系统栈 greenlet              │  │
│  │  std::thread sys_thread_    ← OS 线程句柄                      │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 三、问题清单

### 3.1 命名问题

| # | 当前名称 | 问题 | 建议 | 影响 lock-free? |
|---|----------|------|------|:---:|
| N1 | `RuntimeSpawn` | 命名冗余，"Runtime" 前缀无意义 | `SpawnClosure` | ❌ 不影响 |
| N2 | `DoSpawn` | "Do" 前缀是 Java 风格，C++ 不推荐 | 合并到 `Spawn` 模板中 | ❌ 不影响 |
| N3 | `SpawnSimple` | "Simple" 含义不清 | `SpawnInternal` | ❌ 不影响 |
| N4 | `GreenletFunc` | 类型别名，不清晰 | `ZContextEntry` | ❌ 不影响 |
| N5 | `GpCast` / `GpCastBack` | "Gp" 缩写不直观 | `GreenletToUint` / `UintToGreenlet` | ⚠️ 被锁算法使用 |
| N6 | `GLET_RUNNING` 等 | 全大写 + 前缀，C 风格枚举 | `enum class GreenletState` | ❌ 不影响 |
| N7 | `kGletFlagG0` | "Glet" 缩写 | `kFlagG0` | ❌ 不影响 |
| N8 | `GetG` / `SetG` | 不够描述性 | `CurrentGreenlet` / `SetCurrentGreenlet` | ⚠️ 被锁算法使用 |
| N9 | `cb_` | 命名含糊（"callback"？） | 删除（死代码） | ❌ 不影响 |
| N10 | `retval_` | 缩写 | `entry_return_` | ❌ 不影响 |
| N11 | `glet_tls` | "glet" 缩写 + "tls" 缩写 | `current_greenlet` | ⚠️ 被锁算法使用 |
| N12 | `G` | 单字母别名 | 保留（Go runtime 惯例，广泛使用） | — |

> **⚠️ 标记的命名改动需要谨慎**：`GpCast`/`GpCastBack`/`GetG`/`SetG`/`glet_tls` 被 lock-free 代码直接使用。可以改名（纯文本替换），但不能改变类型签名或调用方式。建议用全局 `sed` 替换，一次性改完所有调用点。

### 3.2 API 设计问题

| # | 问题 | 严重程度 | 影响 lock-free? |
|---|------|:---:|:---:|
| A1 | `RuntimeSpawn(std::function<void()>*)` 指针参数 | 🔴 高 | ❌ 不影响 |
| A2 | `Spawn → DoSpawn → RuntimeSpawn → Create` 4 层间接 | 🟡 中 | ❌ 不影响 |
| A3 | `Greenlet::Create` 7 个参数，`entry`/`closure` 互斥未表达 | 🟡 中 | ❌ 不影响 |
| A4 | 3 个并行入口（`SpawnSimple` ×2 + `RuntimeSpawn`） | 🟡 中 | ❌ 不影响 |
| A5 | `joinable` 参数标注 "not implement" | 🟢 低 | ❌ 不影响 |

### 3.3 代码质量问题

| # | 问题 | 严重程度 | 影响 lock-free? |
|---|------|:---:|:---:|
| C1 | `Proc()` 无异常安全——用户闭包抛异常导致 dead queue 泄漏 + 调度器卡死 | 🔴 高 | ❌ 不影响 |
| C2 | 裸 `new` + `glet.release()` + `delete gp` | 🟡 中 | ❌ 不影响 |
| C3 | `cb_` 遗留死代码（每个 greenlet 多占 48 字节） | 🟡 中 | ❌ 不影响 |
| C4 | `std::swap(glet->closure_, *closure)` swap hack | 🟡 中 | ❌ 不影响 |
| C5 | `Greenlet::Create` 混合构造 + 调度，违反单一职责 | 🟢 低 | ❌ 不影响 |
| C6 | `dead_queue_` 是 `std::list<G*>`（裸指针列表） | 🟢 低 | ❌ 不影响 |

### 3.4 lock-free 约束相关

| # | 观察 | 风险 |
|---|------|:---:|
| L1 | `Greenlet` 不能有虚函数（`reinterpret_cast<uintptr_t>` 依赖对象地址） | 约束，非问题 |
| L2 | `schedlink_` 必须是 `GUintptr`（POD），不能改类型 | 约束，非问题 |
| L3 | `dead_queue_` **非** lock-free（仅在 `OnSwitch` 中由 g0 单线程访问） | 可以安全改类型 |

---

## 四、优化方案

### 方案 A：最小改动——修复 API 反模式

**目标**：消除指针参数、swap hack 和多余间接层。不改变命名、不改变内部结构。

#### 改动 1：删除 `DoSpawn`，`Spawn` 直接调用

```cpp
// include/tin/runtime.h

namespace tin {

// 内部入口（非模板，类型擦除）
void SpawnClosure(std::function<void()> closure);

template <typename Functor, typename... Args>
void Spawn(Functor&& functor, Args&&... args) {
  SpawnClosure([fn = std::forward<Functor>(functor),
                ...args = std::forward<Args>(args)]() mutable {
    std::invoke(fn, args...);
  });
}

// ...
}
```

#### 改动 2：`Greenlet::Create` 接受值而非指针

```cpp
// tin/runtime/greenlet.h
class Greenlet {
  static Greenlet* Create(std::function<void()> closure,
                          bool sysg0 = false,
                          int stack_size = kDefaultStackSize,
                          const char* name = "greenlet");
  // G0 专用（C ABI，zcontext 约束）
  static Greenlet* CreateG0(GreenletFunc entry, intptr_t args,
                            int stack_size, const char* name);
};
```

```cpp
// tin/runtime/greenlet.cc
Greenlet* Greenlet::Create(std::function<void()> closure,
                           bool sysg0, int stack_size, const char* name) {
  auto glet = std::make_unique<Greenlet>();
  glet->state_ = GLET_RUNNABLE;
  glet->closure_ = std::move(closure);   // ✅ move，不再 swap
  glet->SetName(name);
  // ... stack + context ...
  if (!sysg0) {
    GetP()->RunqPut(glet.get(), true);   // lock-free 入队，不改动
    sched->WakePIfNecessary();
  } else {
    glet->SetG0Flag();
    glet_tls = glet.get();
  }
  return glet.release();
}

void SpawnClosure(std::function<void()> closure) {
  runtime::Greenlet::Create(std::move(closure));
}
```

**改动量**：~30 行  
**影响 lock-free**：❌ 完全不影响  
**改动文件**：`include/tin/runtime.h`、`tin/runtime/greenlet.h`、`tin/runtime/greenlet.cc`

---

### 方案 B：命名重构 + 统一入口

**目标**：在方案 A 基础上，统一命名风格，合并冗余入口。

#### 改动 1：命名重构

| 旧名 | 新名 | 说明 |
|------|------|------|
| `RuntimeSpawn` | `SpawnClosure` | 去掉冗余 "Runtime" 前缀 |
| `DoSpawn` | （删除） | 合并到 `Spawn` |
| `SpawnSimple` (重载1) | `SpawnInternal` | 仅供 runtime 内部使用 |
| `SpawnSimple` (重载2) | `SpawnInternal` | 仅供 runtime 内部使用 |
| `GreenletFunc` | `ZContextEntry` | 明确表示这是 zcontext 的 C ABI 入口 |
| `GLET_RUNNING` 等 | `GreenletState::kRunning` 等 | `enum class` |
| `kGletFlagG0` | `kFlagG0` | 去掉 "Glet" 缩写 |
| `cb_` | （删除） | 死代码 |
| `retval_` | `entry_return_` | 明确含义 |

> **注意**：`GpCast`/`GpCastBack`/`GetG`/`SetG`/`glet_tls` 虽然 lock-free 代码在用，但改名只是文本替换，不影响语义。建议在单独 commit 中用 `sed` 全局替换，避免混入逻辑改动。

#### 改动 2：统一入口

```cpp
// include/tin/runtime.h

// ── 公共 API ──────────────────────────────────────────
template <typename Functor, typename... Args>
void Spawn(Functor&& functor, Args&&... args) {
  SpawnClosure([fn = std::forward<Functor>(functor),
                ...args = std::forward<Args>(args)]() mutable {
    std::invoke(fn, args...);
  });
}

// ── 内部 API ──────────────────────────────────────────
struct SpawnOptions {
  int stack_size = 0;           // 0 = 使用全局配置
  const char* name = "greenlet";
};

void SpawnClosure(std::function<void()> closure,
                  const SpawnOptions& opts = {});
```

```cpp
// tin/runtime/greenlet.h

// C ABI 入口（zcontext 约束，不可改签名）
using ZContextEntry = void* (*)(intptr_t);

class Greenlet {
 public:
  // 用户 greenlet 工厂方法
  static Greenlet* Create(std::function<void()> closure,
                          const SpawnOptions& opts = {});

  // G0 工厂方法（内部，zcontext ABI）
  static Greenlet* CreateG0(ZContextEntry entry, intptr_t args,
                            int stack_size, const char* name);

  // ...
};

// 内部 spawn（替代旧的 SpawnSimple）
void SpawnInternal(std::function<void()> closure,
                   const char* name = nullptr);
```

#### 改动 3：删除 `cb_` 遗留字段

```cpp
// 删除：
// std::function<void()>  cb_;        // unused (legacy)
```

每个 `Greenlet` 节省 48 字节（`std::function` 的典型大小）。

**改动量**：~80 行  
**影响 lock-free**：❌ 完全不影响（仅改命名和入口函数）  
**改动文件**：`include/tin/runtime.h`、`tin/runtime/greenlet.h`、`tin/runtime/greenlet.cc`、以及所有 `SpawnSimple` 调用点

---

### 方案 C：异常安全 + 资源管理

**目标**：在方案 B 基础上，保证 `Proc()` 异常安全，防止用户闭包抛异常导致调度器卡死。

#### 改动 1：`Proc()` 异常安全

```cpp
// tin/runtime/greenlet.cc

void Greenlet::Proc() noexcept {
  // 异常安全：用户闭包抛异常不应导致调度器崩溃。
  // 即使闭包抛异常，也必须：
  //   1. 加入 dead queue（否则内存泄漏）
  //   2. Park()（否则调度器无法回收此 greenlet）
  try {
    if (closure_) {
      std::move(closure_)();   // move 后调用，释放闭包捕获的资源
    } else if (entry_) {
      entry_return_ = entry_(args_);
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "Greenlet \"" << name_ << "\" threw: " << e.what();
  } catch (...) {
    LOG(ERROR) << "Greenlet \"" << name_ << "\" threw unknown exception";
  }

  // 无论是否异常，都执行回收流程
  state_ = GLET_EXITED;
  M()->AddToDeadQueue(this);
  Park();   // never return
}
```

> **注意**：`noexcept` 不阻止内部 try/catch——它只是告诉编译器此函数本身不会传播异常。如果 try/catch 之外的代码抛异常（理论上不会），`std::terminate` 会被调用，这是正确的——调度器内部不应该有未捕获异常。

#### 改动 2：`dead_queue_` 类型安全

`dead_queue_` **不是** lock-free 数据结构——它只在 `Scheduler::OnSwitch` 中由 g0 线程访问（单线程），所以可以安全改类型。

但考虑到 `ClearDeadQueue` 每次只清理一个（FIFO），改 `unique_ptr` 收益不大。保持 `std::list<G*>` + `delete` 是合理的。

**唯一的改进**：让 `ClearDeadQueue` 更明确地表达"每次清理一个"的语义：

```cpp
// tin/runtime/m.h
class M {
  // 语义不变，但名字更清晰
  void ReclaimOneExitedGreenlet();
  void EnqueueExitedGreenlet(G* gp);  // 替代 AddToDeadQueue
};
```

**改动量**：~50 行  
**影响 lock-free**：❌ 完全不影响（`Proc` 和 `dead_queue_` 都不参与 lock-free）  
**改动文件**：`tin/runtime/greenlet.cc`、`tin/runtime/greenlet.h`、`tin/runtime/m.h`、`tin/runtime/m.cc`、`tin/runtime/scheduler.cc`

---

### 方案 D：完整重构（方案 A + B + C + 分离构造与调度）

**目标**：按现代 C++ 最佳实践全面重构 Spawn 子系统，同时严格遵守 lock-free 约束。

#### 完整头文件设计

```cpp
// include/tin/runtime.h

#ifndef TIN_RUNTIME_H_
#define TIN_RUNTIME_H_

#include <functional>
#include <utility>

namespace tin {

// ── Spawn 选项 ────────────────────────────────────────
struct SpawnOptions {
  int stack_size = 0;           // 0 = 使用全局配置
  const char* name = "greenlet";
  // 未来扩展（不影响 ABI）：
  // int priority = 0;
};

// ── 公共 Spawn API ────────────────────────────────────
// 创建一个新协程并加入运行队列。
// 接受任意可调用对象和参数，完美转发。
template <typename Functor, typename... Args>
void Spawn(Functor&& functor, Args&&... args) {
  SpawnClosure(
      [fn = std::forward<Functor>(functor),
       ...args = std::forward<Args>(args)]() mutable {
        std::invoke(fn, args...);
      });
}

// ── 类型擦除的内部入口 ────────────────────────────────
void SpawnClosure(std::function<void()> closure,
                  const SpawnOptions& opts = {});

// ── 调度控制 ──────────────────────────────────────────
void Sched();
void LockOSThread();
void UnlockOSThread();
void Throw(const char* str);
void Panic(const char* str = nullptr);

}  // namespace tin

#endif  // TIN_RUNTIME_H_
```

```cpp
// tin/runtime/greenlet.h

#ifndef TIN_RUNTIME_GREENLET_H_
#define TIN_RUNTIME_GREENLET_H_

#include <cstdlib>
#include <memory>
#include <functional>
#include <string>

#include "context/zcontext.h"
#include "tin/config/config.h"
#include "tin/runtime/util.h"
#include "tin/runtime/guintptr.h"   // ★ lock-free，不改动
#include "tin/runtime/stack/stack.h"

namespace tin {
namespace runtime {

class M;
struct Timer;

// ── Greenlet 状态 ─────────────────────────────────────
enum class GreenletState {
  kRunning = 0,
  kRunnable = 1,
  kWaiting = 2,
  kSyscall = 3,
  kExited = 4
};

// ── Greenlet 标志 ─────────────────────────────────────
enum class GreenletFlag : int32_t {
  kG0 = 1,
};

// zcontext C ABI 入口（签名不可改，zcontext 约束）
using ZContextEntry = void* (*)(intptr_t);

class Greenlet {
 public:
  Greenlet();
  Greenlet(const Greenlet&) = delete;
  Greenlet& operator=(const Greenlet&) = delete;
  ~Greenlet();

  // ── ★ lock-free 接口（不改动签名）──────────────────
  // schedlink_ 用于全局运行队列的侵入式链表。
  // 类型必须保持 GUintptr（POD），不能改。
  void SetSchedLink(G* gp) { schedlink_ = gp; }
  uintptr_t SchedLink() const { return schedlink_.Integer(); }

  // ── 状态 ────────────────────────────────────────────
  GreenletState state() const { return state_; }
  void set_state(GreenletState s) { state_ = static_cast<int>(s); }
  // 兼容旧代码（lock-free 路径用 int 比较）
  int GetState() const { return state_; }
  void SetState(int s) { state_ = s; }

  // ── M 绑定 ──────────────────────────────────────────
  M* M() const { return m_; }
  void SetM(tin::runtime::M* m) { m_ = m; }
  M* LockedM() const { return lockedm_; }
  void SetLockedM(M* m) { lockedm_ = m; }

  // ── 标志 ────────────────────────────────────────────
  void SetG0Flag() { flags_ |= static_cast<int32_t>(GreenletFlag::kG0); }
  bool IsG0() const { return (flags_ & static_cast<int32_t>(GreenletFlag::kG0)) != 0; }

  // ── 名字 ────────────────────────────────────────────
  void SetName(const char* name);
  const char* GetName() const { return name_.c_str(); }

  // ── zcontext ────────────────────────────────────────
  zcontext_t* MutableContext() { return &context_; }

  // ── 错误码 ──────────────────────────────────────────
  int GetErrorCode() const { return error_code_; }
  void SetErrorCode(int code) { error_code_ = code; }

  // ── 定时器（懒初始化）──────────────────────────────
  Timer* GetTimer();

  // ── 工厂方法 ────────────────────────────────────────
  // 创建用户 greenlet，自动加入运行队列。
  static Greenlet* Create(std::function<void()> closure,
                          const SpawnOptions& opts = {});

  // 创建 G0（系统栈 greenlet），不加入运行队列。
  // zcontext ABI 约束：entry 必须是 void* (*)(intptr_t)。
  static Greenlet* CreateG0(ZContextEntry entry, intptr_t args,
                            int stack_size, const char* name);

 private:
  // zcontext 入口（C ABI，不可改签名）
  static void StaticProc(intptr_t args);

  // greenlet 执行体（noexcept：绝不传播异常到调度器）
  void Proc() noexcept;

 private:
  // ★ lock-free 字段（不改动类型/布局）
  GUintptr schedlink_;

  // M 绑定
  tin::runtime::M* m_;
  tin::runtime::M* lockedm_;

  // 执行体
  ZContextEntry entry_;              // G0 的 C ABI 入口
  std::function<void()> closure_;    // 用户闭包
  intptr_t args_;                    // C ABI 入口参数
  void* entry_return_;               // C ABI 入口返回值

  // 元数据
  std::string name_;
  std::unique_ptr<Stack> stack_;
  zcontext_t context_;
  int state_;
  int32_t flags_;
  int error_code_;
  Timer* timer_;

  // 删除了 cb_（遗留死代码）
};

// 内部 spawn（替代旧的 SpawnSimple）
void SpawnInternal(std::function<void()> closure,
                   const char* name = nullptr);

}  // namespace runtime

// 公共内部入口（替代旧的 RuntimeSpawn）
void SpawnClosure(std::function<void()> closure,
                  const SpawnOptions& opts = {});

}  // namespace tin
#endif  // TIN_RUNTIME_GREENLET_H_
```

```cpp
// tin/runtime/greenlet.cc

Greenlet* Greenlet::Create(std::function<void()> closure,
                           const SpawnOptions& opts) {
  auto glet = std::make_unique<Greenlet>();
  glet->state_ = static_cast<int>(GreenletState::kRunnable);
  glet->closure_ = std::move(closure);         // ✅ move
  glet->SetName(opts.name);

  int stack_size = opts.stack_size > 0 ? opts.stack_size : kDefaultStackSize;
  if (rtm_conf->IsStackProtectionEnabled()) {
    glet->stack_.reset(NewStack(kProtectedFixedStack, stack_size));
  } else {
    glet->stack_.reset(NewStack(kFixedStack, stack_size));
  }
  glet->context_ = make_zcontext(glet->stack_->Pointer(), stack_size, StaticProc);

  // ★ lock-free 入队（不改动）
  GetP()->RunqPut(glet.get(), true);
  sched->WakePIfNecessary();

  return glet.release();
}

Greenlet* Greenlet::CreateG0(ZContextEntry entry, intptr_t args,
                             int stack_size, const char* name) {
  auto glet = std::make_unique<Greenlet>();
  glet->state_ = static_cast<int>(GreenletState::kRunnable);
  glet->entry_ = entry;
  glet->args_ = args;
  glet->SetName(name);
  glet->stack_.reset(NewStack(kFixedStack, stack_size));
  glet->context_ = make_zcontext(glet->stack_->Pointer(), stack_size, StaticProc);
  glet->SetG0Flag();
  glet_tls = glet.get();
  return glet.release();
}

void Greenlet::Proc() noexcept {
  try {
    if (closure_) {
      std::move(closure_)();
    } else if (entry_) {
      entry_return_ = entry_(args_);
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "Greenlet \"" << name_ << "\" threw: " << e.what();
  } catch (...) {
    LOG(ERROR) << "Greenlet \"" << name_ << "\" threw unknown exception";
  }

  state_ = static_cast<int>(GreenletState::kExited);
  M()->AddToDeadQueue(this);
  Park();   // never return
}

void SpawnInternal(std::function<void()> closure, const char* name) {
  SpawnOptions opts;
  opts.name = name ? name : "internal";
  Greenlet::Create(std::move(closure), opts);
}

void SpawnClosure(std::function<void()> closure, const SpawnOptions& opts) {
  runtime::Greenlet::Create(std::move(closure), opts);
}
```

#### 调用点迁移

| 旧调用 | 新调用 | 文件 |
|--------|--------|------|
| `SpawnSimple(&MainGlet, nullptr, "main")` | `Greenlet::CreateG0(&MainGlet, 0, kDefaultStackSize, "main")` | `env.cc` |
| `SpawnSimple(absl::bind_front(&TimerQueue::Proc, this), "timer_queue")` | `SpawnInternal(absl::bind_front(&TimerQueue::Proc, this), "timer_queue")` | `timer_queue.cc` |
| `RuntimeSpawn(&closure)` | 删除（`Spawn` 模板直接调用 `SpawnClosure`） | `runtime.h` |

**改动量**：~200 行  
**影响 lock-free**：❌ 完全不影响  
**改动文件**：`include/tin/runtime.h`、`tin/runtime/greenlet.h`、`tin/runtime/greenlet.cc`、`tin/runtime/env.cc`、`tin/runtime/timer/timer_queue.cc`

---

## 五、对比总结

| 维度 | A: 最小改动 | B: 命名+统一 | C: 异常安全 | D: 完整重构 |
|------|:---:|:---:|:---:|:---:|
| **消除指针参数** | ✅ | ✅ | ✅ | ✅ |
| **消除多余间接层** | ✅ | ✅ | ✅ | ✅ |
| **命名重构** | ❌ | ✅ | ❌ | ✅ |
| **统一入口** | ❌ | ✅ | ❌ | ✅ |
| **删除 `cb_`** | ❌ | ✅ | ❌ | ✅ |
| **`SpawnOptions`** | ❌ | ✅ | ❌ | ✅ |
| **异常安全** | ❌ | ❌ | ✅ | ✅ |
| **`std::move(closure_)()`** | ❌ | ❌ | ✅ | ✅ |
| **`Create` / `CreateG0` 分离** | ❌ | ❌ | ❌ | ✅ |
| **改动行数** | ~30 | ~80 | ~50 | ~200 |
| **影响 lock-free** | ❌ | ❌ | ❌ | ❌ |
| **风险** | 极低 | 低 | 低 | 中 |
| **可分阶段实施** | — | ✅ 独立 | ✅ 独立 | — |

> 所有方案 **均不影响 lock-free 算法**。改动范围限于：
> - 公共 API 函数签名（`SpawnClosure` 替代 `RuntimeSpawn`）
> - Greenlet 的非 lock-free 字段（`closure_`、`entry_`、`cb_`）
> - `Proc()` 的异常处理
> - 工厂方法的参数列表

---

## 六、推荐

**推荐分阶段实施方案 D**：

1. **Phase 1**（方案 A）：消除指针参数 + swap hack → 立即修复反模式
2. **Phase 2**（方案 B）：命名重构 + 统一入口 + 删 `cb_` → 可独立 commit
3. **Phase 3**（方案 C）：`Proc() noexcept` + try/catch → 异常安全

每个阶段独立可验证，不混合逻辑改动和命名改动。

### 不推荐做的事

| 不推荐 | 原因 |
|--------|------|
| 给 `Greenlet` 加虚函数 | 破坏 `reinterpret_cast<uintptr_t>`，影响 lock-free |
| 把 `GUintptr` 改成 `std::atomic<G*>` | 破坏 POD 语义，`memset` 不安全 |
| 把 `runq_[]` 改成 `std::array` | 可能改变布局和对齐，影响 CAS |
| 把 `dead_queue_` 改成 lock-free | 不需要——它只在 `OnSwitch` 中单线程访问 |
| 把 `schedlink_` 改成 `std::shared_ptr` | 破坏侵入式链表，`GUintptr` 必须是 POD |
| 把 `Greenlet::Create` 返回 `unique_ptr` | `runq_` 存裸指针，`unique_ptr` 反而增加 `release`/`reset` 心智负担 |
