# Go 1.15 Runtime 升级特性清单

> 对比基准：tin 当前实现（基于 Go 1.6 runtime）→ 目标：Go 1.15 runtime
>
> 源码位置：Go 1.15 `D:\home\dev\code\ai\1\go1.15\src\runtime\`，tin `d:\home\dev\code\ai\1\tin\tin\runtime\`

---

## 0. 总览

Go 1.6 → Go 1.15 之间（含 1.7-1.15 九个版本）runtime 的主要演进：

| 版本 | 关键 runtime 变化 |
|------|-----------------|
| 1.7 | SSA 编译器后端（间接影响 runtime），`sort` 算法改进 |
| 1.8 | `defer` 占用空间减半（struct 字段顺序优化），`map` 迭代顺序完全随机化 |
| 1.9 | `sync.Mutex` 引入饥饿模式；`time` 包部分接口调整；`runtime.nanotime` 精度提升 |
| 1.10 | per-P timer heap 第一版雏形；`string` 标准化 |
| 1.11 | `map` 的 `clear` 优化；`sync.Map` 改进 |
| 1.12 | 引入 `pageAlloc`/`pageCache`/`spanSet`，scavenger goroutine，sweep 大改 |
| 1.13 | `defer` 栈分配（避免堆分配）；错误包装 `errors.Is/As`；`map` 新的 `mapassign_faststr` |
| 1.14 | **异步抢占**（信号-based）；per-P timer heap 正式版；`open-coded defer`；`netpoll` 接受 `pollUntil` |
| 1.15 | timer 状态机优化；`mcentral` 新实现（`spanSet`）；`markrootSpans` 新算法；`mallocinit` hint 调整 |

---

## 1. 调度器（Scheduler / GMP）

### 1.1 G（Goroutine）结构体新增字段

| 字段 | Go 1.15 位置 | 用途 | tin 现状 | 迁移难度 |
|------|------------|------|---------|---------|
| `preempt` | runtime2.go:433 | 抢占信号 | 缺失 | 高（依赖异步抢占基础设施） |
| `preemptStop` | runtime2.go:434 | 转 `_Gpreempted` | 缺失 | 高 |
| `preemptShrink` | runtime2.go:435 | 同步安全点缩栈 | 缺失 | 高 |
| `asyncSafePoint` | runtime2.go:440 | 异步安全点标记 | 缺失 | 高 |
| `gcscandone` | runtime2.go:443 | 栈扫描完成标记 | 缺失 | 中（依赖 GC） |
| `stackguard0` | runtime2.go:415 | 抢占哨兵（设为 `stackPreempt`） | 缺失 | 高（异步抢占核心） |
| `syscallsp` / `syscallpc` | runtime2.go:422-423 | GC 时 syscall sp/pc | 缺失 | 中 |
| `stktopsp` | runtime2.go:424 | traceback 校验 | 缺失 | 中 |
| `param` | runtime2.go:425 | 唤醒时传递参数 | 缺失 | 低 |
| `goid` | runtime2.go:428 | goroutine ID | 缺失 | 低 |
| `waitsince` / `waitreason` | runtime2.go:430-431 | 阻塞起始时间/原因 | 缺失 | 低 |
| `activeStackChans` | runtime2.go:449 | 栈上有 channel 指针 | 缺失 | 中（依赖栈收缩） |
| `selectDone` | runtime2.go:470 | select 唤醒竞争 CAS | 缺失 | 中（依赖 select） |
| `gcAssistBytes` | runtime2.go:481 | GC 辅助信用 | 缺失 | 高（依赖 GC） |

### 1.2 P（Processor）结构体新增字段

| 字段 | Go 1.15 位置 | 用途 | tin 现状 | 迁移难度 |
|------|------------|------|---------|---------|
| `runnext` | runtime2.go:598 | 下一轮继承时间片 | **已有** | 已迁移 |
| `timers` / `timersLock` | runtime2.go:663-668 | per-P timer heap | **已有** | 已迁移 |
| `timer0When` | runtime2.go:637 | 堆顶 timer when（atomic） | **已有** | 已迁移 |
| `adjustTimers` | runtime2.go:678 | 修改更早的 timer 数 | **已有** | 已迁移 |
| `deletedTimers` | runtime2.go:682 | 已删除 timer 数 | **已有** | 已迁移 |
| `numTimers` | runtime2.go:672 | timer 总数 | **已有** | 已迁移 |
| `syscalltick` | runtime2.go:571 | sysmon retake 用 tick | 缺失 | 中 |
| `sysmontick` | runtime2.go:572 | sysmon 上次观察的 tick | 缺失 | 中 |
| `mcache` | runtime2.go:574 | 内存分配缓存 | 缺失 | 高（依赖分配器） |
| `pcache` | runtime2.go:575 | pageCache | 缺失 | 高（依赖 pageAlloc） |
| `deferpool` / `deferpoolbuf` | runtime2.go:578-579 | defer 缓存池 | 缺失 | 中（依赖 defer） |
| `goidcache` / `goidcacheend` | runtime2.go:582-583 | goid 批量缓存 | 缺失 | 低 |
| `gFree` | runtime2.go:601 | 死 G 复用链 | 缺失 | 中 |
| `sudogcache` / `sudogbuf` | runtime2.go:606-607 | sudog 缓存 | 缺失 | 中 |
| `mspancache` | runtime2.go:610 | mspan 缓存 | 缺失 | 高 |
| `gcBgMarkWorker` | runtime2.go:642 | 后台标记 G | 缺失 | 高（依赖 GC） |
| `gcw` (gcWork) | runtime2.go:652 | GC 工作缓冲 | 缺失 | 高（依赖 GC） |
| `wbBuf` | runtime2.go:657 | 写屏障缓冲 | 缺失 | 高（依赖 GC） |
| `preempt` (P 级) | runtime2.go:689 | P 异步抢占请求 | 缺失 | 高 |
| `runSafePointFn` | runtime2.go:659 | 安全点函数 | 缺失 | 中 |

### 1.3 M（Machine）结构体新增字段

| 字段 | Go 1.15 位置 | 用途 | tin 现状 | 迁移难度 |
|------|------------|------|---------|---------|
| `g0` | runtime2.go:485 | 调度栈 G | **已有** | 已迁移 |
| `curg` | runtime2.go:496 | 当前用户 G | **已有** | 已迁移 |
| `p` / `nextp` | runtime2.go:498-499 | 绑定 P / 下一个 P | **已有** | 已迁移 |
| `spinning` | runtime2.go:508 | 是否在找 work | **已有** | 已迁移 |
| `park` (note) | runtime2.go:521 | M park | **已有** | 已迁移 |
| `lockedg` | runtime2.go:524 | LockOSThread | **已有** | 已迁移 |
| `oldp` | runtime2.go:500 | syscall 前的 P | **缺失** | 中（syscall affinity 改造） |
| `gsignal` | runtime2.go:491 | 信号处理 G | 缺失 | 高（异步抢占） |
| `goSigStack` / `sigmask` | runtime2.go:492-493 | 信号栈 | 缺失 | 高 |
| `tls [6]uintptr` | runtime2.go:494 | TLS | 缺失 | 中（tin 用全局 TLS 替代） |
| `preemptGen` | runtime2.go:551 | 抢占信号代数 | 缺失 | 高 |
| `signalPending` | runtime2.go:555 | 挂起抢占信号 | 缺失 | 高 |
| `locks` / `mallocing` / `preemptoff` | runtime2.go:502-504 | 持锁/分配/禁抢占标记 | 缺失 | 中 |
| `fastrand` | runtime2.go:514 | 快速随机 | 缺失（用 `rand()`） | 低 |
| `procid` | runtime2.go:490 | OS 线程 ID | 隐含 | 低 |

### 1.4 G/P 状态机扩展

| 状态 | Go 1.15 | tin 现状 | 说明 |
|------|---------|---------|------|
| `_Gidle` | runtime2.go:36 | 缺失 | tin 不区分"刚分配未初始化" |
| `_Grunnable` / `_Grunning` / `_Gsyscall` / `_Gwaiting` | ✓ | ✓ | 一致 |
| `_Gdead` | runtime2.go:88 | tin 用 `kExited` | tin 无 gFree 复用 |
| `_Gcopystack` | runtime2.go:99 | 缺失 | tin 无栈复制 |
| **`_Gpreempted`** | runtime2.go:93 | **缺失** | Go 1.14 新增，异步抢占关键状态 |
| `_Gscan = 0x1000` | runtime2.go:104 | 缺失 | GC scan 位（无 GC 不需要） |
| `_Pgcstop` | runtime2.go:139 | 缺失 | tin 无 STW GC |

### 1.5 调度算法改进

| 算法特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|---------|------------|---------|---------|
| `checkTimers` 在 `schedule` 入口 | proc.go:2595+ | **已有**（FindRunnable 内） | 低 |
| `findRunnable` 集成 timer | proc.go:2175+ | **已有** | 低 |
| **`stealOrder` 确定性遍历** | proc.go:2289-2336 | 缺失（用 `rand()%MaxProcs`） | 中 |
| **`pollUntil` 传给 netpoll** | proc.go:2317-2435 | 缺失 | 中（与 netpoll 协同） |
| work stealing 偷 timer | proc.go:2361-2373 | **部分有**（`ShouldStealTimers`） | 低 |
| spinning M 上限公式 `2*>=procs-npidle` | proc.go:2243 | **已有** | 已迁移 |
| `runnext` 机制 | proc.go:5126-5261 | **已有** | 已迁移 |
| `resetspinning` 唤醒新 spinning | proc.go:2512-2534 | **已有** | 已迁移 |
| `sched.disable`（用户调度禁用） | proc.go:2648-2659 | 缺失 | 低 |

### 1.6 sysmon 改进

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| 自适应 sleep（20us→10ms） | proc.go:4626-4636 | **有**（但初始 20ms，非 20us） | 低 |
| netpoll 10ms 触发 | proc.go:4653-4665 | **已有** | 已迁移 |
| timer 监控唤醒 P | proc.go:4703-4720 | **已有** | 已迁移 |
| **`retake`（syscall handoff）** | proc.go:4746-4813 | **缺失** | 中（性价比最高的下一步） |
| **`preemptone`（同步抢占）** | proc.go:4827-4843 | **缺失** | 高（依赖 stackguard0） |
| `checkdead` 死锁检测 | proc.go:4503-4597 | 缺失 | 中 |
| STW 深睡 | proc.go:4677-4690 | 缺失（无 STW） | 中 |
| force GC | proc.go:4725-4728 | 缺失 | 中 |
| `schedtrace` 调试输出 | proc.go:4875+ | 缺失 | 低 |

### 1.7 异步抢占（Async Preemption，Go 1.14 引入）

| 组件 | Go 1.15 位置 | 用途 | tin 现状 |
|------|------------|------|---------|
| `asyncPreempt` (汇编) | preempt.go:304 | spill 寄存器，调 asyncPreempt2 | 缺失 |
| `asyncPreempt2` | preempt.go:307-316 | 走 `preemptPark` 或 `gopreempt_m` | 缺失 |
| `suspendG` | preempt.go:110-259 | 驱动 G 到达安全点 | 缺失 |
| `resumeG` | preempt.go:263-285 | 清 `_Gscan`，ready G | 缺失 |
| `preemptM` | signal_unix.go | 通过 SIGURG 信号抢占 | 缺失 |
| `canPreemptM` | preempt.go:292-294 | `mp.locks==0 && mp.mallocing==0 && ...` | 缺失 |
| `stackPreempt` 哨兵值 | stack.go:129 | 写入 stackguard0 触发抢占 | 缺失 |
| 三类安全点 | preempt.go:7-19 | blocked/synchronous/asynchronous | 缺失 |

**迁移难度**：极高（依赖编译器 prologue 栈检查 + 信号栈 + 寄存器 spill）

---

## 2. Timer 子系统

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| Per-P timer heap（4-ary heap） | time.go:17-37 | **已迁移** | 已完成 |
| `timer` struct 完整字段 | time.go:17-37 | **已迁移** | 已完成 |
| 10 状态状态机 | time.go:117-158 | **已迁移** | 已完成 |
| `addtimer` / `deltimer` / `modtimer` | time.go:244-460 | **已迁移** | 已完成 |
| `cleantimers` / `adjusttimers` / `runtimer` | time.go | 部分集成在 CheckTimers | 低 |
| `checkTimers(pp, now, pollUntil, ran)` | time.go:537+ | **已迁移** | 已完成 |
| `timeSleepUntil` | time.go:637+ | **已迁移** | 已完成 |
| `MoveTimers`（P 销毁时迁移） | time.go | **已迁移** | 已完成 |
| `wakeNetPoller` | time.go:236-238 | **已迁移**（简化版） | 已完成 |
| `time.Sleep` 用 `gp.timer` | time.go:174-189 | 用 `InternalNanoSleep` | 低（细节差异） |

---

## 3. 网络轮询（netpoll）

### 3.1 核心改进

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| **`netpoll(delay int64) gList`** 接受超时 | netpoll_epoll.go:106 | `NetPoll(bool block)` 无超时 | **高**（架构改动） |
| **`pollUntil` 与 timer 协同** | proc.go:2192, 2317 | 缺失 | 高 |
| **`netpollBreak` 唤醒管道** | netpoll_epoll.go:42-58, 81-99 | **缺失** | 中 |
| `netpollWaiters` 全局计数 | netpoll.go:109 | 缺失 | 中 |
| `everr`（EPOLLERR 标记） | netpoll.go:82 | 缺失 | 低 |
| `pollCache` 对象池化 | netpoll.go:94-102, 527-549 | 缺失（用 ref count） | 中 |
| `epoll_create1` | netpoll_epoll.go:32 | 用 `epoll_create(1024)` | 低 |
| `rseq` / `wseq` 分离 | netpoll.go:77, 79 | 单一 `seq` | 中 |
| `combo` deadline 模式 | netpoll.go:263-267 | 部分有 | 低 |
| `netpollIsPollDescriptor` | netpoll_epoll.go:60 | 缺失 | 低 |

### 3.2 数据结构差异

| 字段 | Go 1.15 `pollDesc` | tin `PollDescriptor` | 差异 |
|------|-------------------|---------------------|------|
| 生命周期 | `//go:notinheap` + pollCache | `RefCountedThreadSafe` | 不同方案 |
| `rseq` / `wseq` | 分离 | 单 `seq` | tin 简化 |
| `everr` | 有 | 无 | tin 缺失 |
| `link` | pollCache 链表 | 无 | tin 用 ref count |

