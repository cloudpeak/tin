# Go 1.15 Runtime 升级实施计划

> 配套文档：`docs/go1.15-runtime-upgrade-features.md`（特性清单）
>
> 目标：将 tin 项目从 Go 1.6 runtime 基线逐步升级到 Go 1.15 runtime 水平
>
> 实施原则：
> - 每一步可独立验证、独立合并，避免大爆炸式重构
> - 优先做高性价比（中低难度 + 明确收益）的改动
> - 不迁移与 C++ 协程模型根本不兼容的特性（异步抢占 / 并发 GC / 栈复制 / open-coded defer）
> - 每步完成后跑 `examples/simple` 和 `examples/echo` 回归测试

---

## 阶段总览

| 阶段 | 主题 | 难度 | 预计步骤数 | 依赖 |
|------|------|------|----------|------|
| Phase 1 | 快速补齐字段与基础接口 | 低 | 4 | 无 |
| Phase 2 | sysmon retake 与 syscall affinity | 中 | 3 | Phase 1 |
| Phase 3 | netpoll 与 timer 协同 | 中 | 4 | Phase 1 |
| Phase 4 | sync.Mutex 饥饿模式 + sudog 池化 | 中 | 3 | Phase 1 |
| Phase 5 | work stealing 算法改进 | 中 | 2 | Phase 1 |
| Phase 6 | 调试与可观测性 | 低-中 | 3 | Phase 1 |
| Phase 7 | 可选高级特性 | 中-高 | 4 | 各阶段 |

---

## Phase 1：快速补齐字段与基础接口

**目标**：补齐 G/M/P 结构体中难度低、无依赖的字段，为后续阶段打基础。

**前置依赖**：无

**风险**：极低（纯字段添加，无行为变化）

### Step 1.1：G（Coroutine）补充基础字段

**修改文件**：
- `tin/runtime/coroutine.h`
- `tin/runtime/coroutine.cc`

**新增字段**：
```cpp
class Coroutine {
  // ... 现有字段 ...
  int64_t goid_;                 // goroutine ID（runtime2.go:428）
  int64_t waitsince_;            // 阻塞起始时间（runtime2.go:430）
  int32_t waitreason_;           // 阻塞原因枚举（runtime2.go:431）
  void* param_;                  // 唤醒时传递参数（runtime2.go:425）
};
```

**`waitreason` 枚举**（参考 `runtime2.go:979-1004`）：
```cpp
enum WaitReason {
  kWaitReasonZero = 0,
  kWaitReasonGCAssistWait,
  kWaitReasonIOWait,
  kWaitReasonChanReceive,
  kWaitReasonChanSend,
  kWaitReasonSelect,
  kWaitReasonMutex,
  kWaitReasonSleep,
  kWaitReasonTimer,
  // ... 仅实现 tin 已有的等待场景
};
```

**`goid_` 分配策略**：
- 全局 `std::atomic<int64_t> next_goid_{1}`
- 在 `Coroutine` 构造时 `next_goid_.fetch_add(1, memory_order_relaxed)`
- 后续 Step 6.1 会改为 per-P 批量分配（`goidcache`）

**Go 1.15 参考**：`runtime2.go:425-431, 428, 979-1004`

**验证**：
- 编译通过
- `examples/simple` 正常运行
- `examples/echo` 5 次循环 + 关闭不崩溃

### Step 1.2：P 补充 syscalltick / sysmontick 字段

**修改文件**：
- `tin/runtime/p.h`
- `tin/runtime/p.cc`

**新增字段**：
```cpp
class P {
  // ... 现有字段 ...
  uint32_t syscalltick_;    // 每次 EnterSyscall 时 ++（runtime2.go:571）
  uint32_t sysmontick_;     // sysmon 上次观察的 syscalltick（runtime2.go:572）
};
```

**修改点**：
- `P::P(int id)` 构造函数初始化为 0
- `EnterSyscall()` / `EnterSyscallBlock()` 中 `syscalltick_++`

**Go 1.15 参考**：`runtime2.go:571-572`, `proc.go:3035-3052`

**验证**：编译通过 + 回归测试

### Step 1.3：M 补基础字段

**修改文件**：
- `tin/runtime/m.h`
- `tin/runtime/m.cc`

**新增字段**：
```cpp
class M {
  // ... 现有字段 ...
  int32_t locks_;           // 持锁计数（runtime2.go:505）
  int32_t mallocing_;       // 分配中标记（runtime2.go:502）
  std::string preemptoff_;  // 禁止抢占字符串（runtime2.go:504），空字符串表示允许
  P* oldp_;                 // syscall 前的 P（runtime2.go:500）
  uint32_t fastrand_;       // 快速随机数状态（runtime2.go:514）
};
```

**修改点**：
- `M::M()` 初始化全部为 0 / 空
- 提供 `IncLocks()/DecLocks()/IsMAllocating()` 等访问器
- 在 `RawMutex::Lock()/Unlock()` 调用 `IncLocks()/DecLocks()`（仅 debug 模式）
- `M::Fastrand()` 实现 Go 的 `fastrand` 算法（`runtime2.go:514` 注释），替代 `rand()`

**`fastrand` 算法**（参考 Go 1.15 `runtime2.go:514`）：
```cpp
uint32_t M::Fastrand() {
  uint32_t x = fastrand_;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  fastrand_ = x;
  return x;
}
```

