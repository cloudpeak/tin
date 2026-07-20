# Tin Per-P Timer 改造设计方案

> 参考来源：Go 1.15 `src/runtime/time.go`、`src/runtime/runtime2.go`、`src/runtime/proc.go`

---

## 1. 背景与动机

### 1.1 当前 Tin Timer 架构的问题

Tin 当前的定时器实现是一个**全局单例**的 `TimerQueue`，存在以下问题：

| 问题 | 说明 |
|------|------|
| **全局锁竞争** | 所有 P 上的协程添加/删除定时器都争抢同一把 `mutex_`，在多核场景下成为瓶颈 |
| **专用 goroutine 开销** | `TimerQueue::Proc()` 运行在一个独立的内部协程中，定时器回调需要跨协程唤醒，增加调度延迟 |
| **无法 Work Stealing** | 定时器与 P 的本地运行队列完全解耦，当一个 P 空闲时无法"偷取"其他 P 上已就绪的定时器 |
| **DelTimer 需要持锁** | 删除定时器必须获取 `mutex_` 并从堆中物理移除，在高频增删场景下锁竞争严重 |
| **Sysmon 无法精确睡眠** | `SysMon` 固定 `SleepFor(8ms)`，无法根据下一个定时器的到期时间自适应调整睡眠深度 |

### 1.2 Go 1.15 的解法

Go 1.14 起将全局定时器堆改为 **per-P 的四叉小顶堆**（每个 P 拥有独立的 timer heap），核心收益：

- **锁竞争大幅降低**：每个 P 操作自己的堆，只需加自己的 `timersLock`
- **无锁删除**：`deltimer` 通过原子 CAS 修改 timer 的 `status` 字段标记删除，堆的物理清理由拥有该堆的 P 在调度循环中完成
- **Work Stealing 一体化**：P 在偷取其他 P 的运行队列时，顺带 `checkTimers` 运行其他 P 上已到期的定时器
- **Sysmon 精确睡眠**：通过 `timeSleepUntil()` 遍历所有 P 的堆顶，找到最早到期时间，sysmon 据此自适应睡眠

---

## 2. Go 1.15 Per-P Timer 架构深度剖析

### 2.1 数据结构

#### `timer` 结构体（`time.go:17-37`）

```go
type timer struct {
    pp puintptr  // 该 timer 位于哪个 P 的堆上
    when   int64
    period int64
    f      func(interface{}, uintptr)
    arg    interface{}
    seq    uintptr
    nextwhen int64  // modtimer 时的新 when 值
    status uint32   // 原子状态机
}
```

关键设计：`pp` 字段让任何 P 都能知道一个 timer 属于哪个堆；`status` 是无锁操作的基础。

#### `p` 结构体的 timer 字段（`runtime2.go:661-685`）

```go
type p struct {
    // ...
    timersLock  mutex     // 保护 timers 的锁
    timers      []*timer  // 四叉小顶堆
    timer0When  uint64    // 原子读写：堆顶 timer 的 when（快速路径）
    numTimers   uint32    // 原子读写：堆中 timer 总数
    adjustTimers uint32   // timerModifiedEarlier 的计数
    deletedTimers uint32  // 原子读写：已标记删除的 timer 计数
    // ...
}
```

`timer0When` 是性能关键：调度循环的快速路径只需原子读它，无需加锁即可判断是否有定时器到期。

### 2.2 Timer 状态机（10 个状态）

```
timerNoStatus          // 0: 新建，未加入任何堆
timerWaiting           // 1: 在堆中等待触发
timerRunning           // 2: 正在执行回调（瞬态）
timerDeleted           // 3: 已标记删除，仍在堆中
timerRemoving          // 4: 正在从堆中移除（瞬态）
timerRemoved           // 5: 已从堆中移除
timerModifying         // 6: 正在被修改（瞬态）
timerModifiedEarlier   // 7: 修改为更早时间，nextwhen 存新值
timerModifiedLater     // 8: 修改为更晚时间，nextwhen 存新值
timerMoving            // 9: 正在移动堆位置（瞬态）
```

状态转换的核心规则：

| 操作 | 允许的起始状态 | 目标状态 |
|------|---------------|---------|
| `addtimer` | `NoStatus` | `Waiting` |
| `deltimer` | `Waiting`/`ModifiedEarlier`/`ModifiedLater` | `Deleted`（无锁CAS） |
| `modtimer` | `Waiting`/`ModifiedXX`/`NoStatus`/`Removed`/`Deleted` | `ModifiedXX` 或 `Waiting` |
| `cleantimers` | `Deleted`/`ModifiedXX` | `Removed`/`Waiting` |
| `runtimer` | `Waiting` | `Running` → `NoStatus`/`Waiting` |

### 2.3 核心操作详解

#### `addtimer(t)` — 添加到当前 P

```go
func addtimer(t *timer) {
    t.status = timerWaiting
    pp := getg().m.p.ptr()       // 当前 P
    lock(&pp.timersLock)
    cleantimers(pp)               // 先清理堆顶垃圾
    doaddtimer(pp, t)             // 物理加入堆
    unlock(&pp.timersLock)
    wakeNetPoller(t.when)         // 可能需要唤醒 netpoller
}
```

#### `deltimer(t)` — 无锁删除

```go
func deltimer(t *timer) bool {
    for {
        switch s := atomic.Load(&t.status); s {
        case timerWaiting, timerModifiedLater:
            if atomic.Cas(&t.status, s, timerModifying) {
                tpp := t.pp.ptr()
                atomic.Cas(&t.status, timerModifying, timerDeleted)
                atomic.Xadd(&tpp.deletedTimers, 1)
                return true  // 未运行就被删除
            }
        case timerModifiedEarlier:
            // 类似，但需要 adjustTimers--
        case timerDeleted, timerRemoving, timerRemoved:
            return false  // 已运行过
        case timerRunning, timerMoving, timerModifying:
            osyield()      // 自旋等待
        }
    }
}
```