---

## 4. Channel / Select

### 4.1 Channel

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| `hchan` struct（循环数组 buf） | chan.go:32-51 | `std::deque<T>` | **高**（架构不同） |
| `sendq` / `recvq` 等待队列 | chan.go:53-56 | 用信号量替代 | 高 |
| `sendDirect` / `recvDirect` 跨栈 | chan.go:330-350 | 缺失 | 高 |
| `sudog` 完整 14 字段 | runtime2.go:345-373 | 10 字段（缺 `acquiretime/releasetime/ticket/isSelect/parent/waittail/c`） | 中 |
| Fast path 非阻塞接口 | chan.go:175-193, 666-713 | **永远阻塞** | 中 |
| `selectnbsend` / `selectnbrecv` | chan.go:666-713 | 缺失 | 中 |
| `activeStackChans` 与栈收缩 | chan.go:641-647 | 缺失 | 中 |
| `chanparkcommit` | chan.go:641-647 | 缺失 | 中 |
| 三种分配策略 | chan.go:91-107 | 模板 + deque | — |
| close 唤醒所有等待者 | chan.go:352-419 | 有（但行为可能不一致） | 中 |
| per-P `sudogcache` | runtime2.go:606-607 | 缺失（每次 new） | 中 |

### 4.2 Select

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| `scase` struct | select.go:28-34 | **完全缺失** | 极高 |
| `selectgo` 三阶段算法 | select.go:119-500 | 缺失 | 极高 |
| Fisher-Yates shuffle `pollorder` | select.go:159-163 | 缺失 | 中 |
| `lockorder` heap sort | select.go:167-199 | 缺失 | 中 |
| `selectDone` CAS 竞态解决 | chan.go:792 | 缺失 | 中 |
| `selparkcommit` | select.go:77-102 | 缺失 | 中 |
| 0/1 case 编译器优化 | select.go:150-156 | 缺失 | 高（依赖编译器） |