**Go 1.15 参考**：`runtime2.go:490-535, 514`

**验证**：编译通过 + 回归测试

### Step 1.4：64 位原子 + 位操作

**修改文件**：
- `tin/sync/atomic.h`

**新增 API**：
```cpp
namespace tin::atomic {
  // 64 位原子（参考 Go 1.15 atomic_amd64.go:39-92）
  int64_t load64(const volatile int64_t* p);
  void store64(volatile int64_t* p, int64_t v);
  int64_t exchange64(volatile int64_t* p, int64_t v);
  int64_t inc64(volatile int64_t* p, int64_t delta);
  bool cas64(volatile int64_t* p, int64_t old, int64_t neu);

  // 8 位位操作（参考 atomic_amd64.go:63-66）
  void and8(volatile uint8_t* p, uint8_t v);
  void or8(volatile uint8_t* p, uint8_t v);
}
```

**实现**：基于 `std::atomic<int64_t>` / `std::atomic<uint8_t>::fetch_and/fetch_or`

**Go 1.15 参考**：`internal/atomic/atomic_amd64.go:39-92`

**验证**：写单元测试验证原子语义正确

---

## Phase 2：sysmon retake 与 syscall affinity

**目标**：实现 Go 1.15 的 `retake` 机制，让 sysmon 能从长时间 syscall 中抢回 P；同时引入 `oldp` 字段恢复 syscall affinity。

**前置依赖**：Phase 1（Step 1.2 `syscalltick` / Step 1.3 `oldp`）

**风险**：中（涉及 syscall 路径改造，需仔细测试）

### Step 2.1：ExitSyscall 路径引入 oldp

**修改文件**：
- `tin/runtime/scheduler.cc`（`ExitSyscallFast`、`ExitSyscall0`）
- `tin/runtime/scheduler.h`

**当前 tin 行为**（`scheduler.cc:639-659`）：
- `ExitSyscallFast` 尝试 CAS 重获原 P，失败则 `ExitSyscall0`
- `ExitSyscall0` 尝试获取任一 idle P，失败则 park G + 释放 M

**Go 1.15 改造**（`proc.go:3035-3090`）：
```cpp
bool Scheduler::ExitSyscallFast() {
  M* curm = GetG()->M();
  P* oldp = curm->P();          // 记录原 P
  curm->set_oldp(oldp);          // Go 1.9+: 保存 oldp
  // ... 原 CAS 逻辑 ...
}

void Scheduler::ExitSyscall0() {
  M* curm = GetG()->M();
  P* oldp = curm->oldp();       // 用 oldp 而非 m->P()
  // ... 在 handoffp 决策中使用 oldp 的本地 runq 状态 ...
  curm->set_oldp(nullptr);
}
```

**收益**：syscall 返回后能基于原 P 的本地 runq 状态做更准确的 handoff 决策

**Go 1.15 参考**：`proc.go:3035-3090`, `runtime2.go:500`

**验证**：
- `examples/echo` 长连接场景，多个客户端并发时不出现 P 卡在 syscall

### Step 2.2：sysmon retake 实现

**修改文件**：
- `tin/runtime/sysmon.cc`
- `tin/runtime/sysmon.h`
- `tin/runtime/scheduler.h`（暴露 `Retake` 接口）

**实现 `Retake` 函数**（参考 `proc.go:4746-4813`）：
```cpp
namespace {
uint32_t Retake(int64_t now) {
  uint32_t n = 0;
  int nprocs = rtm_conf->MaxProcs();
  for (int i = 0; i < nprocs; i++) {
    P* p = sched->AllpPublic()[i];
    if (p == nullptr) continue;

    uint32_t s = p->GetStatus();
    if (s == kPsyscall) {
      // P 在 syscall 中：检查停留时间
      uint32_t t = p->SyscallTick();
      if (p->SysmonTick() == t) {
        // 自上次观察以来 syscalltick 未变 → 仍在同一 syscall
        // 但只有 runq 空 + 有 idle/spinning 时才 handoff
        // 否则强制 handoff（超过 10ms）
        // ... Go: pd.syscallwhen + 10ms > now 则跳过 ...
      }
      p->SetSysmonTick(t);

      // CAS Psyscall → Pidle
      if (p->CasStatus(kPsyscall, kPidle)) {
        n++;
        // 把 P 交给新 M
        sched->HandoffP(p);
      }
    } else if (s == kPrunning) {
      // P 在运行：长时间未调度切换则抢占（仅同步抢占，tin 不支持异步）
      // tin 不实现 preemptone，跳过此分支
    }
  }
  return n;
}
}  // namespace
```

**sysmon 主循环加入 retake**：
```cpp
void SysMon() {
  // ... 现有自适应 sleep + netpoll + timer 监控 ...
  uint32_t retaken = Retake(now);
  if (retaken > 0) {
    idle = 0;  // 重置 idle 计数
  }
}
```

**注意**：
- tin 无 `preemptone`（同步抢占），所以 `_Prunning` 分支跳过
- 仅实现 `_Psyscall` 的 handoff，这是性价比最高的部分
- `syscallwhen` 字段可暂不实现，用 10ms 固定阈值替代

**Go 1.15 参考**：`proc.go:4746-4813`

