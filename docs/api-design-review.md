# Tin 项目对外头文件与 `tin.h` 接口设计评审

> 评审对象：`tin/tin.h`、`tin/all.h` 以及由它们暴露出来的整套对外 API
> 评审目标：从一个"想在另一个项目里使用 tin"的用户视角，看现在的设计合不合理，并给出可落地的重构方案。

---

## 一、当前现状速览

### 1.1 用户唯一入口：`tin/all.h`

[tin/all.h](file:///d:/home/dev/code/cpp/202607/tin/tin/all.h) 是 echo / simple 等 example 唯一 include 的头文件，它把以下东西一股脑塞给用户：

```cpp
#include <absl/log/log.h>
#include <absl/log/check.h>
#include "tin/net/sys_socket.h"
#include "cliff/base/sys_byteorder.h"
#include "tin/error/error.h"
#include "tin/time/time.h"
#include "tin/communication/chan.h"
#include "tin/net/resolve.h"
#include "tin/net/dialer.h"
#include "tin/net/netfd.h"
#include "tin/sync/atomic_flag.h"
#include "tin/sync/atomic.h"
#include "tin/sync/mutex.h"
#include "tin/sync/wait_group.h"
#include "tin/runtime/spawn.h"
#include "tin/runtime/runtime.h"
#include "tin/tin.h"
#include <thread>
```

### 1.2 `tin.h` 暴露的生命周期 API

[tin/tin.h](file:///d:/home/dev/code/cpp/202607/tin/tin/tin.h):

```cpp
namespace tin {

typedef int(*EntryFn)(int argc, char** argv);

void Initialize();
void PowerOn(EntryFn fn, int argc, char** argv, Config* new_conf);
void PowerOn(EntryFn fn, Config* new_conf);
int  WaitForPowerOff();
void Deinitialize();

Config* GetWorkingConfig();
Config  DefaultConfig();
}
```

### 1.3 实际实现 [tin/tin.cc](file:///d:/home/dev/code/cpp/202607/tin/tin/tin.cc)

```cpp
void Initialize() {
  std::string name = "John"; int age = 25;
  std::string formatted_string = absl::StrFormat(
      "My name is %s and I am %d years old.", name, age);
  std::cout << formatted_string << std::endl;        // <-- 调试代码混在库初始化里
  conf = new tin::Config;
  *conf = DefaultConfig();
  PlatformInit();
}

int WaitForPowerOff() {
  return runtime::rtm_env->WaitMainExit();
}

// runtime::Env::OnMainExit 的实现：
void Env::OnMainExit() {
  _exit(0);                       // <-- 直接 _exit，WaitForPowerOff 永远不会返回
  // TODO(author) wait for all exit, thread pool, net poller etc.
  exit_flag_ = true;
  ThreadPoll::GetInstance()->JoinAll();
  timer_q->Join();
  rtm_env->main_signal_.Notify(); // <-- 走不到这里
}
```

---

## 二、对外头文件设计的问题

### 2.1 `all.h` 是反模式的"大杂烩"

- **强制全包含**：即使用户只想写一个 echo 服务器，也会被拽进 `atomicops.h`、`sys_socket.h`、`sys_byteorder.h` 这些底层细节。
- **编译时间爆炸**：修改任何一个被 `all.h` 间接包含的头文件，所有用户代码都要重新编译。
- **依赖泄漏**：`cliff/base/sys_byteorder.h`、`<absl/log/log.h>` 这种"实现细节"被推到用户面前，用户不得不关心 cliff 是什么、abseil 是什么。
- **没有模块边界**：用户无法判断"哪些 API 是稳定的、哪些是内部的"。所有 `tin/...` 头文件都被视作对外可用。

### 2.2 公共头文件内部还 `using namespace cliff;`

[tin/sync/atomic.h:16](file:///d:/home/dev/code/cpp/202607/tin/tin/sync/atomic.h#L16):

```cpp
namespace tin::atomic {
using namespace cliff;   // <-- 把 cliff:: 全部符号倒入 tin::atomic
...
}
```

这把 cliff 这个实现细节永久焊死在公共 API 上。一旦后续把 cliff 换成 std::atomic，所有依赖 `tin::atomic::Acquire_CompareAndSwap` 之类的代码都得改。

### 2.3 `tin/runtime/runtime.h` 是个"杂物间"

```cpp
namespace tin {
void Throw(const char* str);
void Panic(const char* str = 0);
void LockOSThread();
void UnlockOSThread();
void SetErrorCode(int error_code);
int  GetErrorCode();
bool ErrorOccured();
const char* GetErrorStr();
void Sched();
void NanoSleep(int64_t ns);
void Sleep(int64_t ms);
int64_t Now();
int64_t MonoNow();
int32_t NowSeconds();
}
```

这里塞了 4 类完全不相干的东西：

| 类别 | 函数 | 应该去的头文件 |
|---|---|---|
| 错误码 | `SetErrorCode`/`GetErrorCode`/`ErrorOccured`/`GetErrorStr` | `tin/error.h` |
| 时间 | `Now`/`MonoNow`/`NowSeconds`/`NanoSleep`/`Sleep` | `tin/time.h` |
| 调度控制 | `Sched`/`LockOSThread`/`UnlockOSThread` | `tin/runtime.h` |
| 异常 | `Throw`/`Panic` | `tin/runtime.h` |

而 [tin/error/error.h](file:///d:/home/dev/code/cpp/202607/tin/tin/error/error.h) 里只定义了 `TIN_E*` 错误码枚举和 `TinTranslateSysError` 翻译函数，**读取错误码的 `GetErrorCode()` 反而在 `runtime.h` 里**——这是模块边界设计的硬伤。

### 2.4 `tin/runtime/spawn.h` 让用户去 include "runtime"

用户调用 `tin::Spawn(fn, args...)`，需要 `#include "tin/runtime/spawn.h"`。这给用户一种"我在碰内部实现"的错觉。对比 Go 的 `go` 关键字、Rust tokio 的 `tokio::spawn`，spawn 是一等公民的公共 API，不应该藏在 `runtime/` 目录下。

### 2.5 没有 install 规则、没有 namespaced target

[CMakeLists.txt](file:///d:/home/dev/code/cpp/202607/tin/CMakeLists.txt) 只 `add_library(tin ${SOURCES})`，没有：

- `install(TARGETS tin EXPORT tinTargets ...)`
- `install(DIRECTORY tin/ DESTINATION include/tin ...)`
- `target_include_directories(tin PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> ...)`

所以另一个项目要用 tin，只能在 CMakeLists 里手动 `target_link_libraries(my_app tin)` + `target_include_directories(my_app PRIVATE path/to/tin)`，无法 `find_package(tin)`。

### 2.6 命名空间风格不统一

```cpp
namespace tin {
namespace net {        // 旧式两段
...
}
}

namespace tin::net {   // 新式一段，listener.h / tcp_conn.h / raw_mutex.h 用这种
...
}
```

最新的 Google C++ Style Guide 不规定具体哪种，但同一个项目里两种混用就是问题。

---

## 三、`tin.h` 生命周期接口的问题

### 3.1 `Initialize()` 里混入了调试代码

```cpp
void Initialize() {
  std::string name = "John"; int age = 25;
  std::string formatted_string = absl::StrFormat(
      "My name is %s and I am %d years old.", name, age);
  std::cout << formatted_string << std::endl;
  conf = new tin::Config;
  ...
}
```

每次调用 `tin::Initialize()` 都会在 stdout 打印一行 `"My name is John and I am 25 years old."`——这是调试 absl::StrFormat 是否工作的代码，根本不应该留在库里。echo 启动时也会看到这行，用户体验极差。

### 3.2 `WaitForPowerOff()` 实际上永远不会返回

[tin/runtime/env.cc:74-82](file:///d:/home/dev/code/cpp/202607/tin/tin/runtime/env.cc#L74):

```cpp
void Env::OnMainExit() {
  _exit(0);   // <-- 直接干掉进程
  // 下面这些代码都走不到
  exit_flag_ = true;
  ThreadPoll::GetInstance()->JoinAll();
  timer_q->Join();
  rtm_env->main_signal_.Notify();
}
```

所以 example 里的这段：

```cpp
tin::PowerOn(TinMain, argc, argv, &config);
tin::WaitForPowerOff();   // <-- 永远不返回
tin::Deinitialize();      // <-- 永远不执行
return 0;
```

`WaitForPowerOff` 和 `Deinitialize` 都是**装饰性的**，给用户一种"我有清理流程"的假象，实际上进程是被 `_exit(0)` 暴力终止的。析构函数、RAII、atexit 注册的回调统统不会跑。这是一个**严重的设计缺陷**。

### 3.3 全局单例状态，无法多实例

```cpp
namespace { Config* conf = NULL; }          // tin.cc
extern Env* rtm_env;                         // env.h
extern Scheduler* sched;                     // env.h
extern TimerQueue* timer_q;                  // env.h
extern thread_local Greenlet* glet_tls;      // env.h
extern tin::Config* rtm_conf;                // env.h
```

整个 runtime 是一组全局变量。后果：

- **不能在一个进程里跑两个独立的 tin runtime**（例如插件架构里两个插件都想用 tin）。
- **不可测试**：单元测试之间会相互污染全局状态。
- **不可嵌入**：想把 tin 嵌到一个更大的框架（比如游戏引擎、应用服务器）里，没法做隔离。

### 3.4 `EntryFn` 是 C 函数指针，不能用带捕获的 lambda

```cpp
typedef int(*EntryFn)(int argc, char** argv);
```

这导致用户写：

```cpp
// 想把 config 传给 TinMain？不行，EntryFn 不能带状态。
// 只能通过全局变量或者重新 parse argv。
```

对比 Rust tokio：`tokio::runtime::Runtime::new().block_on(async { ... })`，闭包可以捕获任意状态。

### 3.5 `PowerOn` 的 `Config*` 是可空裸指针

```cpp
void PowerOn(EntryFn fn, int argc, char** argv, Config* new_conf);
```

- 裸指针：所有权语义不清。
- 可空：`new_conf == NULL` 时用默认配置——这种"魔法行为"应该用函数重载或 `std::optional` 表达。
- 可变：调用方传进去之后还能继续修改这个 `Config`，而 `PowerOn` 内部直接 `*conf = *new_conf` 拷贝，时序问题极容易出 bug。

### 3.6 缺少 graceful shutdown

用户无法主动让 tin 停下来。TinMain 返回之后进程就被 `_exit(0)` 干掉了。没有：

- `tin::Shutdown()` 请求调度器停止
- `tin::RegisterShutdownHook(fn)` 注册清理回调
- 超时强制停止机制

对一个网络库来说，没有 graceful shutdown 就意味着无法做热重启、无法在容器里处理 SIGTERM、无法在测试里清理状态。

### 3.7 `argc/argv` 透传没意义

```cpp
void PowerOn(EntryFn fn, int argc, char** argv, Config* new_conf);
```

用户的 `main(int argc, char** argv)` 已经能拿到 argc/argv，为什么还要 PowerOn 帮忙转发一次？TinMain 直接用全局变量或者捕获即可。这个设计是从 Go 的 `os.Args` 抄过来的，但 Go 那是因为 main 函数签名固定。

### 3.8 `GetWorkingConfig()` 返回可变指针

```cpp
Config* GetWorkingConfig();
```

允许用户在 PowerOn 之后随时改 Config，但实际上大部分配置项（StackSize、MaxProcs）在 scheduler 启动后就改不动了。返回 `const Config&` 才合理。

### 3.9 生命周期顺序没有校验

调用 `PowerOn` 之前没 `Initialize`？崩。`Deinitialize` 调两次？第二次 delete 已经被释放的指针，UB。`WaitForPowerOff` 不调直接 return？runtime 还在跑。这些都没有断言保护。

---

## 四、推荐的重构方案

### 4.1 头文件分层（解决 §2.1 ~ §2.4）

按"用户视角的功能模块"重新组织公共头文件，废弃 `all.h`：

```
tin/
├── tin.h                    # 顶层生命周期 API（Runtime 类）
├── config.h                 # Config 类（从 config/config.h 提升）
├── error.h                  # 错误码 + GetErrorCode/GetErrorStr
├── time.h                   # kSecond/kMillisecond/Now/Sleep/NanoSleep
├── runtime.h                # Spawn/Sched/LockOSThread/Panic/Throw
├── net/
│   ├── tcp.h                # TcpConn、DialTcp、ListenTcp（聚合头）
│   ├── tcp_conn.h           # TcpConn 类定义
│   ├── listener.h           # TCPListener 类定义
│   ├── dialer.h             # DialTcp/ListenTcp 函数
│   ├── resolve.h            # DNS 解析
│   └── ip_address.h         # IPAddress 类
├── sync/
│   ├── mutex.h              # Mutex / MutexGuard
│   ├── wait_group.h         # WaitGroup
│   ├── chan.h               # Chan<T> / MakeChan
│   └── atomic.h             # 原子操作（重新设计，不再 using namespace cliff）
└── io/
    ├── io.h                 # Reader/Writer 接口
    └── bufio.h              # BufferedReader
```

**用户的 include 模式变为：**

```cpp
// 最小化：只想用生命周期
#include "tin/tin.h"

// 写一个 TCP echo 服务器
#include "tin/tin.h"
#include "tin/net/tcp.h"
#include "tin/runtime.h"     // Spawn
#include "tin/time.h"        // kSecond
#include "tin/error.h"       // GetErrorCode

// 用同步原语
#include "tin/sync/mutex.h"
#include "tin/sync/wait_group.h"
```

**保留 `tin/all.h` 作为兼容入口**，但内容改为只包含上面几个顶层头：

```cpp
// tin/all.h - deprecated, kept for backwards compatibility
#include "tin/tin.h"
#include "tin/config.h"
#include "tin/error.h"
#include "tin/time.h"
#include "tin/runtime.h"
#include "tin/net/tcp.h"
#include "tin/sync/mutex.h"
#include "tin/sync/wait_group.h"
#include "tin/sync/chan.h"
#include "tin/sync/atomic.h"
#include "tin/io/io.h"
```

### 4.2 用 `Runtime` 类替代全局状态（解决 §3.3 ~ §3.9）

```cpp
// tin/tin.h
namespace tin {

class Config;

// RAII 包装的 tin runtime。一个实例 = 一个独立的调度器。
// 构造时初始化，析构时清理。不可拷贝、不可移动。
class Runtime {
 public:
  Runtime();
  explicit Runtime(const Config& conf);
  ~Runtime();

  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  // 启动调度器，在内部 greenlet 里调用 entry。阻塞直到 entry 返回
  // 或 Shutdown() 被调用。返回 entry 的返回值。
  int Run(std::function<int(int, char**)> entry, int argc, char** argv);

  // 异步启动：立即返回，entry 在后台 greenlet 里跑。
  void Start(std::function<int(int, char**)> entry, int argc, char** argv);

  // 等待 Start'd runtime 结束。返回 entry 的返回值。
  int Wait();

  // 请求优雅停机：通知 scheduler 停止派发新 G，等当前 G 跑完。
  // 可在任意线程调用。Run/Wait 会在所有 G 退出后返回。
  void Shutdown();

  // 配置访问。SetConfig 只在 Run/Start 之前调用有效。
  const Config& config() const;
  void set_config(const Config& conf);
};

// 便捷函数：等价于 Runtime rt(conf); return rt.Run(entry, argc, argv);
int Run(std::function<int(int, char**)> entry, int argc, char** argv,
        const Config& conf = Config::Default());

// 便捷工厂。
Config DefaultConfig();

}  // namespace tin
```

**对应的用户代码：**

```cpp
#include "tin/tin.h"
#include "tin/net/tcp.h"
#include "tin/runtime.h"

int TinMain(int argc, char** argv) {
  auto listener = tin::net::ListenTcp("0.0.0.0", 2222);
  while (true) {
    auto conn = listener->Accept();
    tin::Spawn([conn]() { /* echo */ });
  }
  return 0;
}

int main(int argc, char** argv) {
  tin::Config conf = tin::DefaultConfig();
  conf.SetMaxProcs(4);
  return tin::Run(TinMain, argc, argv, conf);
}
```

**对比现有 API 解决的问题：**

| 问题 | 旧 API | 新 API |
|---|---|---|
| 全局状态 | `rtm_env`、`sched` 都是全局变量 | `Runtime` 实例持有所有状态 |
| 调试代码 | `Initialize` 打印 "My name is John" | 构造函数干净 |
| `_exit(0)` | `OnMainExit` 暴力退出 | `Run` 自然返回 |
| 清理 | `Deinitialize` 走不到 | 析构函数保证清理 |
| EntryFn | C 函数指针 | `std::function` |
| Config | 可空裸指针 | `const Config&` 重载 |
| 多实例 | 不可能 | 一个进程可以多个 Runtime |
| Shutdown | 没有 | `Runtime::Shutdown()` |
| 生命周期顺序校验 | 没有 | 构造/析构自然保证 |

### 4.3 修复 `OnMainExit` 的 `_exit(0)`（解决 §3.2）

[env.cc:74](file:///d:/home/dev/code/cpp/202607/tin/tin/runtime/env.cc#L74) 改为：

```cpp
void Env::OnMainExit() {
  exit_flag_ = true;
  ThreadPoll::GetInstance()->JoinAll();
  timer_q->Join();
  rtm_env->main_signal_.Notify();   // 唤醒 WaitForPowerOff
  // 不要 _exit，让控制流回到 main 函数
}
```

这样 `WaitForPowerOff` 才真正能返回，`Deinitialize` 才能执行，RAII 析构才能跑。

### 4.4 移除 `using namespace cliff;`（解决 §2.2）

[tin/sync/atomic.h](file:///d:/home/dev/code/cpp/202607/tin/tin/sync/atomic.h) 重写为基于 `std::atomic` 的薄封装，不再依赖 cliff：

```cpp
namespace tin::atomic {

inline bool cas32(std::atomic<uint32_t>* ptr, uint32_t old_v, uint32_t new_v) {
  return ptr->compare_exchange_strong(old_v, new_v, std::memory_order_acq_rel);
}
// ... 其他接口同理

}  // namespace tin::atomic
```

### 4.5 加 CMake install 规则（解决 §2.5）

在 [CMakeLists.txt](file:///d:/home/dev/code/cpp/202607/tin/CMakeLists.txt) 末尾加：

```cmake
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS tin
  EXPORT tinTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY tin/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tin
  FILES_MATCHING PATTERN "*.h"
)

install(EXPORT tinTargets
  FILE tinTargets.cmake
  NAMESPACE tin::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tin
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tinConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/tinConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tin
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tinConfig.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tin
)
```

然后下游项目就能：

```cmake
find_package(tin REQUIRED)
target_link_libraries(my_app PRIVATE tin::tin)
```

### 4.6 统一命名空间风格（解决 §2.6）

全项目改用 `namespace tin::net { ... }` 形式（Google C++ Style Guide 推荐的简洁形式）。

---

## 五、迁移计划（按优先级）

| 优先级 | 任务 | 影响范围 | 兼容性 |
|---|---|---|---|
| **P0** | 删掉 `Initialize()` 里的 `StrFormat` + `cout` 调试代码 | tin.cc | 100% 兼容 |
| **P0** | 修 `OnMainExit` 的 `_exit(0)`，让 `WaitForPowerOff` 真正返回 | env.cc | example 行为变化（会走 Deinitialize） |
| **P1** | 拆分 `runtime.h` 杂物间为 `error.h` / `time.h` / `runtime.h` | 新建文件，旧文件保留 include 转发 | 100% 兼容 |
| **P1** | 提供 `tin::Runtime` 类，保留旧 free function 作为 wrapper | 新增 tin.h API | 100% 兼容 |
| **P1** | 加 CMake install 规则 | CMakeLists.txt | 100% 兼容 |
| **P2** | 把 `EntryFn` 升级为 `std::function` | tin.h | 旧 C 函数指针仍能隐式转换 |
| **P2** | 重写 `tin/sync/atomic.h` 去掉 `using namespace cliff` | sync/atomic.h | API 保持兼容，实现切换 |
| **P2** | 把 `tin/runtime/spawn.h` 提升到 `tin/runtime.h`（公共位置） | 新头文件，旧路径保留 include 转发 | 100% 兼容 |
| **P3** | 废弃 `all.h`，文档推荐模块化 include | 文档 | all.h 仍可用但 deprecated |
| **P3** | 全项目命名空间风格统一 | 全项目 | 纯重构 |

---

## 六、总结

当前 tin 的对外 API 设计有以下核心问题：

1. **`all.h` kitchen-sink 头文件**让用户被迫依赖一切，编译慢、模块边界模糊。
2. **`tin/runtime/runtime.h` 是杂物间**，错误码、时间、调度控制混在一起。
3. **`Initialize()` 里有调试代码**，每次启动打印 "My name is John"。
4. **`OnMainExit` 用 `_exit(0)` 暴力退出**，`WaitForPowerOff` / `Deinitialize` 形同虚设，RAII 析构走不到。
5. **全局单例状态**（`rtm_env` / `sched` / `timer_q`），无法多实例、不可测试、不可嵌入。
6. **`EntryFn` 是 C 函数指针**，不能用带捕获的 lambda。
7. **没有 graceful shutdown**，无法处理 SIGTERM、热重启、测试清理。
8. **没有 CMake install 规则**，下游项目无法 `find_package(tin)`。

推荐的核心改造：**引入 `tin::Runtime` 类替代全局状态 + 拆分 `all.h` 为模块化头文件 + 修 `_exit(0)` 让清理流程真正执行**。这套改造可以做到完全向后兼容（旧 free function 作为 wrapper 保留），同时为未来的多实例、可测试、可嵌入打下基础。