**迁移难度**：极高（依赖 channel 重构 + 编译器配合）

---

## 5. Semaphore

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| `semaRoot` treap 数据结构 | sema.go:40-44 | 双向链表 | 中 |
| `semtable` cache line padding | sema.go:47-52 | ALIGNAS(64) | 低（已对齐） |
| `semacquire1` `lifo` 参数 | sema.go:98, 143 | 缺失 | 中 |
| `semrelease1` `handoff` 机制 | sema.go:191-213 | 缺失 | 中 |
| `ticket` 优先级 + 饥饿标志 | runtime2.go:361 | 缺失 | 中 |
| `goyield`（让出 P） | sema.go:204-211 | 缺失 | 中 |
| `acquireSudog` / `releaseSudog` 池化 | proc.go:6580-6640 | 每次 `new Sudog` | 中 |
| `notifyList`（Cond） | sema.go:449-604 | tin/sync/cond.cc 有独立实现 | 低 |
| profile 采样 | sema.go:121-130 | 缺失 | 低 |
| `SemSetDeadline` | — | tin 独有 | — |

---

## 6. 内存分配器

### 6.1 三层架构

| 组件 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| `mheap` 全局堆 | mheap.go:66-243 | **完全缺失**（依赖 `std::malloc`） | 高 |
| `mcentral` 中心自由列 | mcentral.go:20-53 | 缺失 | 高 |
| `mcache` 每 P 缓存 | mcache.go:19-54 | 缺失 | 高 |
| `mallocgc` 主入口 | malloc.go:903-1158 | 缺失 | 高 |
| 67 个 size class | sizeclasses.go:6-95 | 缺失 | 低（纯数据表） |
| `mspan` 结构 | mheap.go:412-486 | 缺失 | 中 |