**验证**：
- 写一个测试：协程在 `read` 阻塞超过 20ms 后，sysmon 能 handoff P
- `examples/echo` 大量慢客户端连接场景，吞吐不下降

### Step 2.3：HandoffP 完善对齐 Go 1.15

**修改文件**：
- `tin/runtime/scheduler.cc`（`HandoffP` 函数）

**当前 tin `HandoffP`**（`scheduler.cc:582-612`）：
- 3 个分支：本地 work → CAS spinning 启动 → sched.lock 内检查全局 work / 最后一个 P

**Go 1.15 改造**（`proc.go:1987-2043`）：
- 增加 syscalltick 检查：若 P 在 syscall 且 runq 非空，立即 handoff
- 增加 GC work 检查（tin 无 GC，可跳过）

**修改点**：
```cpp
bool Scheduler::HandoffP(P* p) {
  // 1. 本地 runq 非空 → 立即 handoff
  if (!p->RunqEmpty()) { /* startm */ return true; }
  // 2. (tin 跳过 GC work 检查)
  // 3. 无 spinning 且无 idle P → 启动 spinning M
  if (atomic::relaxed_load32(&nr_spinning_) == 0 &&
      atomic::relaxed_load32(&nr_idlep_) == 0) {
    if (cas32(&nr_spinning_, 0, 1)) {
      StartM(p, true);
      return true;
    }
  }
  // 4. sched.lock 内检查全局 work / poll / 最后一个 P
  // ... 保持现有逻辑 ...
}
```

**Go 1.15 参考**：`proc.go:1987-2043`

**验证**：`examples/echo` 回归测试

---

## Phase 3：netpoll 与 timer 协同

**目标**：让 netpoll 接受超时参数，与 timer 协同，避免 M 空转；同时实现 `netpollBreak` 唤醒机制。

**前置依赖**：Phase 1

**风险**：中-高（涉及调度器主循环和 netpoll 核心接口）

### Step 3.1：netpoll 接口改造为接受超时

**修改文件**：
- `tin/runtime/net/netpoll.h`
- `tin/runtime/net/netpoll.cc`
- `tin/runtime/net/netpoll_epoll.cc`
- `tin/runtime/net/netpoll_kqueue.cc`
- `tin/runtime/net/netpoll_windows.cc`

**接口改造**：
```cpp
// 旧
G* NetPoll(bool block);
// 新（参考 Go 1.15 netpoll_epoll.go:106）
G* NetPoll(int64_t delay_ns);  // -1 = 无限阻塞, 0 = 非阻塞, >0 = 阻塞最多 delay_ns
```

**epoll 实现改造**（`netpoll_epoll.cc`）：
```cpp
G* NetPoll(int64_t delay_ns) {
  int waitms;
  if (delay_ns < 0) waitms = -1;
  else if (delay_ns == 0) waitms = 0;
  else {
    waitms = static_cast<int>(delay_ns / 1000000);  // ns → ms
    if (waitms == 0) waitms = 1;
  }
  // ... epoll_wait(..., waitms) ...
}
```

**保持向后兼容**：
- 旧调用点改为 `NetPoll(0)` 或 `NetPoll(-1)` 或 `NetPoll(delta_ns)`

**Go 1.15 参考**：`netpoll_epoll.go:106-123`

**验证**：编译通过 + 回归测试（行为应与改造前一致）

### Step 3.2：netpollBreak 唤醒管道

**修改文件**：
- `tin/runtime/net/netpoll_epoll.cc`（Linux）
- `tin/runtime/net/netpoll_kqueue.cc`（macOS，可选）
- `tin/runtime/net/netpoll_windows.cc`（Windows，用 eventfd 或 pipe）

**实现**（参考 `netpoll_epoll.go:42-58, 81-99`）：
```cpp
// 全局
static int g_netpoll_break_rd = -1;
static int g_netpoll_break_wr = -1;

void NetPollInit() {
  // ... 原 epoll_create ...
  // 创建 break 管道
  int pipefd[2];
  if (pipe(pipefd) != 0) { /* fallback: socketpair */ }
  g_netpoll_break_rd = pipefd[0];
  g_netpoll_break_wr = pipefd[1];
  // 加入 epoll 监听
  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.ptr = kNetpollBreak;  // 特殊标记
  epoll_ctl(g_epfd, EPOLL_CTL_ADD, g_netpoll_break_rd, &ev);
}

void NetPollBreak() {
  // 唤醒阻塞中的 epoll_wait
  char c = 0;
  write(g_netpoll_break_wr, &c, 1);
}

G* NetPoll(int64_t delay_ns) {
  // ... epoll_wait ...
  for (int i = 0; i < n; i++) {
    if (events[i].data.ptr == kNetpollBreak) {
      // 读空管道
      char buf[16];
      read(g_netpoll_break_rd, buf, sizeof(buf));
      continue;  // 跳过 break 事件
    }
    // ... 处理正常事件 ...
  }
}
```

**调用点**：
- `WakeNetPoller(int64_t when)` 在 timer 添加时调用 `NetPollBreak()`（替代当前的 `WakePIfNecessary`）
- `NetPollShutdown()` 调用 `NetPollBreak()` 唤醒阻塞 netpoll

**Go 1.15 参考**：`netpoll_epoll.go:42-58, 81-99`