**核心思想**：删除方只做标记（CAS status → `Deleted`），不做物理移除。拥有该堆的 P 在后续 `cleantimers`/`adjusttimers`/`runtimer` 中统一清理。

#### `modtimer(t, when, ...)` — 修改已有 timer

两种路径：
1. **timer 已被移除**（`NoStatus`/`Removed`）：相当于 `addtimer`，加入当前 P 的堆
2. **timer 仍在某 P 的堆中**：不能直接改 `when`（会破坏堆序），而是将新值写入 `nextwhen`，状态置为 `ModifiedEarlier`/`ModifiedLater`。拥有该堆的 P 在 `cleantimers`/`adjusttimers` 中用 `nextwhen` 覆盖 `when` 并重新堆化

#### `cleantimers(pp)` — 清理堆顶（持锁）

只处理堆顶元素（`timers[0]`）：
- `Deleted` → 物理移除
- `ModifiedXX` → 用 `nextwhen` 更新 `when`，重新堆化

复杂度 O(log N)，但通常只处理一两个元素就遇到 `Waiting` 状态即返回。

#### `adjusttimers(pp)` — 全堆调整（持锁）

当 `adjustTimers > 0` 时调用，遍历整个堆：
- 移除所有 `Deleted` 的 timer
- 用 `nextwhen` 更新所有 `ModifiedXX` 的 timer 并重新堆化

#### `runtimer(pp, now)` — 运行堆顶 timer

```go
func runtimer(pp *p, now int64) int64 {
    for {
        t := pp.timers[0]
        switch s := atomic.Load(&t.status); s {
        case timerWaiting:
            if t.when > now { return t.when }  // 未到期
            // CAS → Running，执行回调
            runOneTimer(pp, t, now)
            return 0  // 运行了一个
        case timerDeleted: /* 移除 */ 
        case timerModifiedXX: /* 更新 when，重新堆化 */
        }
    }
}
```

`runOneTimer` 会临时释放 `timersLock` 执行回调（避免回调中操作定时器导致死锁），执行完后重新加锁。

#### `checkTimers(pp, now)` — 调度循环入口

```go
func checkTimers(pp *p, now int64) (rnow, pollUntil int64, ran bool) {
    // 快速路径：无 adjustTimers 且堆顶未到期 → 直接返回
    if atomic.Load(&pp.adjustTimers) == 0 {
        next := int64(atomic.Load64(&pp.timer0When))
        if next == 0 { return now, 0, false }
        if now < next { return now, next, false }
    }
    // 慢速路径：加锁，adjust + run
    lock(&pp.timersLock)
    adjusttimers(pp)
    for len(pp.timers) > 0 {
        if tw := runtimer(pp, rnow); tw != 0 { break }
        ran = true
    }
    // 本地 P 且 deletedTimers 过多时做全量清理
    if pp == getg().m.p.ptr() && deletedTimers > len(timers)/4 {
        clearDeletedTimers(pp)
    }
    unlock(&pp.timersLock)
    return
}
```

### 2.4 调度循环集成

#### `findrunnable()` 中的定时器检查（`proc.go:2192`）

```go
func findrunnable() (gp *g, inheritTime bool) {
top:
    _p_ := _g_.m.p.ptr()
    // 1. 检查当前 P 的定时器
    now, pollUntil, _ := checkTimers(_p_, 0)
    // 2. 本地运行队列
    if gp := runqget(_p_); gp != nil { return gp, ... }
    // 3. 全局运行队列
    // 4. netpoll
    // 5. Work Stealing
    for i := 0; i < 4; i++ {
        for ... {
            p2 := allp[enum.position()]
            if gp := runqsteal(_p_, p2, stealRunNextG); gp != nil { return gp }
            // 偷取时顺带检查 p2 的定时器
            if i > 2 || (i > 1 && shouldStealTimers(p2)) {
                tnow, w, ran := checkTimers(p2, now)
                if ran { /* 可能有新 G 就绪 */ }
            }
        }
    }
}
```

#### `timeSleepUntil()` — Sysmon 用

```go
func timeSleepUntil() (int64, *p) {
    next := maxWhen
    lock(&allpLock)
    for _, pp := range allp {
        c := atomic.Load(&pp.adjustTimers)
        if c == 0 {
            w := int64(atomic.Load64(&pp.timer0When))
            if w != 0 && w < next { next = w; pret = pp }
            continue
        }
        // 有 adjustTimers 时需加锁精确扫描
        lock(&pp.timersLock)
        for _, t := range pp.timers { /* 找最早 */ }
        unlock(&pp.timersLock)
    }
    unlock(&allpLock)
    return next, pret
}
```

#### P 销毁时的 timer 迁移（`proc.go:4231-4247`）

```go
// STW 期间，将 pp 的 timers 全部迁移到 plocal
if len(pp.timers) > 0 {
    plocal := getg().m.p.ptr()
    lock(&plocal.timersLock)
    lock(&pp.timersLock)
    moveTimers(plocal, pp.timers)
    pp.timers = nil
    pp.numTimers = 0
    pp.adjustTimers = 0
    pp.deletedTimers = 0
    atomic.Store64(&pp.timer0When, 0)
    unlock(&pp.timersLock)
    unlock(&plocal.timersLock)
}
```

### 2.5 四叉堆算法

Go 使用 4-ary heap（每个节点最多 4 个子节点），相比二叉堆减少了比较次数（树更浅）：

```
父节点:   parent(i) = (i - 1) / 4
子节点:   child(i)  = i * 4 + 1, i*4+2, i*4+3, i*4+4
```

---

## 3. Tin 当前实现分析

### 3.1 当前架构图