### 6.2 Go 1.12 引入的新结构

| 特性 | Go 1.15 位置 | 用途 | 迁移难度 |
|------|------------|------|---------|
| **`pageAlloc` 基数树** | mpagealloc.go:175-220 | 替代 treap 自由链表 | 极高（~1500 行） |
| **`pageCache` 每 P 页缓存** | mpagecache.go:12-22 | 64 位 bitmap，无锁取 64 页 | 中 |
| **`spanSet` 无锁双层** | mspanset.go:17-52 | 替代 `MHeap.spans[]` 数组 | 高（~350 行） |
| `heapArena` 两级 arena map | mheap.go:247+ | 稀疏堆元数据 | 高 |

### 6.3 栈分配

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| 连续栈（contiguous stack） | stack.go | FixedSizeStack | — |
| **`copystack` 栈复制** | stack.go:840+ | 缺失 | 极高（C++ 栈不可移动） |
| `morestack` 增长 | stack.go | 缺失 | 极高 |
| `stackpool` 小栈缓存 | stack.go:141-156 | 缺失 | 低-中 |
| `stackLarge` 大栈分桶 | stack.go:152-156 | 缺失 | 低-中 |
| per-P `mcache.stackcache` | mcache.go:42 | 缺失 | 中 |
| guard page | _StackGuard + prologue | ProtectedFixedSizeStack 已有 | 已迁移 |

