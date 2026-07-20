# P3-1 / P3-2 改造方案（审核稿）

> **状态**: 待审核  
> **日期**: 2026-07-20  
> **原则**: 不改 lock-free 算法核心，不改协程切换 ABI，只消除不安全的 C 风格强转和裸 `void*` 回调。

---

## 一、背景

用户提出疑虑：**Go 运行时本身就是这么做的——用 `uintptr_t` 存指针、用 `void*` + 函数指针做回调。lock-free 算法也依赖裸指针的原子 CAS。不能随便改。**

这个疑虑是**完全正确的**。以下是详细分析。

---

## 二、现状盘点

### 2.1 P3-1: `uintptr_t` 当指针用的场景

| 编号 | 位置 | 用途 | 能否改 | 原因 |
|------|------|------|--------|------|
| **A1** | `GpCast(G*) → uintptr_t` / `GpCastBack(uintptr_t) → G*` | 调度器内部 G* ↔ uintptr_t 互转，用于 `SwitchG` 的 `intptr_t args` 参数传给 `jump_zcontext` | ❌ 不能改 | `jump_zcontext` 的 ABI 是 `intptr_t`，zcontext 汇编实现按整数传参/取参 |
| **A2** | `GUintptr` 类 (`guintptr.h`) | POD 包装 G*，用于 P 的 runq_ 数组、run_next_ | ❌ 不能改 | Go 的 `guintptr` 设计同构；runq 是 lock-free CAS 环形队列，CAS 目标是 `uintptr_t` |
| **A3** | `P::Address() → uintptr_t` | 返回 `reinterpret_cast<uintptr_t>(this)`，用于 `allp_` 数组的原子 store | ❌ 不能改 | `allp_` 是 `P*` 的原子数组，Go runtime 同构 |
| **A4** | `RawMutex::key` (`uintptr_t`) | 0=unlocked, kLocked=1, 或 `M* | kLocked` = held+waiters | ❌ 不能改 | Go runtime `mutex.key` 完全同构；lock-free CAS + 位运算 |
| **A5** | `Note::key` (`uintptr_t`) | 0=clear, kLocked=1, 或 `M*` = sleeper | ❌ 不能改 | Go runtime `note.key` 完全同构 |
| **A6** | `PollDescriptor::rg/wg` (`uintptr_t`) | 0=none, kPdWait=1, kPdReady=2, 或 `G*` | ❌ 不能改 | Go runtime `pollDesc.rg/wg` 完全同构；lock-free CAS |
| **A7** | `PollDescriptor::fd` (`uintptr_t`) | 存 OS fd | ✅ 可改 | 只是存 fd 值，改为 `int` 即可 |
| **A8** | `M::next_waitm_` (`uintptr_t`) | RawMutex 等待队列链表，存下一个 M* | ❌ 不能改 | Go runtime `m.nextwaitm` 同构；lock-free 链表 |
| **A9** | `semroot()` 中 `(uintptr_t(addr) >> 3) % kSemTabSize` | 信号量哈希表分桶 | ❌ 不能改 | Go runtime `semtable` 同构 |
| **A10** | `tin::atomic` 的 `uintptr_t` 重载 | CAS/load/store on `uintptr_t*` | ❌ 不能改 | 这是 A2/A4/A5/A6/A8 的底层支撑 |

**结论**：A1-A6, A8-A10 全部是 Go runtime 同构的 lock-free 算法核心，**不能改**。只有 A7（`PollDescriptor::fd`）可以安全地从 `uintptr_t` 改为 `int`。

### 2.2 P3-2: `void*` + 函数指针回调场景

| 编号 | 位置 | 签名 | 用途 | 能否改 | 原因 |
|------|------|------|------|--------|------|
| **B1** | `UnlockFunc = bool(*)(void*, void*)` | `(void* arg1, void* arg2) → bool` | Greenlet park 时注册的 unlock 回调 | ⚠️ 可改但**收益低风险高** | Park 发生在调度器核心路径，`std::function` 有堆分配风险（small object optimization 不保证） |
| **B2** | `TimerCallback = void(*)(void*, uintptr_t)` | `(void* arg, uintptr_t seq) → void` | 定时器到期回调 | ✅ 可改 | Timer 回调不在热路径，`std::function` 的开销可接受 |
| **B3** | `GreenletFunc = void* (*)(intptr_t)` | `(intptr_t) → void*` | Greenlet 入口函数，传给 `make_zcontext` | ❌ 不能改 | zcontext C ABI 要求 `void (*)(intptr_t)` |
| **B4** | `M::G0StaticProc(intptr_t) → void*` | GreenletFunc | g0 的入口 | ❌ 不能改 | 同 B3 |
| **B5** | `NetPollBlockCommit(void*, void*) → bool` | UnlockFunc | netpoll park 时提交 CAS | ⚠️ 同 B1 | 调度核心路径 |