```
┌─────────────────────────────────────────────┐
│              Global TimerQueue               │
│  ┌───────────────────────────────────────┐  │
│  │  mutex_ (RawMutex)                    │  │
│  │  timers_ (vector<Timer*>)  四叉堆     │  │
│  │  gp_ (专用协程)                       │  │
│  │  wait_note_ (睡眠唤醒)                │  │
│  └───────────────────────────────────────┘  │
│                    ▲                         │
│    ┌───────────────┼───────────────┐        │
│    │               │               │        │
│   P0              P1              P2  ...   │
│  (所有 P 共享同一个全局 timer 堆)            │
└─────────────────────────────────────────────┘
```

### 3.2 当前 `Timer` 结构（`timer_queue.h:23-38`）

```cpp
struct Timer {
    int i;              // 在堆中的索引
    int64_t when;
    int64_t period;
    uintptr_t seq;
    TimerCallback f;
    void* arg;
    // 缺失: pp (所属P), status (状态机), nextwhen
};
```

### 3.3 当前 `TimerQueue` 类（`timer_queue.h:40-77`）

```cpp
class TimerQueue {
    G* gp_;                    // 专用协程
    bool created_;
    bool rescheduling_;
    bool sleeping_;
    RawMutex mutex_;           // 全局唯一锁
    Note wait_note_;
    std::vector<Timer*> timers_;
    bool exit_flag_;
    tin::WaitGroup wait_group_;

    void Proc();               // 专用协程主循环
    void SiftUp(int i);
    void SiftDown(int i);
};
```

### 3.4 当前调用点

| 调用点 | 文件 | 操作 |
|--------|------|------|
| `InternalNanoSleep` | `timer_queue.cc:25` | `timer_q->Lock()` + `AddTimerLocked` + `Park` |
| `SemSetDeadline` | `semaphore.cc:114` | `timer_q->AddTimer(timer)` |
| `AddTimerRefCounted` | `net/pollops.cc:105` | `timer_q->AddTimer(t)` |
| `DelTimerRefCounted` | `net/pollops.cc:109` | `timer_q->DelTimer(t)` |
| `Env::OnMainExit` | `env.cc:106` | `timer_q->Join()` |
| `Env::Deinitialize` | `env.cc:64` | `timer_q_->Join()` |

### 3.5 当前 `Proc()` 主循环（`timer_queue.cc:173-232`）

```
loop:
    lock(mutex_)
    while heap 不空 and 堆顶已到期:
        弹出堆顶, unlock, 执行回调, lock
    if 无定时器:
        ParkUnlock(mutex_)     // 挂起协程
    else:
        sleeping_ = true
        unlock(mutex_)
        wait_note_.TimedSleepG(delta)  // 睡眠到下一个定时器
```

**问题**：回调执行在专用协程中，`Ready(gp)` 只是把目标协程放入运行队列，目标协程还需等待被调度，增加了唤醒延迟。

---

## 4. Tin Per-P Timer 改造设计

### 4.1 目标架构

```
┌──────┐  ┌──────┐  ┌──────┐
│  P0   │  │  P1  │  │  P2  │  ...
│timers │  │timers│  │timers│   每个 P 拥有独立的 timer 堆
│ Lock  │  │ Lock │  │ Lock │   独立的 timersLock
│timer0 │  │timer0│  │timer0│   原子 timer0When 快速路径
└───┬───┘  └───┬──┘  └───┬──┘
    │          │          │
    └──────────┼──────────┘
               │
     FindRunnable 中 checkTimers
     Work Stealing 时跨 P checkTimers
     Sysmon 通过 timeSleepUntil 找最早到期
```

### 4.2 改造范围总览

| 改造项 | 文件 | 说明 |
|--------|------|------|
| `Timer` 结构体重构 | `timer_queue.h` | 增加 `pp`、`status`、`nextwhen` |
| `P` 类增加 timer 字段 | `p.h` / `p.cc` | `timers_`、`timers_lock_`、`timer0_when_` 等 |
| 新增 per-P timer 操作 | `timer_queue.h` / `timer_queue.cc` | `AddTimer`/`DelTimer`/`ModTimer`/`CheckTimers` 等 |
| 删除全局 `TimerQueue` 类 | `timer_queue.h` / `timer_queue.cc` | 移除 `Proc()` 专用协程 |
| `Scheduler::FindRunnable` 集成 | `scheduler.cc` | 顶部 `CheckTimers` + 偷取时 `CheckTimers` |
| `Scheduler::ResizeProc` 集成 | `scheduler.cc` | P 销毁时 `MoveTimers` |
| `SysMon` 改造 | `sysmon.cc` | 使用 `TimeSleepUntil` 自适应睡眠 |
| 全局 `timer_q` 移除 | `env.h` / `env.cc` | 清理全局指针 |
| 调用点迁移 | `semaphore.cc`、`net/pollops.cc`、`timer_queue.cc` | 改为 per-P API |

### 4.3 详细设计

#### 4.3.1 Timer 状态枚举与结构体

```cpp
// tin/runtime/timer/timer_queue.h

enum TimerStatus : uint32_t {
  kTimerNoStatus = 0,
  kTimerWaiting,
  kTimerRunning,
  kTimerDeleted,
  kTimerRemoving,
  kTimerRemoved,
  kTimerModifying,
  kTimerModifiedEarlier,
  kTimerModifiedLater,
  kTimerMoving,
};

struct Timer {
  Timer() {
    pp = nullptr;
    when = period = nextwhen = 0;
    seq = 0;
    f = nullptr;
    arg = nullptr;
    status.store(kTimerNoStatus, std::memory_order_relaxed);
  }

  P* pp;                          // 该 timer 属于哪个 P 的堆
  int64_t when;
  int64_t period;
  int64_t nextwhen;               // modtimer 时的新 when
  uintptr_t seq;
  TimerCallback f;
  void* arg;
  std::atomic<uint32_t> status;   // 无锁状态机
};
```