---

## 7. 垃圾回收（GC）

### 7.1 整体算法

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| 并发标记-清除 | mgc.go:1-83 | **完全缺失** | 极高 |
| 三色标记 | mgc.go | 缺失 | 极高 |
| **混合写屏障（1.8 引入）** | mgc.go:257-263 | 缺失 | 极高（依赖编译器插桩） |
| `writeBarrier` 全局 | mgc.go:257-263 | 缺失 | 极高 |
| `wbBuf` 写屏障缓冲 | mwbbuf.go:42-66 | 缺失 | 高 |
| STW 机制 | proc.go + mgc.go | 缺失 | 高 |
| `_Pgcstop` P 状态 | runtime2.go:139 | 缺失 | 高 |
| `_Gscan` 状态位 | runtime2.go:104 | 缺失 | 高 |

### 7.2 Go 1.12 scavenge

| 特性 | Go 1.15 位置 | 用途 | 迁移难度 |
|------|------------|------|---------|
| 后台 scavenger goroutine | mgcscavenge.go:11-17 | 归还空闲页给 OS | 中（独立于 GC） |
| `scavengeGoal` RSS 目标 | mgcscavenge.go:21 | 降 RSS 峰值 | 中 |
| `madvise(MADV_DONTNEED)` | mem_linux.go | POSIX 接口 | 低 |
| `wakeScavenger` | mgcscavenge.go:185-200 | sysmon 唤醒 | 中 |

### 7.3 Go 1.14/1.15 pacer 改进

| 特性 | Go 1.15 位置 | 用途 | 迁移难度 |
|------|------------|------|---------|
| `gcControllerState` | mgc.go:336-409 | GC 节奏控制 | 高 |
| `GOGC=off` 支持 | mgc.go:152-167 | 调试 | 高 |
| dedicated/fractional/idle worker | mgc.go:283-319 | 三种 worker 模式 | 高 |
| mutator assist | malloc.go:952 | 分配时还债 | 高 |
| `revise()` 动态调整 | mgc.go:489+ | STW 外调整 | 高 |