**结论**：B3/B4 是 zcontext C ABI 约束，不能改。B1/B5 是调度器 park 路径，改 `std::function` 有风险。只有 B2（TimerCallback）可以安全替换。

---

## 三、方案

### 3.1 P3-1：仅改 A7（`PollDescriptor::fd`）

**改动**：
```cpp
// poll_descriptor.h
struct PollDescriptor : public RefCountedThreadSafe {
  // ...
  int fd;          // 原: uintptr_t fd;  OS fd 本身就是 int
  // ...
};
```

**影响文件**：`poll_descriptor.h`、`poll_descriptor.cc`、`netpoll.cc`、`netpoll_epoll.cc`、`netpoll_kqueue.cc`、`netpoll_windows.cc`、`pollops.cc`、`netfd_common.cc`、`netfd_posix.cc`、`netfd_windows.cc`

**风险**：极低。`fd` 从不被当作指针用，纯粹是 OS 文件描述符值。

### 3.2 P3-1：为 A1-A6/A8-A10 添加类型安全包装（不改算法）

**策略**：不改 lock-free 算法本身，但用类型安全的包装层消除裸 `reinterpret_cast` 的散布。

#### 3.2.1 `GpCast` / `GpCastBack` — 保留但加 `static_assert`

```cpp
// util.h
inline uintptr_t GpCast(G* gp) {
  // A1: This uintptr_t ↔ G* conversion is required by the zcontext ABI
  // (jump_zcontext takes intptr_t). Not a bug — mirrors Go runtime's
  // guintptr pattern. DO NOT "fix" by replacing with G*.
  return reinterpret_cast<uintptr_t>(gp);
}

inline G* GpCastBack(uintptr_t gp) {
  // A1: Inverse of GpCast. Used to reconstruct G* from the intptr_t
  // argument passed through jump_zcontext, and from GUintptr::Integer().
  return reinterpret_cast<G*>(gp);
}
```

**改动量**：仅注释。零代码变更，零风险。

#### 3.2.2 `GUintptr` — 保留（Go `guintptr` 同构）

`GUintptr` 已经是类型安全的包装层了。它把 `uintptr_t` 封装成类，提供 `Pointer()` / `Integer()` / `Address()` 方法，比裸 `uintptr_t` 安全得多。**不需要改。**

#### 3.2.3 `RawMutex::key` / `Note::key` — 保留，注释说明

```cpp
// raw_mutex.h
class RawMutex {
  // ...
  // A4: key is a uintptr_t used as a lock-free word:
  //   0           = unlocked
  //   kLocked (1) = held, no waiters
  //   M* | kLocked = held, waiters queued via M::next_waitm_
  // This mirrors Go runtime's mutex. The pointer-integer punning is
  // intentional and required for the CAS-based fast path.
  uintptr_t key;
  M* owner_;
};
```

**改动量**：仅注释。零代码变更，零风险。

### 3.3 P3-2：仅改 B2（`TimerCallback`）

**当前**：
```cpp
using TimerCallback = void (*)(void* arg, uintptr_t seq);
// ...
struct Timer {
  int64_t when;
  int64_t period;
  uintptr_t seq;
  TimerCallback f;
  void* arg;
};
```

**改为**：
```cpp
using TimerCallback = std::function<void(uintptr_t seq)>;
// ...
struct Timer {
  int64_t when;
  int64_t period;
  uintptr_t seq;         // 保留: 用于 deadline 版本检查（Go runtime 同构）
  TimerCallback f;       // 改: std::function 替代函数指针+void*
  // void* arg 删除: 回调通过闭包捕获
};
```

**调用点变更**：

| 当前 | 改后 |
|------|------|
| `void WakeupSleeperFn(void* arg, uintptr_t seq) { G* gp = static_cast<G*>(arg); Ready(gp); }` | `Timer cb = [gp]() { Ready(gp); };` |
| `void OnSemDeadlineReached(void* arg, uintptr_t seq) { ... }` | `[s]() { ... }()` |
| `void NetpollDeadline(void* arg, uintptr_t seq) { ... }` | `[pd, seq]() { ... }()` |
| `fired_f(fired_arg, fired_seq);` | `fired_f(fired_seq);` |

**影响文件**：`timer_queue.h`、`timer_queue.cc`、`netpoll.cc`、`semaphore.cc`、`poll_descriptor.cc`、`netfd_common.cc`