**验证**：
- 添加 timer 时 netpoll 立即唤醒（用 strace 验证 epoll_wait 立即返回）
- `examples/echo` 关闭时不再有 M 卡在 epoll_wait

### Step 3.3：pollUntil 传递给 netpoll

**修改文件**：
- `tin/runtime/scheduler.cc`（`FindRunnable`）
- `tin/runtime/timer/timer_queue.h`（`CheckTimers` 返回值已有 `poll_until`）

**当前 tin `FindRunnable`**（`scheduler.cc:259-424`）：
- `CheckTimers` 已经返回 `poll_until`，但未传给 `NetPoll`

**Go 1.15 改造**（`proc.go:2317-2435`）：
```cpp
G* Scheduler::FindRunnable(bool* inherit_time) {
  // ... 顶部 CheckTimers ...
  int64_t now = 0;
  int64_t poll_until = 0;
  bool ran_timer = false;
  CheckTimers(curp, 0, &now, &poll_until, &ran_timer);
  // ... 现有 runq / 全局队列 / spinning 检查 ...

  // 阻塞 netpoll 时使用 poll_until
  if (NetPollInited() && /* ... */) {
    int64_t delta = 0;
    if (poll_until != 0) {
      delta = poll_until - now;
      if (delta < 0) delta = 0;
    }
    G* gp = NetPoll(delta);  // 替代原来的 NetPoll(true)
    // ...
  }
}
```

**注意**：需要把 `poll_until` / `now` 从顶部的 `CheckTimers` 块作用域提到 `FindRunnable` 函数作用域，让阻塞 netpoll 分支能访问

**Go 1.15 参考**：`proc.go:2192, 2317-2435`

**验证**：
- timer 到期前 netpoll 阻塞时间不超过 delta（用日志验证）
- `examples/echo` 长连接 idle 场景，CPU 占用降低

### Step 3.4：netpollWaiters 计数与调度决策

**修改文件**：
- `tin/runtime/net/netpoll.h`
- `tin/runtime/net/netpoll.cc`（`NetPollBlock` / `NetPollBlockCommit` / `NetPollUnblock`）
- `tin/runtime/scheduler.cc`（`FindRunnable` 阻塞 netpoll 条件）

**新增全局**：
```cpp
namespace tin::runtime {
extern std::atomic<uint32_t> g_netpoll_waiters;
}
```

**修改点**：
- `NetPollBlockCommit` 中 `g_netpoll_waiters.fetch_add(1, memory_order_relaxed)`
- `NetPollReady` / `NetPollUnblock` 中 `g_netpoll_waiters.fetch_sub(1, memory_order_relaxed)`
- `FindRunnable` 阻塞 netpoll 条件改为：
  ```cpp
  if (NetPollInited() &&
      (atomic::relaxed_load32(&g_netpoll_waiters) > 0 || poll_until != 0)) {
    // 阻塞 netpoll
  }
  ```

**Go 1.15 参考**：`netpoll.go:109, 399, 405`, `proc.go:2277-2280`

**验证**：`examples/echo` 回归测试

---

## Phase 4：sync.Mutex 饥饿模式 + sudog 池化

**目标**：引入 Go 1.9 sync.Mutex 饥饿模式；实现 per-P sudogcache 减少 malloc。

**前置依赖**：Phase 1

**风险**：中（Mutex 改造需仔细测试并发正确性）

### Step 4.1：sync.Mutex 饥饿模式

**修改文件**：
- `tin/sync/mutex.h`
- `tin/sync/mutex.cc`

**新增状态位**（参考 Go 1.9 `sync/mutex.go`）：
```cpp
class Mutex {
  static constexpr int32_t kMutexStarving = 1 << 3;  // 饥饿模式
  static constexpr int32_t kMutexWaiterShift = 4;
  static constexpr int64_t kStarvationThresholdNs = 1000000;  // 1ms

  int32_t state_;
  uint32_t sema_;
  int64_t queue_start_time_;  // 队首 waiter 入队时间（仅饥饿模式维护）
};
```

**Lock 算法**（参考 `sync/mutex.go:Lock()`）：
```cpp
void Mutex::Lock() {
  // 快速路径：CAS 0 → Locked
  if (atomic::cas32(&state_, 0, kMutexLocked)) return;

  LockSlow();  // 包含 normal/starving 双模式
}

void Mutex::LockSlow() {
  bool starved = false;
  int64_t wait_start = 0;
  int iter = 0;
  int32_t old = atomic::load32(&state_);
  while (true) {
    // normal mode spin
    if (old & kMutexLocked && CanSpin(iter) && /* woken bit */) {
      DoSpin();
      iter++;
      old = atomic::load32(&state_);
      continue;
    }
    // ... 计算 new state，加入 waiter ...
    if (atomic::cas32(&state_, old, new)) {
      if (starved) {
        // 饥饿模式：直接 handoff，不排队
      }
      wait_start = MonoNow();
      SemAcquire(&sema_);
      // 唤醒后判断是否进入饥饿模式
      if (old&kMutexLocked == 0 &&
          (queue_start_time_ != 0 &&
           MonoNow() - queue_start_time_ > kStarvationThresholdNs)) {
        starved = true;
      }
      // ...
    }
    old = atomic::load32(&state_);
  }
}
```