### 7.4 Go 1.15 mcentral 新实现

| 特性 | Go 1.15 位置 | 用途 | 迁移难度 |
|------|------------|------|---------|
| `go115NewMCentralImpl` flag | mheap.go:48-55 | 新 mcentral 实现 | 高 |
| `partial [2]spanSet` + `full [2]spanSet` | mcentral.go:46-47 | 替代 `nonempty/empty mSpanList` | 高 |
| `cacheSpan` 4 路查找 | mcentral.go:95-200 | partialSwept→partialUnswept→fullUnswept→grow | 高 |
| `go115NewMarkrootSpans` | mgcmark.go:53 | 位图式 markrootSpans | 高 |

---

## 8. defer / panic / recover

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| `_defer` struct | runtime2.go:881-906 | **完全缺失** | 高（语言层） |
| `_panic` struct | runtime2.go:919-928 | 缺失 | 高 |
| 经典 defer（堆分配） | panic.go:218-261 | 缺失 | 高 |
| **Go 1.13 栈分配 defer** | panic.go:271-306 | 缺失 | 高 |
| **Go 1.14 open-coded defer** | panic.go:13-24 | 缺失 | 极高（依赖编译器 + funcdata） |
| `newdefer` per-P pool | panic.go:387-432 | 缺失 | 中 |
| `gopanic` / `gorecover` | panic.go:889+, 1084-1098 | 缺失 | 高 |
| `Goexit` | panic.go:583-658 | 缺失 | 中 |
| `runOpenDeferFrame` bitmask | panic.go:814-869 | 缺失 | 极高 |
| `PanicException` C++ 异常 | — | tin 独有 | — |

**迁移难度**：高（语言层特性，C++ 无 `defer` 关键字）

---

## 9. Map

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| `hmap` struct | map.go:115-129 | **完全缺失** | 高 |
| `bmap` 桶结构（8 kv/桶） | map.go:149-159 | 缺失 | 高 |
| `tophash` 高 8 位加速 | map.go:107+ | 缺失 | 中 |
| `emptyRest` 提前终止 | map.go:92-107 | 缺失 | 中 |
| **渐进式扩容** `evacuate` | map.go:1128+ | 缺失 | 高 |
| `sameSizeGrow` 横向扩容 | map.go:1068-1078 | 缺失 | 中 |
| `mapextra` GC 友好 | map.go:132-146 | 缺失 | — |
| `map_fast32/64/str` fast path | map_fast64.go 等 | 缺失 | 中 |
| `hiter` 迭代器 | map.go:164-180 | 缺失 | 中 |
| 并发 map 检测 | map.go:571+ | 缺失 | 低 |

**迁移难度**：高（C++ 有 `std::unordered_map`、`absl::flat_hash_map` 可替代）

---

## 10. Locks（Mutex / RWMutex）

### 10.1 Runtime mutex

| 特性 | Go 1.15 位置 | tin RawMutex 现状 | 迁移难度 |
|------|------------|-----------------|---------|
| `lock`/`unlock` 三态（unlocked/locked/sleeping） | lock_futex.go:25-33 | 用 sema 替代 | 已迁移 |
| Linux futex 路径 | lock_futex.go | 统一用 std::counting_semaphore | 低（性能优化） |
| spin + passive spin | lock_futex.go:50-108 | **已有** | 已迁移 |
| `lockRankStruct` 排序检测 | runtime2.go:162-169 | 缺失 | 中（debug 用） |

### 10.2 sync.Mutex（用户态）

| 特性 | Go 1.15 位置 | tin Mutex 现状 | 迁移难度 |
|------|------------|--------------|---------|
| **Go 1.9 饥饿模式** | sync/mutex.go | **缺失** | 中 |
| `mutexStarving` 位 | sync/mutex.go | 缺失 | 中 |
| 排队时间 >1ms 切换 | sync/mutex.go | 缺失 | 中 |
| 饥饿模式 handoff | sync/mutex.go | 缺失 | 中 |
| `mutexWoken` 位 | sync/mutex.go | **已有** | 已迁移 |
| normal mode spin | sync/mutex.go | **已有** | 已迁移 |