#### 4.3.2 P 类增加 timer 字段

```cpp
// tin/runtime/p.h

class P {
 public:
  // ... 现有接口 ...

  // ---- Per-P Timer 接口 ----
  RawMutex& TimersLock() { return timers_lock_; }
  std::vector<Timer*>& Timers() { return timers_; }
  void SetTimer0When(uint64_t when) {
    timer0_when_.store(when, std::memory_order_release);
  }
  uint64_t Timer0When() {
    return timer0_when_.load(std::memory_order_acquire);
  }
  void IncNumTimers(int32_t n) {
    num_timers_.fetch_add(n, std::memory_order_relaxed);
  }
  uint32_t NumTimers() {
    return num_timers_.load(std::memory_order_relaxed);
  }
  void IncDeletedTimers(int32_t n) {
    deleted_timers_.fetch_add(n, std::memory_order_relaxed);
  }
  uint32_t DeletedTimers() {
    return deleted_timers_.load(std::memory_order_relaxed);
  }
  uint32_t AdjustTimers() {
    return adjust_timers_.load(std::memory_order_relaxed);
  }
  void IncAdjustTimers(int32_t n) {
    adjust_timers_.fetch_add(n, std::memory_order_relaxed);
  }

 private:
  // ... 现有字段 ...

  // ---- Per-P Timer 字段 ----
  RawMutex timers_lock_;
  std::vector<Timer*> timers_;
  std::atomic<uint64_t> timer0_when_{0};
  std::atomic<uint32_t> num_timers_{0};
  std::atomic<uint32_t> adjust_timers_{0};
  std::atomic<uint32_t> deleted_timers_{0};
};
```

> **注意**：`P` 使用 `aligned_alloc(64, ...)` 分配，新增字段不影响 cache-line 对齐。建议将 timer 字段放在 `pad` 之前，确保 `timers_lock_` 不与 runq 产生 false sharing。

#### 4.3.3 Per-P Timer 操作函数

以下函数作为**自由函数**实现（不放在 P 类内部，保持 P 类精简），放在 `timer_queue.h/cc` 中：

```cpp
namespace tin::runtime {

// ---- 堆操作（内部，持锁调用） ----
void DoAddTimer(P* pp, Timer* t);     // 物理加入堆，更新 timer0When
void DoDelTimer(P* pp, int i);       // 物理移除 timers_[i]
void DoDelTimer0(P* pp);             // 物理移除 timers_[0]
void SiftUpTimer(std::vector<Timer*>& t, int i);
void SiftDownTimer(std::vector<Timer*>& t, int i);
void UpdateTimer0When(P* pp);

// ---- 公开 API ----
void AddTimer(Timer* t);             // 添加到当前 P
bool DelTimer(Timer* t);             // 无锁删除（CAS status）
bool ModTimer(Timer* t, int64_t when, int64_t period,
              TimerCallback f, void* arg, uintptr_t seq);
bool ResetTimer(Timer* t, int64_t when);

// ---- 调度循环调用（持锁） ----
void CleanTimers(P* pp);             // 清理堆顶
void AdjustTimers(P* pp);            // 全堆调整
int64_t RunTimer(P* pp, int64_t now);// 运行堆顶，返回 0=已运行/-1=空/正数=下次时间
void CheckTimers(P* pp, int64_t now, // 调度循环入口
                 int64_t* rnow, int64_t* poll_until, bool* ran);
void ClearDeletedTimers(P* pp);      // 全量清理已删除 timer

// ---- P 间迁移 ----
void MoveTimers(P* dst, std::vector<Timer*>& src);

// ---- Sysmon 用 ----
int64_t TimeSleepUntil(P** out_pp);  // 遍历所有 P 找最早到期

}  // namespace tin::runtime
```

#### 4.3.4 `AddTimer` 实现要点

```cpp
void AddTimer(Timer* t) {
  if (t->when < 0)
    t->when = std::numeric_limits<int64_t>::max();
  t->status.store(kTimerWaiting, std::memory_order_relaxed);

  P* pp = GetP();  // 当前 P
  pp->TimersLock().Lock();
  CleanTimers(pp);
  DoAddTimer(pp, t);
  pp->TimersLock().Unlock();

  WakeNetPoller(t->when);  // 可能需要唤醒 netpoller
}

void DoAddTimer(P* pp, Timer* t) {
  if (t->pp != nullptr)
    LOG(FATAL) << "DoAddTimer: P already set";
  t->pp = pp;
  int i = static_cast<int>(pp->Timers().size());
  pp->Timers().push_back(t);
  SiftUpTimer(pp->Timers(), i);
  if (t == pp->Timers()[0]) {
    pp->SetTimer0When(static_cast<uint64_t>(t->when));
  }
  pp->IncNumTimers(1);
}
```

#### 4.3.5 `DelTimer` 无锁实现

```cpp
bool DelTimer(Timer* t) {
  for (;;) {
    uint32_t s = t->status.load(std::memory_order_acquire);
    switch (s) {
      case kTimerWaiting:
      case kTimerModifiedLater: {
        // acquirem 等价：禁止抢占
        if (t->status.compare_exchange_strong(s, kTimerModifying)) {
          P* tpp = t->pp;
          t->status.store(kTimerDeleted, std::memory_order_release);
          tpp->IncDeletedTimers(1);
          return true;  // 未运行就被删除
        }
        break;
      }
      case kTimerModifiedEarlier: {
        if (t->status.compare_exchange_strong(s, kTimerModifying)) {
          P* tpp = t->pp;
          tpp->IncAdjustTimers(-1);
          t->status.store(kTimerDeleted, std::memory_order_release);
          tpp->IncDeletedTimers(1);
          return true;
        }
        break;
      }
      case kTimerDeleted:
      case kTimerRemoving:
      case kTimerRemoved:
        return false;  // 已运行过
      case kTimerNoStatus:
        return false;  // 从未添加
      case kTimerRunning:
      case kTimerMoving:
      case kTimerModifying:
        // 其他 P 正在操作，自旋等待
        std::this_thread::yield();
        break;
      default:
        LOG(FATAL) << "DelTimer: invalid status";
    }
  }
}
```