**Unlock 算法**（参考 `sync/mutex.go:Unlock()`）：
- normal mode：唤醒队首 waiter
- starving mode：直接 handoff（`SemRelease` 后等待被唤醒者拿到锁）

**Go 1.9 参考**：`sync/mutex.go`（Go 1.15 版本）

**验证**：
- 写并发测试：100 个协程争抢同一 Mutex，无饥饿
- `examples/echo` 回归测试

### Step 4.2：per-P sudogcache

**修改文件**：
- `tin/runtime/p.h`（新增 `sudogcache_` 字段）
- `tin/runtime/p.cc`
- `tin/runtime/semaphore.cc`（`acquireSudog` / `releaseSudog`）

**P 新增字段**（参考 `runtime2.go:606-607`）：
```cpp
class P {
  // ... 现有字段 ...
  static constexpr int kSudogCacheSize = 128;
  std::vector<Sudog*> sudogcache_;  // per-P sudog 缓存
  RawMutex sudogcache_lock_;         // 仅溢出时用
};
```

**`acquireSudog` / `releaseSudog`**（参考 `proc.go:6580-6640`）：
```cpp
Sudog* AcquireSudog() {
  P* p = GetP();
  if (!p->sudogcache_.empty()) {
    Sudog* s = p->sudogcache_.back();
    p->sudogcache_.pop_back();
    return s;
  }
  // 全局池 fallback（可选）
  return new Sudog();
}

void ReleaseSudog(Sudog* s) {
  // 清空字段
  s->gp = nullptr;
  s->next = nullptr;
  // ...
  P* p = GetP();
  if (p->sudogcache_.size() < P::kSudogCacheSize) {
    p->sudogcache_.push_back(s);
  } else {
    delete s;  // 或转到全局池
  }
}
```

**`semaphore.cc` 修改**：
- 把所有 `new Sudog` 替换为 `AcquireSudog()`
- 把所有 `delete s` 替换为 `ReleaseSudog(s)`

**Go 1.15 参考**：`runtime2.go:606-607`, `proc.go:6580-6640`

**验证**：
- 高频 channel 操作场景下 malloc 次数减少（用 malloc hook 统计）
- `examples/echo` 回归测试

### Step 4.3：semaphore handoff 机制（可选）

**修改文件**：
- `tin/runtime/semaphore.cc`

**实现 `goyield`**（参考 `sema.go:204-211`）：
```cpp
namespace {
void GoYield() {
  // 让出 P 但把当前 G 放到本地 runq 头部
  G* gp = GetG();
  P* p = gp->M()->P();
  // 标记 G 为 runnable，重新入队，调用 Sched()
  gp->SetState(CoroutineState::kRunnable);
  p->RunqPut(gp, true);  // next=true，下一轮立即执行
  Sched();
}
}

// SemRelease 增加 handoff 参数
void SemRelease(uint32_t* addr, bool handoff) {
  // ... 现有逻辑 ...
  if (handoff && CanSemAcquire(addr)) {
    s->ticket = 1;
  }
  Ready(s->gp);
  if (s->ticket == 1 && GetG()->M()->locks() == 0) {
    GoYield();  // 直接 P 转交
  }
}
```

**注意**：handoff 仅在 Mutex 饥饿模式启用，普通 semrelease 用 `handoff=false`

**Go 1.15 参考**：`sema.go:191-213`

**验证**：Mutex 饥饿模式 + handoff 协同测试

---

## Phase 5：work stealing 算法改进

**目标**：把 `rand()%MaxProcs` 改为 Go 1.15 的 `stealOrder` 确定性遍历，提高 work stealing 覆盖度。

**前置依赖**：Phase 1

**风险**：中（调度核心算法，需仔细测试）

### Step 5.1：实现 stealOrder

**修改文件**：
- `tin/runtime/scheduler.cc`（`FindRunnable`）

**新增 `stealOrder` 类型**（参考 `proc.go:2289-2336`）：
```cpp
namespace {
struct StealOrder {
  static constexpr int kStealRandomOffset = 1;
  uint32_t count;     // = MaxProcs
  uint32_t position;  // 当前位置
  uint32_t offset;    // 起始偏移
  bool random;        // 是否随机模式

  void Start(uint32_t procs, uint32_t fastrand) {
    if (procs > 1) {
      count = procs;
      offset = fastrand % procs;
      random = (offset == 0);  // offset==0 时退化为顺序遍历
      position = 0;
    }
  }

  uint32_t Next() {
    uint32_t i = position + offset;
    position++;
    if (random) {
      // 使用伪随机：i = (i * 2654435761) % count
    }
    return i % count;
  }

  bool Done() const { return position >= count; }
};
}
```

**`FindRunnable` steal 循环改造**（参考 `proc.go:2336-2373`）：
```cpp
// 旧
for (int i = 0; i < 4 * MaxProcs; i++) {
  P* p = Allp()[rand() % MaxProcs];
  // ...
}

// 新
for (int round = 0; round < 4; round++) {
  StealOrder so;
  so.Start(MaxProcs, fastrand);
  while (!so.Done()) {
    P* p = Allp()[so.Next()];
    if (p == curp) continue;
    bool steal_run_next = round > 2;
    G* gp = curp->RunqSteal(p, steal_run_next);
    if (gp == nullptr && ShouldStealTimers(p)) {
      // 顺带 checkTimers
      CheckTimers(p, 0, &tnow, &w, &ran);
      // ...
    }
    if (gp != nullptr) return gp;
  }
}
```