### 10.3 RWMutex

| 特性 | Go 1.15 位置 | tin RWMutex 现状 | 迁移难度 |
|------|------------|----------------|---------|
| `readerCount` / `readerWait` | sync/rwmutex.go | **已有** | 已迁移 |
| `readerSem` / `writerSem` | sync/rwmutex.go | **已有** | 已迁移 |
| `RLock` / `RUnlock` | sync/rwmutex.go | **已有** | 已迁移 |
| 翻转 `rwmutexMaxReaders=1<<30` | sync/rwmutex.go | **已有** | 已迁移 |

---

## 11. Atomic

| 特性 | Go 1.15 位置 | tin atomic 现状 | 迁移难度 |
|------|------------|---------------|---------|
| `Load/Store/CAS/Xchg/Xadd` | internal/atomic | **已有**（基于 std::atomic） | 已迁移 |
| 显式内存序（acquire/release/relaxed） | x86 TSO 隐式 | **更优**（显式） | 已迁移 |
| 64 位原子（`Xadd64/Cas64/Store64`） | atomic_amd64.go | **缺失** | 低（trivial） |
| `And8` / `Or8` 位操作 | atomic_amd64.go:63-66 | **缺失** | 低 |
| `StorepNoWB` 无写屏障存储 | atomic_amd64.go:92 | 缺失 | —（C++ 无 GC） |
| `atomic.Value` | sync/atomic | 缺失 | 低（用模板替代） |
| `atomic.Pointer/Int32/...` 类型 | —（Go 1.19 才有） | — | — |

---

## 12. Time

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| Per-P timer 模型 | time.go | **已迁移** | 已完成 |
| Timer 状态机 | time.go:117-158 | **已迁移** | 已完成 |
| `nanotime` 单调时钟 | 汇编 | `std::chrono::steady_clock` | 已迁移 |
| `time.Sleep` 用 `gp.timer` | time.go:174-189 | `InternalNanoSleep` | 低（细节差异） |
| `MonoNow` | — | **已有** | 已迁移 |

---

## 13. 其他子系统

### 13.1 Trace

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| `traceReader` | proc.go:2639-2642 | 缺失 | 中 |
| `tracebuf` per-P 缓冲 | runtime2.go:620-628 | 缺失 | 中 |
| `traceseq` / `tracelastp` | runtime2.go:454-455 | 缺失 | 中 |

### 13.2 Lock Ranking（debug）

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| `lockRank` 枚举 | lockrank.go | 缺失 | 中 |
| `lockWithRank` | lock_futex.go | 缺失 | 中 |
| `locksHeld[10]` per-M | runtime2.go:562-563 | 缺失 | 中 |

### 13.3 CPU Profiling

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| `cpuprof` | cpuprof.go | 缺失 | 中 |
| `sigprof` 信号采样 | signal_unix.go | 缺失 | 高

### 13.4 cgo

| 特性 | Go 1.15 位置 | tin 现状 | 迁移难度 |
|------|------------|---------|---------|
| `cgocall` | cgocall.go | 缺失 | —（tin 是纯 C++，无 cgo 概念） |
| `cgoCtxt` per-G | runtime2.go:467 | 缺失 | — |

---

## 14. 迁移优先级与可行性总评

### 14.1 已完成迁移（保持现状）

- ✅ Per-P timer 模型（1.14）
- ✅ runnext 机制
- ✅ spinning M 管理
- ✅ netpoll 集成（基础版）
- ✅ sysmon netpoll/timer 唤醒
- ✅ RawMutex（runtime mutex）
- ✅ RWMutex
- ✅ atomic 基础（基于 std::atomic）
- ✅ ProtectedFixedSizeStack guard page

### 14.2 高性价比可迁移（推荐）