#### 4.3.6 `CheckTimers` 调度循环入口

```cpp
void CheckTimers(P* pp, int64_t now,
                 int64_t* rnow, int64_t* poll_until, bool* ran) {
  *ran = false;
  *poll_until = 0;

  // 快速路径：无 adjustTimers 且堆顶未到期
  if (pp->AdjustTimers() == 0) {
    uint64_t next = pp->Timer0When();
    if (next == 0) {
      *rnow = now;
      return;
    }
    if (now == 0) now = MonoNow();
    if (now < static_cast<int64_t>(next)) {
      // 未到期，但如果 deletedTimers 过多仍需清理
      if (pp != GetP() ||
          static_cast<int32_t>(pp->DeletedTimers()) <=
          static_cast<int32_t>(pp->NumTimers() / 4)) {
        *rnow = now;
        *poll_until = static_cast<int64_t>(next);
        return;
      }
    }
  }

  // 慢速路径：加锁
  pp->TimersLock().Lock();
  AdjustTimers(pp);

  *rnow = now;
  if (!pp->Timers().empty()) {
    if (*rnow == 0) *rnow = MonoNow();
    while (!pp->Timers().empty()) {
      int64_t tw = RunTimer(pp, *rnow);
      if (tw != 0) {
        if (tw > 0) *poll_until = tw;
        break;
      }
      *ran = true;
    }
  }

  // 本地 P 且 deletedTimers 过多 → 全量清理
  if (pp == GetP() &&
      static_cast<int32_t>(pp->DeletedTimers()) >
      static_cast<int32_t>(pp->Timers().size() / 4)) {
    ClearDeletedTimers(pp);
  }

  pp->TimersLock().Unlock();
}
```

#### 4.3.7 `RunTimer` 实现

```cpp
int64_t RunTimer(P* pp, int64_t now) {
  for (;;) {
    Timer* t = pp->Timers()[0];
    uint32_t s = t->status.load(std::memory_order_acquire);
    switch (s) {
      case kTimerWaiting:
        if (t->when > now) return t->when;  // 未到期
        if (!t->status.compare_exchange_strong(s, kTimerRunning))
          continue;
        RunOneTimer(pp, t, now);  // 会临时释放锁
        return 0;                  // 运行了一个
      case kTimerDeleted:
        if (!t->status.compare_exchange_strong(s, kTimerRemoving))
          continue;
        DoDelTimer0(pp);
        t->status.store(kTimerRemoved, std::memory_order_release);
        pp->IncDeletedTimers(-1);
        if (pp->Timers().empty()) return -1;
        break;
      case kTimerModifiedEarlier:
      case kTimerModifiedLater:
        if (!t->status.compare_exchange_strong(s, kTimerMoving))
          continue;
        t->when = t->nextwhen;
        DoDelTimer0(pp);
        DoAddTimer(pp, t);
        if (s == kTimerModifiedEarlier) pp->IncAdjustTimers(-1);
        t->status.store(kTimerWaiting, std::memory_order_release);
        break;
      case kTimerModifying:
        std::this_thread::yield();
        break;
      default:
        LOG(FATAL) << "RunTimer: invalid status";
    }
  }
}
```

#### 4.3.8 `RunOneTimer` — 临时释放锁执行回调

```cpp
void RunOneTimer(P* pp, Timer* t, int64_t now) {
  TimerCallback fired_f = t->f;
  void* fired_arg = t->arg;
  uintptr_t fired_seq = t->seq;

  if (t->period > 0) {
    // 周期性 timer：更新 when，保持在堆中
    int64_t delta = t->when - now;
    t->when += t->period * (1 + -delta / t->period);
    SiftDownTimer(pp->Timers(), 0);
    t->status.store(kTimerWaiting, std::memory_order_release);
    UpdateTimer0When(pp);
  } else {
    // 一次性 timer：从堆中移除
    DoDelTimer0(pp);
    t->status.store(kTimerNoStatus, std::memory_order_release);
  }

  // 临时释放锁，执行回调
  pp->TimersLock().Unlock();
  fired_f(fired_arg, fired_seq);
  pp->TimersLock().Lock();
}
```

### 4.4 调度器集成

#### 4.4.1 `FindRunnable` 改造

```cpp
// scheduler.cc - FindRunnable()

G* Scheduler::FindRunnable(bool* inherit_time) {
  G* curg = GetG();
  M* curm = curg->M();

top:
  P* curp = curm->P();

  // === 新增：检查当前 P 的定时器 ===
  int64_t now = 0, poll_until = 0;
  bool ran_timer = false;
  CheckTimers(curp, 0, &now, &poll_until, &ran_timer);
  if (ran_timer) {
    // 运行定时器可能 Ready 了新协程，检查本地队列
    G* gp = curp->RunqGet(inherit_time);
    if (gp != nullptr) return gp;
  }

  // 本地运行队列
  G* gp = curp->RunqGet(inherit_time);
  if (gp != nullptr) return gp;

  // 全局运行队列、netpoll ...

  // === 改造：Work Stealing 时顺带检查其他 P 的定时器 ===
  for (int i = 0; i < 4; i++) {
    for (...) {
      P* p2 = Allp()[rand() % rtm_conf->MaxProcs()];
      if (p2 == curp) continue;
      gp = curp->RunqSteal(p2, steal_run_next);
      if (gp != nullptr) return gp;

      // 偷取时检查 p2 的定时器
      if (i > 2 || (i > 1 && ShouldStealTimers(p2))) {
        int64_t tnow, w;
        bool ran;
        CheckTimers(p2, now, &tnow, &w, &ran);
        now = tnow;
        if (ran) {
          gp = curp->RunqGet(inherit_time);
          if (gp != nullptr) return gp;
        }
      }
    }
  }
  // ... stop 逻辑 ...
}
```