**收益**：
- 4 轮 × 遍历所有 P，覆盖度 100%（避免重复访问同一 P）
- 与 Go 1.15 行为一致

**Go 1.15 参考**：`proc.go:2289-2336, 2336-2373`

**验证**：
- 写测试：4 P 场景，1 P 有 work，统计 steal 命中率
- `examples/echo` 回归测试

### Step 5.2：优化 ShouldStealTimers 触发条件

**修改文件**：
- `tin/runtime/scheduler.cc`

**当前 tin**（`scheduler.cc:29-32`）：
```cpp
bool ShouldStealTimers(P* p2) {
  return p2->GetStatus() != kPrunning;
}
```

**Go 1.15 改造**（`proc.go:2361`）：
```cpp
bool ShouldStealTimers(P* p2) {
  return p2->GetStatus() != kPrunning;  // 与现有相同
}

// 在 steal 循环中：
// 仅 round > 1 时才偷 timer（避免过早偷 timer 浪费 CPU）
if (gp == nullptr && (round > 2 || (round > 1 && ShouldStealTimers(p)))) {
  CheckTimers(p, 0, &tnow, &w, &ran);
  // ...
}
```

**Go 1.15 参考**：`proc.go:2361`

**验证**：`examples/echo` 回归测试

---

## Phase 6：调试与可观测性

**目标**：补齐 Go 1.15 的调试可观测性功能，便于线上诊断。

**前置依赖**：Phase 1

**风险**：低

### Step 6.1：goid 批量分配（goidcache）

**修改文件**：
- `tin/runtime/p.h`（新增 `goidcache_` / `goidcacheend_`）
- `tin/runtime/coroutine.cc`（构造时从 P 取 goid）

**P 新增字段**（参考 `runtime2.go:582-583`）：
```cpp
class P {
  uint64_t goidcache_;      // 当前可用 goid
  uint64_t goidcacheend_;   // 上限
};
```

**`AllocGoid` 函数**（参考 `proc.go:3403-3413`）：
```cpp
uint64_t P::AllocGoid() {
  if (goidcache_ < goidcacheend_) {
    return goidcache_++;
  }
  // 批量从全局取
  constexpr uint64_t kGoidBatch = 16;
  std::lock_guard<std::mutex> lock(g_global_goid_lock);
  goidcache_ = g_next_goid;
  goidcacheend_ = g_next_goid + kGoidBatch;
  g_next_goid = goidcacheend_;
  return goidcache_++;
}
```

**Coroutine 构造**：
```cpp
Coroutine::Coroutine(...) {
  goid_ = GetP()->AllocGoid();
}
```

**收益**：减少全局原子操作

**Go 1.15 参考**：`runtime2.go:582-583`, `proc.go:3403-3413`

**验证**：goid 单调递增，无重复

### Step 6.2：checkdead 死锁检测

**修改文件**：
- `tin/runtime/sysmon.cc`

**实现 `CheckDead`**（参考 `proc.go:4503-4597`）：
```cpp
namespace {
void CheckDead() {
  // 所有 G 都在等待，无 G 可运行 → 死锁
  // 1. 排除 main goroutine（tin 有 G0）
  // 2. 排除 system goroutine（sysmon / scavenger 等）
  // 3. 若有 work.workFull 等待则不算死锁
  // 4. 若 netpoll 有 pending 事件则不算死锁
  if (/* 全部 G 都 waiting */ && /* 无 netpoll pending */) {
    LOG(FATAL) << "all goroutines are asleep - deadlock!";
  }
}
}  // namespace

// 在 sysmon 主循环加入
void SysMon() {
  // ...
  if (/* 长时间无调度 */) {
    CheckDead();
  }
}
```

**注意**：tin 的 G0 模型与 Go 不同，需仔细判断哪些 G 算"用户 G"

**Go 1.15 参考**：`proc.go:4503-4597`

**验证**：写一个 `sync.Mutex` 自锁测试，应触发死锁日志

### Step 6.3：schedtrace 调试输出

**修改文件**：
- `tin/runtime/scheduler.h`
- `tin/runtime/scheduler.cc`
- `tin/runtime/env.h`（新增 `schedtrace` 配置）

**实现**（参考 `proc.go:4875+`）：
```cpp
void Scheduler::SchedTrace(bool detailed) {
  LOG(INFO) << "SCHED " << MonoNow() << "ms: m=" << mcount_
            << " p=" << rtm_conf->MaxProcs()
            << " idlep=" << nr_idlep_
            << " spinning=" << nr_spinning_
            << " needspinning=" << NeedSpinning()
            << " runnable=" << runq_size_;
  if (detailed) {
    for (int i = 0; i < rtm_conf->MaxProcs(); i++) {
      P* p = allp_[i];
      if (p == nullptr) continue;
      LOG(INFO) << "  P" << i << " status=" << p->GetStatus()
                << " schedtick=" << p->SchedTick()
                << " syscalltick=" << p->SyscallTick()
                << " runqsize=" << p->RunqSize()
                << " timers=" << p->NumTimers()
                << " deletedTimers=" << p->DeletedTimers();
    }
  }
}

// sysmon 每 100ms 调用一次（受 SCHEDTRACE 环境变量控制）
```