**风险**：低。Timer 回调不在调度热路径（定时器到期才触发）。`std::function` 的 SSO（small string optimization 等价物）对于无捕获或单指针捕获的 lambda 通常不分配堆内存。

**`seq` 保留原因**：`PollDescriptor::seq` 用于 deadline 版本检查（`if (seq != pd->seq) return;`），这是 Go runtime 的标准模式，不是指针强转。

### 3.4 P3-2：B1/B5（`UnlockFunc`）— **不改**

**理由**：

1. **热路径**：`Park()` 是调度器核心，每次 greenlet 阻塞都会调用。`std::function` 的构造可能触发堆分配（当捕获超过 SSO 阈值时），在协程调度路径上引入不可控延迟。

2. **Go runtime 同构**：Go 的 `goready` / `gopark` 也是函数指针 + 回调参数的模式。这不是 C 风格遗留，而是刻意设计——函数指针是 POD，可以零开销存储在 `G` 结构体中。

3. **ABI 稳定性**：如果未来要暴露 C 接口给其他语言绑定，函数指针是唯一可用的方式。

**建议**：为 `UnlockFunc` 添加注释说明保留原因即可。

### 3.5 P3-2：B3/B4（`GreenletFunc`）— **不改**

**理由**：`make_zcontext` 的 C ABI 签名是 `void (*fn)(intptr_t)`，这是 zcontext 汇编实现的硬约束。改 `GreenletFunc` 会导致编译错误。

---

## 四、改动清单总览

| 任务 | 文件 | 改动类型 | 风险 |
|------|------|----------|------|
| P3-1 A7 | `poll_descriptor.h` + ~8 个 .cc | `uintptr_t fd` → `int fd` | 极低 |
| P3-1 A1-A6/A8-A10 | `util.h`, `raw_mutex.h`, `guintptr.h` 等 | 仅添加注释 | 零 |
| P3-2 B2 | `timer_queue.h`, `timer_queue.cc`, `netpoll.cc`, `semaphore.cc` 等 | `TimerCallback` 改为 `std::function` | 低 |
| P3-2 B1/B5 | `unlock.h`, `scheduler.h` | 仅添加注释 | 零 |
| P3-2 B3/B4 | `greenlet.h` | 仅添加注释 | 零 |

**总代码变更量**：约 100-150 行（含注释），核心算法零改动。

---

## 五、不做什么（及原因）

| 不做的事 | 原因 |
|----------|------|
| 不把 `GUintptr` 改成 `std::atomic<G*>` | Go runtime 的 `guintptr` 设计是有意的：`G*` 在 GC 环境下可能被移动，`uintptr_t` 是 GC-safe 的。tin 虽然没有移动 GC，但保持与 Go 同构有助于未来理解和移植。 |
| 不把 `RawMutex::key` 改成 `std::atomic<M*>` | `key` 不是纯指针——它是 `M* | kLocked` 的位编码。`std::atomic<M*>` 无法表达这种位运算。 |
| 不把 `Note::key` 改成 `std::atomic<M*>` | 同上，`key` 有三态：0/kLocked/M*。 |
| 不把 `PollDescriptor::rg/wg` 改成 `std::atomic<G*>` | 同上，rg/wg 有四态：0/kPdWait/kPdReady/G*。 |
| 不把 `UnlockFunc` 改成 `std::function` | 调度热路径，堆分配风险。Go runtime 同构。 |
| 不把 `GreenletFunc` 改成 `std::function` | zcontext C ABI 硬约束。 |

---

## 六、验收标准

- [ ] `PollDescriptor::fd` 类型从 `uintptr_t` 改为 `int`，编译通过
- [ ] `TimerCallback` 改为 `std::function<void(uintptr_t)>`，所有调用点更新
- [ ] lock-free 路径（A1-A6/A8-A10）仅添加注释，零代码变更
- [ ] `UnlockFunc` / `GreenletFunc` 仅添加注释说明保留原因
- [ ] WSL2 clean build 成功
- [ ] 全部 41 个测试通过
- [ ] echo 示例正常运行

---

## 七、结论

**P3-1 和 P3-2 的大部分内容不应该改**——它们是 Go runtime 同构的 lock-free 算法核心，用 `uintptr_t` 存指针和用函数指针做回调是刻意设计，不是 C 风格遗留。

真正可以安全改的只有两处：
1. `PollDescriptor::fd`：`uintptr_t` → `int`（纯粹的 fd 值，不涉及指针语义）
2. `TimerCallback`：函数指针 + `void*` → `std::function`（非热路径，可接受 `std::function` 开销）

请审核后告知是否执行。