#### 4.4.2 `ShouldStealTimers` 辅助函数

```cpp
// 只在 p2 不处于 Running 或已被标记抢占时偷取定时器，
// 减少对正在运行的 P 的锁竞争
bool ShouldStealTimers(P* p2) {
  if (p2->GetStatus() != kPrunning) return true;
  // tin 目前无 preempt 标记，简化为：Running 状态不偷
  return false;
}
```

> **说明**：Go 1.15 还检查 `gp.preempt`，tin 当前无抢占标记机制，可先简化为只在非 Running 时偷取。后续引入抢占后再完善。

#### 4.4.3 `ResizeProc` 中 P 销毁时迁移定时器

```cpp
// scheduler.cc - ResizeProc()

for (int i = nprocs; i < old; i++) {
  P* p = Allp()[i];
  // 现有：迁移运行队列
  while (1) {
    G* gp = p->RunqGet();
    if (gp == nullptr) break;
    GlobalRunqPutHead(gp);
  }

  // === 新增：迁移定时器到当前 P ===
  if (!p->Timers().empty()) {
    P* plocal = GetP();
    plocal->TimersLock().Lock();
    p->TimersLock().Lock();
    MoveTimers(plocal, p->Timers());
    p->Timers().clear();
    p->SetTimer0When(0);
    // 重置计数器
    p->IncNumTimers(-static_cast<int32_t>(p->NumTimers()));
    p->IncDeletedTimers(-static_cast<int32_t>(p->DeletedTimers()));
    p->IncAdjustTimers(-static_cast<int32_t>(p->AdjustTimers()));
    p->TimersLock().Unlock();
    plocal->TimersLock().Unlock();
  }

  p->SetStatus(kPdead);
}
```

#### 4.4.4 `SysMon` 改造为自适应睡眠

```cpp
// sysmon.cc

void SysMon() {
  int idle = 0;
  uint32_t delay = 0;
  while (!rtm_env->ExitFlag()) {
    if (idle == 0) {
      delay = 20;  // 20us
    } else if (idle > 50) {
      delay *= 2;
    }
    if (delay > 10000) delay = 10000;  // 上限 10ms
    absl::SleepFor(absl::Microseconds(delay));

    int64_t now = MonoNow();

    // === 新增：检查下一个定时器到期时间 ===
    P* timer_pp = nullptr;
    int64_t next = TimeSleepUntil(&timer_pp);

    // 网络轮询（保留现有逻辑）
    uint32_t last_poll = sched->LastPollTime();
    uint32_t now_ms = static_cast<uint32_t>(now / tin::kMillisecond);
    if (now_ms == 0) now_ms = 1;
    if (NetPollInited() && last_poll != 0 && (last_poll + 10 < now_ms)) {
      atomic::cas32(sched->MutableLastPollTime(), last_poll, now_ms);
      G* gp = NetPoll(false);
      if (gp != nullptr) {
        sched->InjectGList(gp);
      }
    }

    // 如果有更早的定时器，调整 idle 计数
    if (next != std::numeric_limits<int64_t>::max() && next > now) {
      idle = 0;  // 有定时器待触发，不要深度睡眠
    } else if (timer_pp != nullptr) {
      // 定时器已到期但可能没人运行，唤醒一个 P
      sched->WakeupP();
      idle = 0;
    }
  }
}
```

#### 4.4.5 `TimeSleepUntil` 实现

```cpp
int64_t TimeSleepUntil(P** out_pp) {
  int64_t next = std::numeric_limits<int64_t>::max();
  *out_pp = nullptr;

  // 加 allpLock 防止 allp 切片变化
  SchedulerLocker guard;  // 复用 sched->lock_

  int nprocs = rtm_conf->MaxProcs();
  for (int i = 0; i < nprocs; i++) {
    P* pp = Allp()[i];
    if (pp == nullptr) continue;

    uint32_t c = pp->AdjustTimers();
    if (c == 0) {
      uint64_t w = pp->Timer0When();
      if (w != 0 && static_cast<int64_t>(w) < next) {
        next = static_cast<int64_t>(w);
        *out_pp = pp;
      }
      continue;
    }

    // 有 adjustTimers，需加锁精确扫描
    pp->TimersLock().Lock();
    for (Timer* t : pp->Timers()) {
      uint32_t s = t->status.load(std::memory_order_acquire);
      switch (s) {
        case kTimerWaiting:
          if (t->when < next) next = t->when;
          break;
        case kTimerModifiedEarlier:
        case kTimerModifiedLater:
          if (t->nextwhen < next) next = t->nextwhen;
          if (s == kTimerModifiedEarlier) c--;
          break;
      }
      if (static_cast<int32_t>(c) <= 0) break;
    }
    pp->TimersLock().Unlock();
  }
  return next;
}
```

### 4.5 调用点迁移

#### 4.5.1 `InternalNanoSleep`

```cpp
// 改造前:
void InternalNanoSleep(int64_t ns) {
  G* gp = GetG();
  Timer* t = gp->GetTimer();
  t->when = MonoNow() + ns;
  t->f = WakeupSleeperFn;
  t->arg = gp;
  timer_q->Lock();
  timer_q->AddTimerLocked(t);
  Park(TimerQueue::UnlockQueue, timer_q, 0);
}

// 改造后:
void InternalNanoSleep(int64_t ns) {
  G* gp = GetG();
  Timer* t = gp->GetTimer();
  t->when = MonoNow() + ns;
  t->f = WakeupSleeperFn;
  t->arg = gp;
  AddTimer(t);  // 自动加入当前 P 的堆
  Park(ParkUnlockF, nullptr, nullptr);  // 简单挂起，等待 Ready 唤醒
}
```