| 优先级 | 子系统 | 难度 | 收益 |
|--------|--------|------|------|
| P0 | **sysmon retake（syscall handoff）** | 中 | 长时间 syscall 不再阻塞调度 |
| P0 | **`oldp` 字段 + ExitSyscall 改造** | 中 | syscall affinity，提升缓存命中 |
| P1 | **`stealOrder` 确定性遍历** | 中 | work stealing 覆盖度，避免重复访问 |
| P1 | **`pollUntil` 传给 netpoll** | 中 | timer 与 netpoll 协同，避免空转 |
| P1 | **`netpollBreak` 唤醒管道** | 中 | 阻塞 netpoll 可被唤醒 |
| P1 | **`netpollWaiters` 计数** | 中 | 调度器决策更准确 |
| P2 | **Go 1.9 sync.Mutex 饥饿模式** | 中 | 高竞争场景避免饥饿 |
| P2 | **`gFree` / `sudogcache` 池化** | 中 | 减少 malloc 开销 |
| P2 | **`goid` / `waitsince` / `waitreason` 字段** | 低 | 调试可观测性 |
| P2 | **`sched.disable` 用户调度禁用** | 低 | 调试用 |
| P3 | **`fastrand` per-M** | 低 | 性能 |
| P3 | **`checkdead` 死锁检测** | 中 | 死锁诊断 |
| P3 | **`schedtrace` 调试输出** | 低 | 调试用 |
| P3 | **64 位原子 + And8/Or8** | 低 | 完整性 |

### 14.3 不推荐迁移（成本极高/收益极低）

| 子系统 | 难度 | 不推荐理由 |
|--------|------|----------|
| 异步抢占（async preemption） | 极高 | 依赖编译器 prologue 栈检查 + 信号栈 + 汇编寄存器 spill，C++ 协程库无法复用 |
| 并发 GC + 写屏障 | 极高 | C++ 无运行时类型信息，无编译器指针写插桩，无法做类型精确 GC |
| 栈复制（copystack） | 极高 | C++ 栈有 C frame 和不可移动对象，无法重定位指针 |
| open-coded defer | 极高 | 依赖 SSA funcdata + bitmask，C++ 无对应机制 |
| Go 风格 hmap | 高 | C++ 有 `std::unordered_map`、`absl::flat_hash_map` 可用 |
| Go 风格 channel（hchan + sudog） | 高 | tin 是 C++ 模板风格，重构等于重写 |
| Go select | 极高 | 强依赖 channel 重构 + 编译器配合 |
| `pageAlloc` 基数树 | 极高 | ~1500 行，仅在大堆场景才有收益 |
| Trace 子系统 | 中 | 独立但工作量大，建议单独评估 |

### 14.4 中等性价比可选迁移

| 子系统 | 难度 | 备注 |
|--------|------|------|
| Scavenge（独立于 GC） | 中 | 用 `madvise(MADV_DONTNEED)` 降 RSS，无需完整 GC |
| `stackpool` 小栈缓存 | 低-中 | 频繁 spawn/exit 协程时减少 malloc |
| per-M 小对象缓存 + size class | 中 | 降低 malloc 锁竞争 |
| per-P `sudogcache` | 中 | 减少 sudog 分配开销 |
| Linux futex 路径（RawMutex） | 低 | 性能优化，非必须 |
| `lockRank` 死锁检测 | 中 | debug 模式 |
| `rseq` / `wseq` 分离 | 中 | netpoll deadline 正确性 |

---

## 15. 参考源码位置速查

| 子系统 | Go 1.15 主文件 | tin 主文件 |
|--------|---------------|----------|
| Scheduler | proc.go, runtime2.go | scheduler.cc/h, p.h/cc, m.h/cc, coroutine.h/cc |
| Timer | time.go | timer/timer_queue.cc/h |
| netpoll | netpoll.go, netpoll_epoll.go | net/netpoll.cc, net/netpoll_epoll.cc, net/pollops.cc |
| Channel | chan.go | communication/chan.h |
| Select | select.go | （缺失） |
| Semaphore | sema.go | semaphore.cc/h |
| Allocator | malloc.go, mheap.go, mcentral.go, mcache.go | （缺失） |
| GC | mgc.go, mgcmark.go, mgcsweep.go, mgcscavenge.go | （缺失） |
| Stack | stack.go | stack/stack.cc, stack/fixedsize_stack.cc |
| defer/panic | panic.go | （缺失） |
| Map | map.go, map_fast*.go | （缺失） |
| Locks | lock_futex.go, rwmutex.go | sync/mutex.cc, sync/rwmutex.cc, runtime/raw_mutex_sema.cc |
| Atomic | internal/atomic/atomic_amd64.go | sync/atomic.h, sync/atomic_flag.h |
| Time | time.go | time/time.cc, runtime/timer/timer_queue.cc |
| Sysmon | proc.go:4615+ | runtime/sysmon.cc |
| Async preemption | preempt.go, signal_unix.go | （缺失） |