**环境变量**：
- `TIN_SCHEDTRACE=1000` → 每 1000ms 输出一次
- `TIN_SCHEDDETAIL=1` → 详细模式

**Go 1.15 参考**：`proc.go:4875+`

**验证**：运行 echo server，设置 `TIN_SCHEDTRACE=1000`，应看到周期性输出

---

## Phase 7：可选高级特性

**目标**：根据实际需求选择性实现的高级特性，每项可独立评估。

### Step 7.1：scavenge（独立于 GC）

**目标**：用 `madvise(MADV_DONTNEED)` 归还空闲页给 OS，降低 RSS 峰值。

**修改文件**：
- 新增 `tin/runtime/mem/scavenge.cc`
- `tin/runtime/sysmon.cc`（加入 wakeScavenger）

**实现要点**（参考 `mgcscavenge.go`）：
- 不需要完整 GC，仅依赖一个"已释放但未归还"的页列表
- 由 `FixedSizeStack` / `ProtectedFixedSizeStack` 析构时把页加入列表
- 后台 goroutine 定期 `madvise(MADV_DONTNEED)` 归还

**收益**：协程频繁创建/销毁场景下 RSS 不持续增长

**Go 1.15 参考**：`mgcscavenge.go`, `mem_linux.go`

**难度**：中

### Step 7.2：stackpool 小栈缓存

**目标**：减少协程栈的 malloc/free 开销。

**修改文件**：
- 新增 `tin/runtime/stack/stackpool.cc`
- `tin/runtime/stack/fixedsize_stack.cc`（改造为从 stackpool 取）

**实现要点**（参考 `stack.go:141-156`）：
- 4 个 size class：2KB / 4KB / 8KB / 16KB
- 每 P 一个 `stackcache[4]` 链表
- 分配：先查 P 的 cache，miss 则从全局 pool 取，再 miss 则 malloc
- 释放：归还到 P 的 cache，溢出半数则转入全局 pool

**收益**：高频 spawn/exit 协程场景减少 malloc

**Go 1.15 参考**：`stack.go:141-156, 327-419`

**难度**：低-中

### Step 7.3：rseq / wseq 分离

**目标**：netpoll 的 `PollDescriptor.seq` 拆分为 `rseq` / `wseq`，分别保护 read/write timer，避免一方重置 timer 时误失效另一方。

**修改文件**：
- `tin/runtime/net/poll_descriptor.h`
- `tin/runtime/net/pollops.cc`

**实现要点**（参考 `netpoll.go:77, 79, 279, 295`）：
```cpp
struct PollDescriptor {
  // ...
  uintptr_t rseq;  // 替代原 seq，仅保护 rt
  uintptr_t wseq;  // 仅保护 wt
  // ...
};

void SetDeadline(PollDescriptor* pd, int64_t d, int mode) {
  pd->lock.Lock();
  pd->rseq++;  // 仅当 mode 包含 'r'
  pd->wseq++;  // 仅当 mode 包含 'w'
  // ...
}
```

**收益**：修复潜在 timer 误失效 bug

**Go 1.15 参考**：`netpoll.go:77, 79, 279, 295`

**难度**：中

### Step 7.4：lockRank 死锁检测

**目标**：debug 模式下检测锁顺序违反，预防死锁。

**修改文件**：
- 新增 `tin/runtime/lockrank.h`
- `tin/runtime/raw_mutex.h`（debug 模式加 rank 检查）
- `tin/runtime/m.h`（新增 `locksHeld[10]`）

**实现要点**（参考 `lockrank.go`, `runtime2.go:562-563`）：
- 枚举所有 runtime 锁的 rank
- `LockWithRank` 在 debug 模式检查当前 M 的 `locksHeld[]` 是否有更高 rank 的锁
- 违反则 `LOG(FATAL) << "lock rank violation"`

**收益**：开发阶段预防 runtime 内部死锁

**Go 1.15 参考**：`lockrank.go`, `runtime2.go:562-563`, `lock_futex.go:lockWithRank`

**难度**：中

---

## 验证与回归测试

### 每步必做的回归测试

```bash
# 1. 构建
wsl bash -c "cd /mnt/d/home/dev/code/ai/1/tin && ./build-echo.sh"

# 2. simple 示例（注意：pre-existing 的 Notify bug 不算回归）
wsl bash -c "cd /mnt/d/home/dev/code/ai/1/tin && ./build/bin/simple"

# 3. echo 示例（5 次循环 + 关闭）
wsl bash -c "cd /mnt/d/home/dev/code/ai/1/tin && ./build/bin/echo > /tmp/echo.log 2>&1 &"
# Python 客户端测试 5 次循环
# SIGTERM 关闭，检查 echo.log 无 FATAL/invalid timer status
```

### 性能基准（可选）

- `examples/echo` 大量短连接吞吐测试
- `examples/echo` 长连接 echo 延迟测试
- 高频 timer 创建/删除场景
- 高频 channel 操作场景（sudog 池化前后对比）

---

## 附录 A：不迁移的特性清单与理由