> **注意**：`WakeupSleeperFn` 调用 `Ready(gp)`，将协程放入当前 P 的运行队列。由于 timer 回调在 `RunOneTimer` 中执行（在持有 `timersLock` 的 P 上），`Ready` 会把协程放入该 P 的 `runq`，随后被调度。

#### 4.5.2 `SemSetDeadline`

```cpp
// 改造前:
timer_q->AddTimer(timer);

// 改造后:
AddTimer(timer);  // 自由函数，加入当前 P
```

#### 4.5.3 `AddTimerRefCounted` / `DelTimerRefCounted`

```cpp
// 改造前:
void AddTimerRefCounted(PollDescriptor* pd, Timer* t) {
  pd->AddRef();
  timer_q->AddTimer(t);
}
void DelTimerRefCounted(PollDescriptor* pd, Timer* t) {
  if (timer_q->DelTimer(t))
    pd->Release();
}

// 改造后:
void AddTimerRefCounted(PollDescriptor* pd, Timer* t) {
  pd->AddRef();
  AddTimer(t);  // 自由函数
}
void DelTimerRefCounted(PollDescriptor* pd, Timer* t) {
  if (DelTimer(t))  // 无锁 CAS
    pd->Release();
}
```

### 4.6 全局 `timer_q` 清理

#### `env.h` 变更

```cpp
// 删除:
// class TimerQueue;
// std::unique_ptr<TimerQueue> timer_q_;
// extern TimerQueue* timer_q;

// 保留: Scheduler* sched（不受影响）
```

#### `env.cc` 变更

```cpp
// Initialize() 中删除:
// timer_q_ = std::make_unique<TimerQueue>();
// timer_q = timer_q_.get();

// Deinitialize() 中删除:
// if (timer_q_) timer_q_->Join();
// timer_q = nullptr;

// OnMainExit() 中删除:
// timer_q->Join();
```

### 4.7 删除 `TimerQueue` 类

移除以下内容（`timer_queue.h/cc`）：
- `class TimerQueue` 整体
- `TimerQueue::Proc()` 专用协程
- `TimerQueue::Join()`
- `TimerQueue::UnlockQueue()` 静态方法
- `wait_note_`、`gp_`、`created_`、`rescheduling_`、`sleeping_`、`exit_flag_`、`wait_group_` 字段

保留并改造为自由函数：
- `SiftUp` → `SiftUpTimer`
- `SiftDown` → `SiftDownTimer`
- 新增所有 per-P timer 操作函数

---

## 5. 实施计划

### Phase 1: 基础设施（不破坏现有功能）

| 步骤 | 内容 | 风险 |
|------|------|------|
| 1.1 | 在 `Timer` 结构体中新增 `pp`、`status`、`nextwhen` 字段 | 低，纯新增 |
| 1.2 | 在 `P` 类中新增 timer 字段 | 低，纯新增 |
| 1.3 | 实现所有 per-P timer 操作函数（自由函数），但不接入 | 低，死代码 |
| 1.4 | 实现 `MoveTimers`、`TimeSleepUntil` | 低 |

### Phase 2: 调度器集成（双轨运行期）

| 步骤 | 内容 | 风险 |
|------|------|------|
| 2.1 | `FindRunnable` 顶部增加 `CheckTimers(curp, ...)` | 中，需验证不与全局 TimerQueue 冲突 |
| 2.2 | Work Stealing 中增加跨 P `CheckTimers` | 中 |
| 2.3 | `ResizeProc` 中增加 `MoveTimers` | 中 |

> **双轨期注意**：Phase 2 期间 per-P 的 `CheckTimers` 会运行 per-P 堆中的 timer，但调用点（`InternalNanoSleep` 等）仍使用全局 `timer_q`。需确保两套系统不重叠操作同一个 `Timer` 对象。建议在 Phase 2 中 `CheckTimers` 先只做空操作（per-P 堆为空），仅验证集成路径不崩溃。

### Phase 3: 调用点迁移

| 步骤 | 内容 | 风险 |
|------|------|------|
| 3.1 | `InternalNanoSleep` 改用 `AddTimer` | **高**，Park 机制变更 |
| 3.2 | `SemSetDeadline` 改用 `AddTimer` | 高 |
| 3.3 | `AddTimerRefCounted`/`DelTimerRefCounted` 改用 per-P API | 高 |

### Phase 4: 清理

| 步骤 | 内容 | 风险 |
|------|------|------|
| 4.1 | `SysMon` 改用 `TimeSleepUntil` 自适应睡眠 | 中 |
| 4.2 | 删除全局 `timer_q`、`TimerQueue` 类 | 中 |
| 4.3 | 删除 `env.h/env.cc` 中 timer_q 相关代码 | 低 |
| 4.4 | 更新 `CMakeLists.txt` | 低 |

### Phase 5: 测试验证

| 步骤 | 内容 |
|------|------|
| 5.1 | `examples/simple` 和 `examples/echo` 回归测试 |
| 5.2 | 高频定时器压力测试（创建/删除/修改交替） |
| 5.3 | 多 P 下定时器均匀分布验证 |
| 5.4 | P 销毁/创建（`ResizeProc`）时定时器迁移验证 |
| 5.5 | Sysmon 自适应睡眠验证 |

---

## 6. 关键设计决策与注意事项

### 6.1 为什么保留 `timersLock` 而非完全无锁？

Go 1.15 的 per-P 模型**并非完全无锁**：
- **拥有该堆的 P**：加 `timersLock` 操作堆（`cleantimers`、`adjusttimers`、`runtimer`）
- **其他 P**：通过原子 CAS 修改 timer 的 `status`（`deltimer`、`modtimer`），**不加 `timersLock`**
- **Work Stealing 时**：其他 P 加被偷 P 的 `timersLock` 调用 `checkTimers`

这种设计将锁竞争限制在"堆的物理操作"上，而增删改的标记操作是无锁的。

### 6.2 `acquirem`/`releasem` 等价物

Go 在 `deltimer`/`modtimer` 中调用 `acquirem()` 禁止抢占，防止在 `timerModifying` 状态下被切走导致死锁。tin 需要等价机制：

- **方案 A（推荐）**：在 `Coroutine` 上增加 `preempt_disabled` 计数器，`DelTimer`/`ModTimer` 期间置位
- **方案 B（简化）**：由于 tin 当前无抢占，暂不处理，后续引入抢占时补充

### 6.3 `WakeNetPoller` 的作用

`AddTimer` 和 `ModTimer`（改早时）会调用 `WakeNetPoller(when)`。这是因为 tin 的 netpoller 在 `FindRunnable` 中会阻塞等待，如果新加入的定时器比当前 poll 的超时更早，需要中断 poll 让调度循环回来检查定时器。

tin 当前 netpoll 的阻塞等待在 `FindRunnable` 的 `stop` 分支中。改造后需在 `NetPoll` 阻塞前记录 `poll_until`，在 `WakeNetPoller` 中如果 `when < poll_until` 则中断阻塞。

### 6.4 Timer 回调中的重入安全

`RunOneTimer` 执行回调时临时释放 `timersLock`。回调中可能调用 `AddTimer`/`DelTimer`/`ModTimer`，这些操作会重新加锁。Go 通过 `runOneTimer` 的注释明确了这个时序。tin 需确保：
- 回调执行期间，`timersLock` 已释放
- 回调中 `AddTimer` 等操作能正常获取锁
- 回调返回后，`RunOneTimer` 重新获取锁继续循环

### 6.5 `GUintptr` 与裸指针规则

根据项目规则，`GUintptr`、`PollDescriptor` 等裸指针不修改。timer 的 `pp` 字段使用 `P*` 裸指针（与 Go 的 `puintptr` 一致），不涉及 lock-free 算法，无需改为 `PUintptr`。

### 6.6 CMake 构建注意事项

`timer_queue.cc` 仍保留在 `CMakeLists.txt` 中，但内容从 `TimerQueue` 类实现变为自由函数实现。`timer_queue.h` 同理。无需新增/删除文件条目。

---

## 7. Go 1.15 源码参考索引

| 功能 | 文件 | 行号 |
|------|------|------|
| `timer` 结构体 | `runtime/time.go` | 17-37 |
| Timer 状态常量 | `runtime/time.go` | 117-158 |
| `addtimer` | `runtime/time.go` | 244-264 |
| `doaddtimer` | `runtime/time.go` | 268-286 |
| `deltimer` | `runtime/time.go` | 292-352 |
| `dodeltimer` / `dodeltimer0` | `runtime/time.go` | 358-403 |
| `modtimer` | `runtime/time.go` | 408-521 |
| `resettimer` | `runtime/time.go` | 528-530 |
| `cleantimers` | `runtime/time.go` | 536-585 |
| `moveTimers` | `runtime/time.go` | 591-633 |
| `adjusttimers` | `runtime/time.go` | 640-706 |
| `addAdjustedTimers` | `runtime/time.go` | 710-717 |
| `nobarrierWakeTime` | `runtime/time.go` | 726-732 |
| `runtimer` | `runtime/time.go` | 741-804 |
| `runOneTimer` | `runtime/time.go` | 810-859 |
| `clearDeletedTimers` | `runtime/time.go` | 870-945 |
| `verifyTimerHeap` | `runtime/time.go` | 950-968 |
| `updateTimer0When` | `runtime/time.go` | 972-978 |
| `timeSleepUntil` | `runtime/time.go` | 983-1042 |
| `siftupTimer` | `runtime/time.go` | 1052-1069 |
| `siftdownTimer` | `runtime/time.go` | 1071-1109 |
| `p` 结构体 timer 字段 | `runtime/runtime2.go` | 661-685 |
| `checkTimers` | `runtime/proc.go` | 2734-2788 |
| `shouldStealTimers` | `runtime/proc.go` | 2794-2807 |
| `findrunnable` 中 checkTimers | `runtime/proc.go` | 2192, 2275 |
| `wakeNetPoller` | `runtime/proc.go` | 2499-2510 |
| P 销毁时 moveTimers | `runtime/proc.go` | 4231-4247 |
| `sysmon` 中 timeSleepUntil | `runtime/proc.go` | 4635, 4657, 4674 |
| `checkdead` 中 timer 检查 | `runtime/proc.go` | 4572, 4594-4598 |
| `retake` | `runtime/proc.go` | 4746-4813 |

---

## 8. 总结

将 tin 的全局 `TimerQueue` 改造为 per-P timer 堆，核心收益：

1. **消除全局锁瓶颈**：每个 P 独立操作自己的 `timersLock`
2. **无锁删除**：`DelTimer` 通过 CAS 标记，无需加锁
3. **消除专用协程**：定时器在调度循环中原地执行，减少唤醒延迟
4. **Work Stealing 一体化**：空闲 P 可运行其他 P 上已到期的定时器
5. **Sysmon 精确睡眠**：根据最早定时器到期时间自适应调整

改造遵循 Go 1.15 的成熟设计，同时适配 tin 的 C++ 运行时结构（`P`/`M`/`G` 模型、`RawMutex`、`Note` 等）。建议按 Phase 1→5 渐进实施，每阶段回归测试。