| 特性 | Go 1.15 版本 | 不迁移理由 |
|------|------------|----------|
| 异步抢占（async preemption） | 1.14 | 依赖编译器 prologue 栈检查 + 信号栈 + 汇编寄存器 spill，C++ 协程库无法复用 |
| 并发标记-清除 GC | 1.5+ | C++ 无运行时类型信息，无编译器指针写插桩，无法做类型精确 GC |
| 混合写屏障 | 1.8 | 同上，依赖 GC 与编译器插桩 |
| 栈复制（copystack） | 1.4+ | C++ 栈有 C frame 和不可移动对象，无法重定位指针 |
| open-coded defer | 1.14 | 依赖 SSA funcdata + bitmask，C++ 无对应机制 |
| 经典 defer / panic / recover | 1.0+ | 语言层特性，C++ 无 `defer` 关键字，RAII 已部分替代 |
| Go 风格 hmap | 1.0+ | C++ 有 `std::unordered_map`、`absl::flat_hash_map` 可用 |
| Go 风格 channel（hchan + sudog 等待队列） | 1.0+ | tin 是 C++ 模板风格，重构等于重写 |
| Go select | 1.0+ | 强依赖 channel 重构 + 编译器配合 |
| `pageAlloc` 基数树 | 1.12 | ~1500 行，仅在大堆场景才有收益，tin 用 std::malloc 足够 |
| `_Gscan` 状态位 | 1.5+ | 依赖 GC scan，无 GC 无意义 |
| `_Pgcstop` P 状态 | 1.5+ | 依赖 STW GC，无 GC 无意义 |
| `gcBgMarkWorker` / `gcw` / `wbBuf` | 1.5+ | 依赖完整并发 GC |
| `gsignal` / `preemptGen` / `signalPending` | 1.14 | 依赖异步抢占 |
| `stackguard0` / `stackPreempt` | 1.14 | 依赖异步抢占的同步路径 |
| cgo（`cgocall` / `cgoCtxt`） | — | tin 是纯 C++，无 cgo 概念 |
| Trace 子系统 | 1.5+ | 独立但工作量大，建议单独评估是否需要 |

---

## 附录 B：实施顺序建议

按以下顺序实施，每步完成后合并到主干：

```
Phase 1（并行，1-2 天）
├── Step 1.1 G 字段
├── Step 1.2 P 字段
├── Step 1.3 M 字段
└── Step 1.4 64 位原子

Phase 2（串行，2-3 天）
├── Step 2.1 ExitSyscall oldp（依赖 1.2, 1.3）
├── Step 2.2 sysmon retake（依赖 1.2, 2.1）
└── Step 2.3 HandoffP 完善（依赖 2.2）

Phase 3（串行，3-4 天）
├── Step 3.1 netpoll 超时参数
├── Step 3.2 netpollBreak（依赖 3.1）
├── Step 3.3 pollUntil 传递（依赖 3.1）
└── Step 3.4 netpollWaiters（依赖 3.1）

Phase 4（串行，2-3 天）
├── Step 4.1 Mutex 饥饿模式
├── Step 4.2 sudogcache
└── Step 4.3 semaphore handoff（可选，依赖 4.1）

Phase 5（串行，1-2 天）
├── Step 5.1 stealOrder
└── Step 5.2 ShouldStealTimers 优化（依赖 5.1）

Phase 6（并行，2-3 天）
├── Step 6.1 goidcache
├── Step 6.2 checkdead
└── Step 6.3 schedtrace

Phase 7（按需）
├── Step 7.1 scavenge（独立）
├── Step 7.2 stackpool（独立）
├── Step 7.3 rseq/wseq 分离（独立）
└── Step 7.4 lockRank（独立）
```

---

## 附录 C：关键 Go 1.15 源码位置索引

| 主题 | Go 1.15 文件:行号 |
|------|-----------------|
| schedule() | proc.go:2595-2709 |
| findRunnable() | proc.go:2175-2473 |
| stealWork() | proc.go:2336-2373 |
| stealOrder | proc.go:2289-2336 |
| sysmon() | proc.go:4615-4733 |
| retake() | proc.go:4746-4813 |
| wakep() | proc.go:2045-2054 |
| ready() | proc.go:3000-3100 |
| startm() / handoffp() | proc.go:1927-2043 |
| exitsyscall() / exitsyscall0() | proc.go:3035-3090 |
| runqput / runqget / runqsteal | proc.go:5126-5339 |
| acquireSudog / releaseSudog | proc.go:6580-6640 |
| goidcache 分配 | proc.go:3403-3413 |
| checkdead() | proc.go:4503-4597 |
| schedtrace() | proc.go:4875+ |
| per-P timer (addtimer/deltimer/modtimer) | time.go:244-460 |
| checkTimers() | time.go:537+ |
| timeSleepUntil() | time.go:637+ |
| netpoll() 接口 | netpoll_epoll.go:106 |
| netpollBreak | netpoll_epoll.go:42-58, 81-99 |
| netpollblock / netpollblockcommit | netpoll.go:393-444 |
| sync.Mutex 饥饿模式 | sync/mutex.go |
| semaRoot treap | sema.go:40-44, 234-444 |
| semrelease1 handoff | sema.go:191-213 |
| 异步抢占 | preempt.go:110-316 |
| GMP 结构体 | runtime2.go:33-692 |
| G/P/M 状态常量 | runtime2.go:33-155 |
