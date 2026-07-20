# Tin 生命周期 API 设计方案

> **状态**: ✅ 方案 B 已采纳并实施  
> **日期**: 2026-07-20  
> **目标**: 为 tin 运行时设计直观、安全、符合 C++ 惯用法的生命周期 API  
> **决策**: 采用方案 B（`tin::Run` + 底层 API），删除 `Runtime` 类，新增 `Stop()`/`StopRequested()`

---

## 一、问题分析

### 1.1 当前 API 的问题

```cpp
// 当前 API（echo 示例的 main）
int main(int argc, char** argv) {
  tin::Initialize();                          // ① 初始化全局状态
  tin::Config config = tin::DefaultConfig();
  config.SetMaxProcs(std::thread::hardware_concurrency());
  tin::PowerOn(TinMain, argc, argv, &config); // ② 启动调度器
  tin::WaitForPowerOff();                     // ③ 阻塞等待
  tin::Deinitialize();                        // ④ 清理
  return 0;
}
```

**用户反馈**：`Runtime` 类不直观，`PowerOn`/`Initialize` 那套有必要保留。

**根本原因**：
1. `Runtime` 类的 `Run()` 方法把 ②③ 合并了，但用户无法在启动前灵活配置
2. `PowerOn` 是异步的（启动调度器后立即返回），`WaitForPowerOff` 才阻塞——这种"拆分式"语义对服务器场景更自然
3. `Runtime` 的 RAII 析构在异常路径下行为不直观（调度器还在跑就析构了？）
4. 全局单例 `rtm_env` 的存在使得"多 Runtime"不可能，RAII 承诺了它做不到的事

### 1.2 业界参考

| 框架 | API 风格 | 入口示例 |
|------|----------|----------|
| **Go** | 隐式运行时 | `func main() { go f(); select{} }` — 运行时由 runtime 自举，用户不感知 |
| **Rust tokio** | 宏 + block_on | `#[tokio::main] async fn main() { ... }` 或 `tokio::runtime::Runtime::new().block_on(fut)` |
| **Rust async-std** | 宏 + block_on | `async_std::task::block_on(async { ... })` |
| **Python asyncio** | 显式事件循环 | `asyncio.run(main())` — 单行入口，RAII 管理 |
| **folly** | Executor + block | `folly::runInBkgnd(...)` 或手动 `IOThreadPoolExecutor` |
| **brpc** | Server + Start | `server.Start(port, &options); server.RunUntilAskedToQuit()` |

**共同模式**：
1. 都有一个"启动运行时 → 运行用户代码 → 等待结束 → 清理"的生命周期
2. 入口函数签名统一（函数 + 参数），不暴露配置细节
3. 配置通过 builder 或 options 对象传递，不混入入口函数

---

## 二、方案

### 方案 A：保留 PowerOn/Initialize，去除 Runtime 类（用户偏好）

**设计理念**：`PowerOn`/`WaitForPowerOff`/`Deinitialize` 对服务器场景最直观。去掉 `Runtime` 类避免"RAII 承诺了多实例但实际做不到"的误导。

#### 头文件

```cpp
// include/tin/tin.h

namespace tin {

using EntryFn = std::function<int(int argc, char** argv)>;

// ── 配置 ──────────────────────────────────────────────
Config DefaultConfig();
Config* GetWorkingConfig();

// ── 生命周期（显式四步）──────────────────────────────
// 调用顺序必须是：Initialize → PowerOn → WaitForPowerOff → Deinitialize

// Step 1: 初始化平台层和全局配置。可安全调用多次（幂等）。
void Initialize();

// Step 2: 启动调度器，在协程中执行 entry。立即返回（非阻塞）。
//         new_conf 为 nullptr 时使用 Initialize() 时的默认配置。
void PowerOn(EntryFn entry, int argc, char** argv, Config* new_conf = nullptr);

// Step 3: 阻塞当前线程，直到调度器退出（entry 返回或 Stop 被调用）。
//         返回值是 entry 函数的返回值。
int WaitForPowerOff();

// Step 4: 停止调度器并释放所有资源。必须在 WaitForPowerOff 之后调用。
void Deinitialize();

// ── 停止 ──────────────────────────────────────────────
// 请求调度器停止。可从任意协程调用。
// 调用后 WaitForPowerOff 将返回。
void Stop(int exit_code = 0);

}  // namespace tin
```

#### 用法 1：基础 echo 服务器

```cpp
#include "tin/tin.h"
#include "tin/config.h"
#include "tin/net/tcp.h"
#include <absl/log/log.h>
#include <thread>

// ── 协程入口（运行在调度器管理的协程中）──────────────────
int TinMain(int argc, char** argv) {
  auto listen_result = tin::net::ListenTcp("0.0.0.0", 2222);
  if (!listen_result.ok()) {
    LOG(FATAL) << "Listen failed: " << listen_result.error().ToString();
  }
  tin::net::TcpListener listener = std::move(listen_result.value());
  LOG(INFO) << "echo server listening on :2222";

  while (true) {
    auto accept_result = listener.Accept();
    if (accept_result.ok()) {
      tin::Spawn(&HandleClient, std::move(accept_result.value()));
    }
  }
  return 0;
}

// ── main（运行在 OS 主线程，负责生命周期管理）────────────
int main(int argc, char** argv) {
  // ① 初始化平台层
  tin::Initialize();

  // ② 配置
  tin::Config config = tin::DefaultConfig();
  config.SetMaxProcs(static_cast<int>(std::thread::hardware_concurrency()));

  // ③ 启动调度器（异步，立即返回）
  tin::PowerOn(TinMain, argc, argv, &config);

  // ④ 阻塞等待 TinMain 返回
  int ret = tin::WaitForPowerOff();

  // ⑤ 清理
  tin::Deinitialize();
  return ret;
}
```

#### 用法 2：信号驱动的优雅退出

```cpp
#include <csignal>

std::atomic<bool> g_stop{false};

void SignalHandler(int sig) {
  g_stop.store(true);
  tin::Stop(0);  // 通知调度器停止，WaitForPowerOff 将返回
}

int TinMain(int argc, char** argv) {
  auto listener = tin::net::ListenTcp("0.0.0.0", 2222).value();
  while (!g_stop.load()) {
    auto conn = listener.Accept().value();
    tin::Spawn(&HandleClient, std::move(conn));
  }
  LOG(INFO) << "shutting down...";
  return 0;
}

int main(int argc, char** argv) {
  tin::Initialize();

  tin::Config config = tin::DefaultConfig();
  config.SetMaxProcs(4);

  tin::PowerOn(TinMain, argc, argv, &config);

  // 注册信号（必须在 PowerOn 之后，因为需要调度器已启动）
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  int ret = tin::WaitForPowerOff();
  tin::Deinitialize();
  return ret;
}
```

#### 用法 3：最简示例（simple）

```cpp
#include "tin/tin.h"
#include <absl/log/log.h>

int TinMain(int argc, char** argv) {
  LOG(INFO) << "Hello from greenlet!";
  return 0;
}

int main(int argc, char** argv) {
  tin::Initialize();
  tin::PowerOn(TinMain, argc, argv);  // 使用默认配置
  int ret = tin::WaitForPowerOff();
  tin::Deinitialize();
  return ret;
}
```

**优点**：
- 最简单直观，四步线性流程
- 配置和启动分离，灵活
- `PowerOn` 异步返回，可以在启动后做其他事（如注册信号处理）
- 不假装支持多实例

**缺点**：
- 不是 RAII，忘记 `Deinitialize` 会泄漏
- 全局状态，无法嵌套

---

### 方案 B：单函数入口 `tin::Run` + 底层 API（tokio/asyncio 风格）

**设计理念**：提供一行搞定的入口，同时保留底层 API 供高级用户使用。

#### 头文件

```cpp
// include/tin/tin.h

namespace tin {

using EntryFn = std::function<int(int argc, char** argv)>;

// ── 便捷入口（推荐 90% 用户使用）─────────────────────
// 一行搞定：初始化 → 配置 → 启动 → 等待 → 清理。
// 返回 entry 的退出码。
int Run(EntryFn entry, int argc, char** argv);
int Run(EntryFn entry, int argc, char** argv, const Config& conf);

// ── 底层 API（高级用户使用）──────────────────────────
// 与方案 A 完全相同
void Initialize();
void PowerOn(EntryFn entry, int argc, char** argv, Config* new_conf = nullptr);
int  WaitForPowerOff();
void Deinitialize();
void Stop(int exit_code = 0);

// ── 配置 ──────────────────────────────────────────────
Config DefaultConfig();
Config* GetWorkingConfig();

}  // namespace tin
```

#### 用法 1：最简示例（一行启动）

```cpp
#include "tin/tin.h"
#include <absl/log/log.h>

int TinMain(int argc, char** argv) {
  LOG(INFO) << "Hello from greenlet!";
  return 0;
}

int main(int argc, char** argv) {
  return tin::Run(TinMain, argc, argv);
}
```

#### 用法 2：带配置的 echo 服务器（一行启动）

```cpp
#include "tin/tin.h"
#include "tin/net/tcp.h"
#include <absl/log/log.h>
#include <thread>

int TinMain(int argc, char** argv) {
  auto listener = tin::net::ListenTcp("0.0.0.0", 2222).value();
  LOG(INFO) << "echo server listening on :2222";
  while (true) {
    auto conn = listener.Accept().value();
    tin::Spawn(&HandleClient, std::move(conn));
  }
  return 0;
}

int main(int argc, char** argv) {
  tin::Config config = tin::DefaultConfig();
  config.SetMaxProcs(static_cast<int>(std::thread::hardware_concurrency()));
  return tin::Run(TinMain, argc, argv, config);
}
```

#### 用法 3：信号驱动的优雅退出（底层 API）

```cpp
#include "tin/tin.h"
#include <csignal>
#include <absl/log/log.h>

std::atomic<bool> g_stop{false};

void SignalHandler(int) {
  g_stop.store(true);
  tin::Stop(0);
}

int TinMain(int argc, char** argv) {
  auto listener = tin::net::ListenTcp("0.0.0.0", 2222).value();
  while (!g_stop.load()) {
    auto conn = listener.Accept().value();
    tin::Spawn(&HandleClient, std::move(conn));
  }
  LOG(INFO) << "graceful shutdown complete";
  return 0;
}

int main(int argc, char** argv) {
  tin::Initialize();

  tin::Config config = tin::DefaultConfig();
  config.SetMaxProcs(4);

  tin::PowerOn(TinMain, argc, argv, &config);

  // 信号注册必须在 PowerOn 之后
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  int ret = tin::WaitForPowerOff();
  tin::Deinitialize();
  return ret;
}
```

#### 用法 4：在启动前检查环境（底层 API）

```cpp
int main(int argc, char** argv) {
  tin::Initialize();

  // 启动前做环境检查
  if (std::thread::hardware_concurrency() < 2) {
    LOG(WARNING) << "Running on single-core, performance may be limited";
  }

  // 动态决定配置
  tin::Config config = tin::DefaultConfig();
  config.SetMaxProcs(std::max(2, static_cast<int>(std::thread::hardware_concurrency())));

  // 也可以读取命令行参数
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--stack-protect") {
      config.EnableStackProtection(true);
    }
  }

  tin::PowerOn(TinMain, argc, argv, &config);
  int ret = tin::WaitForPowerOff();
  tin::Deinitialize();
  return ret;
}
```

**优点**：
- `tin::Run` 一行搞定，最简洁
- 底层 API 保留，灵活性最高
- 不误导用户（没有假装 RAII 的 `Runtime` 类）

**缺点**：
- 两种入口可能让新用户困惑（用哪个？）
- `Run` 是阻塞的，无法在启动后注册信号处理

---

### 方案 C：Builder 模式（配置 → 构建 → 运行）

**设计理念**：用 Builder 链式调用解决"配置复杂时不够直观"的问题，同时保持显式的生命周期。

#### 头文件

```cpp
// include/tin/tin.h

namespace tin {

using EntryFn = std::function<int(int argc, char** argv)>;

// ── Builder ───────────────────────────────────────────
class RuntimeBuilder {
 public:
  RuntimeBuilder();

  // 链式配置
  RuntimeBuilder& SetMaxProcs(int n);
  RuntimeBuilder& SetStackSize(int bytes);
  RuntimeBuilder& SetOsThreadStackSize(int bytes);
  RuntimeBuilder& EnableStackProtection(bool enable = true);
  RuntimeBuilder& IgnoreSigpipe(bool ignore = true);
  RuntimeBuilder& SetConfig(const Config& conf);

  // 构建并启动（阻塞直到 entry 返回）
  int Run(EntryFn entry, int argc, char** argv);

  // 或者：构建并异步启动
  // 返回一个 RuntimeHandle，可后续 Wait/Stop
  RuntimeHandle Build(EntryFn entry, int argc, char** argv);

 private:
  Config conf_;
};

// ── 异步句柄（Build 模式使用）─────────────────────────
class RuntimeHandle {
 public:
  // 阻塞等待 entry 返回
  int Wait();

  // 请求停止
  void Stop(int exit_code = 0);

  // 析构时自动 Wait + 清理
  ~RuntimeHandle();

  RuntimeHandle(const RuntimeHandle&) = delete;
  RuntimeHandle& operator=(const RuntimeHandle&) = delete;
  RuntimeHandle(RuntimeHandle&&) noexcept;
  RuntimeHandle& operator=(RuntimeHandle&&) noexcept;

 private:
  friend class RuntimeBuilder;
  explicit RuntimeHandle(/* ... */);
  bool owns_ = false;
};

// ── 便捷函数 ──────────────────────────────────────────
Config DefaultConfig();

}  // namespace tin
```

#### 用法 1：最简示例（Builder 一行启动）

```cpp
#include "tin/tin.h"
#include <absl/log/log.h>

int TinMain(int argc, char** argv) {
  LOG(INFO) << "Hello from greenlet!";
  return 0;
}

int main(int argc, char** argv) {
  return tin::RuntimeBuilder().Run(TinMain, argc, argv);
}
```

#### 用法 2：带配置的 echo 服务器（链式调用）

```cpp
#include "tin/tin.h"
#include "tin/net/tcp.h"
#include <absl/log/log.h>
#include <thread>

int TinMain(int argc, char** argv) {
  auto listener = tin::net::ListenTcp("0.0.0.0", 2222).value();
  LOG(INFO) << "echo server listening on :2222";
  while (true) {
    auto conn = listener.Accept().value();
    tin::Spawn(&HandleClient, std::move(conn));
  }
  return 0;
}

int main(int argc, char** argv) {
  return tin::RuntimeBuilder()
      .SetMaxProcs(static_cast<int>(std::thread::hardware_concurrency()))
      .EnableStackProtection(true)
      .IgnoreSigpipe(true)
      .Run(TinMain, argc, argv);
}
```

#### 用法 3：信号驱动的优雅退出（Build + Handle）

```cpp
#include "tin/tin.h"
#include <csignal>
#include <absl/log/log.h>

std::atomic<bool> g_stop{false};

void SignalHandler(int) {
  g_stop.store(true);
  tin::Stop(0);
}

int TinMain(int argc, char** argv) {
  auto listener = tin::net::ListenTcp("0.0.0.0", 2222).value();
  while (!g_stop.load()) {
    auto conn = listener.Accept().value();
    tin::Spawn(&HandleClient, std::move(conn));
  }
  LOG(INFO) << "graceful shutdown complete";
  return 0;
}

int main(int argc, char** argv) {
  // Build 异步启动，返回 handle
  auto handle = tin::RuntimeBuilder()
      .SetMaxProcs(4)
      .EnableStackProtection()
      .Build(TinMain, argc, argv);

  // 注册信号
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  // 阻塞等待，handle 析构时自动清理
  return handle.Wait();
}
```

#### 用法 4：带命令行解析的完整服务器

```cpp
#include "tin/tin.h"
#include <absl/log/log.h>
#include <absl/flags/flag.h>
#include <thread>

ABSL_FLAG(int, port, 2222, "listen port");
ABSL_FLAG(int, procs, 0, "max procs (0=auto)");

int TinMain(int argc, char** argv) {
  uint16_t port = static_cast<uint16_t>(absl::GetFlag(FLAGS_port));
  auto listener = tin::net::ListenTcp("0.0.0.0", port).value();
  LOG(INFO) << "listening on :" << port;
  while (true) {
    auto conn = listener.Accept().value();
    tin::Spawn(&HandleClient, std::move(conn));
  }
  return 0;
}

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  int procs = absl::GetFlag(FLAGS_procs);
  if (procs <= 0) {
    procs = static_cast<int>(std::thread::hardware_concurrency());
  }

  return tin::RuntimeBuilder()
      .SetMaxProcs(procs)
      .Run(TinMain, argc, argv);
}
```

**优点**：
- Builder 链式调用，配置直观
- `Run()` 一行搞定简单场景
- `Build()` + `Handle` 支持异步场景
- `RuntimeHandle` 析构自动清理，半 RAII

**缺点**：
- API 表面积最大（Builder + Handle + Run）
- `RuntimeHandle` 的移动语义增加了复杂度
- 对简单场景来说过度设计

---

### 方案 D：完全重写——Go 风格隐式运行时 + 信号驱动

**设计理念**：Go 的运行时是隐式的——用户不需要 `Initialize`/`PowerOn`，直接 `go f()` 就行。tin 可以做到类似效果：`tin::Run` 自动处理信号，用户只关心业务逻辑。

#### 头文件

```cpp
// include/tin/tin.h

namespace tin {

using EntryFn = std::function<int(int argc, char** argv)>;

// ── 配置（必须在 Run 之前调用）────────────────────────
Config& MutableConfig();  // 返回全局 Config 的引用，可在 Run 前修改
Config DefaultConfig();

// ── 单入口 ───────────────────────────────────────────
// 初始化运行时 → 在协程中执行 entry → 阻塞等待 → 清理。
// 内部自动处理 SIGINT/SIGTERM（调用 tin::Stop）。
int Run(EntryFn entry, int argc, char** argv);

// ── 停止 ──────────────────────────────────────────────
void Stop(int exit_code = 0);

// ── 底层控制（高级用户）──────────────────────────────
// 显式初始化，用于不想用 Run 的场景
void Init(const Config& conf = DefaultConfig());
void Shutdown();

}  // namespace tin
```

#### 用法 1：最简示例（真正的一行）

```cpp
#include "tin/tin.h"
#include <absl/log/log.h>

int TinMain(int argc, char** argv) {
  LOG(INFO) << "Hello from greenlet!";
  return 0;
}

int main(int argc, char** argv) {
  return tin::Run(TinMain, argc, argv);
  // Ctrl+C 自动停止
}
```

#### 用法 2：带配置的 echo 服务器

```cpp
#include "tin/tin.h"
#include "tin/net/tcp.h"
#include <absl/log/log.h>
#include <thread>

int TinMain(int argc, char** argv) {
  auto listener = tin::net::ListenTcp("0.0.0.0", 2222).value();
  LOG(INFO) << "echo server listening on :2222";
  while (true) {
    auto conn = listener.Accept().value();
    tin::Spawn(&HandleClient, std::move(conn));
  }
  return 0;
}

int main(int argc, char** argv) {
  // 在 Run 之前修改全局配置
  tin::MutableConfig().SetMaxProcs(
      static_cast<int>(std::thread::hardware_concurrency()));

  return tin::Run(TinMain, argc, argv);
  // Ctrl+C 自动触发 Stop，Run 返回
}
```

#### 用法 3：自定义信号处理（通过回调钩子）

```cpp
#include "tin/tin.h"
#include <absl/log/log.h>

int TinMain(int argc, char** argv) {
  auto listener = tin::net::ListenTcp("0.0.0.0", 2222).value();
  LOG(INFO) << "echo server listening on :2222";

  // 接受连接直到被停止
  while (true) {
    auto conn = listener.Accept().value();
    tin::Spawn(&HandleClient, std::move(conn));
  }
  return 0;
}

int main(int argc, char** argv) {
  tin::MutableConfig().SetMaxProcs(4);
  // Run 内部自动注册 SIGINT/SIGTERM → tin::Stop(0)
  // 用户无需手动处理信号
  return tin::Run(TinMain, argc, argv);
}
```

#### 用法 4：底层 API（不使用自动信号处理）

```cpp
#include "tin/tin.h"
#include <csignal>
#include <absl/log/log.h>

int TinMain(int argc, char** argv) {
  // ... server logic ...
  return 0;
}

int main(int argc, char** argv) {
  // 不想用 Run 的自动信号？用底层 API
  tin::Config config = tin::DefaultConfig();
  config.SetMaxProcs(4);
  tin::Init(config);

  // 自己注册信号
  std::signal(SIGINT, [](int) { tin::Stop(0); });

  // 手动启动 entry（这里用 Spawn 而非 PowerOn）
  tin::Spawn(TinMain, argc, argv);

  // 阻塞等待
  tin::WaitForExit();  // 内部等价于 WaitForPowerOff
  tin::Shutdown();
  return 0;
}
```

**优点**：
- 极简，一行入口
- 自动信号处理（SIGINT/SIGTERM → Stop）
- `MutableConfig()` 直接修改全局配置，不需要 builder 或参数传递

**缺点**：
- 全局可变状态（`MutableConfig` 返回引用）不安全
- `Init`/`Shutdown` 的存在意味着 `Run` 不是唯一入口，仍有两种模式
- 与当前架构差异最大，改动量最大
- Go 的隐式运行时有编译器支持，C++ 做不到完全隐式

---

### 方案 E：Scope Guard RAII（析构即清理，但不假装多实例）

**设计理念**：用一个轻量 RAII 类包装 `Initialize`/`Deinitialize`，但 **不** 包装 `PowerOn`/`WaitForPowerOff`。这样既有了 RAII 的安全性，又保留了 `PowerOn` 的异步语义。类名不叫 `Runtime`（避免暗示多实例），而叫 `RuntimeScope`——明确表示它只是一个 scope guard。

#### 头文件

```cpp
// include/tin/tin.h

namespace tin {

using EntryFn = std::function<int(int argc, char** argv)>;

// ── RAII scope guard ──────────────────────────────────
// 构造时 Initialize()，析构时 Deinitialize()。
// 不管理 PowerOn/WaitForPowerOff —— 那些由用户显式调用。
class RuntimeScope {
 public:
  RuntimeScope();                    // Initialize() + DefaultConfig
  explicit RuntimeScope(const Config& conf);  // Initialize() + 自定义配置
  ~RuntimeScope();                   // Deinitialize()

  RuntimeScope(const RuntimeScope&) = delete;
  RuntimeScope& operator=(const RuntimeScope&) = delete;

  // 访问配置（可在 PowerOn 前修改）
  Config& MutableConfig() { return conf_; }
  const Config& config() const { return conf_; }

 private:
  Config conf_;
};

// ── 生命周期（PowerOn / Wait / Stop）──────────────────
void PowerOn(EntryFn entry, int argc, char** argv);
void PowerOn(EntryFn entry, int argc, char** argv, const Config& conf);
int  WaitForPowerOff();
void Stop(int exit_code = 0);

// ── 便捷入口 ──────────────────────────────────────────
int Run(EntryFn entry, int argc, char** argv);
int Run(EntryFn entry, int argc, char** argv, const Config& conf);

// ── 配置 ──────────────────────────────────────────────
Config DefaultConfig();
Config* GetWorkingConfig();

}  // namespace tin
```

#### 用法 1：最简示例（Run 一行）

```cpp
#include "tin/tin.h"
#include <absl/log/log.h>

int TinMain(int argc, char** argv) {
  LOG(INFO) << "Hello from greenlet!";
  return 0;
}

int main(int argc, char** argv) {
  return tin::Run(TinMain, argc, argv);
}
```

#### 用法 2：echo 服务器（RuntimeScope + PowerOn）

```cpp
#include "tin/tin.h"
#include "tin/net/tcp.h"
#include <absl/log/log.h>
#include <thread>

int TinMain(int argc, char** argv) {
  auto listener = tin::net::ListenTcp("0.0.0.0", 2222).value();
  LOG(INFO) << "echo server listening on :2222";
  while (true) {
    auto conn = listener.Accept().value();
    tin::Spawn(&HandleClient, std::move(conn));
  }
  return 0;
}

int main(int argc, char** argv) {
  // RAII：构造时 Initialize，析构时 Deinitialize
  tin::RuntimeScope scope;

  // 配置
  scope.MutableConfig().SetMaxProcs(
      static_cast<int>(std::thread::hardware_concurrency()));

  // 启动 + 等待
  tin::PowerOn(TinMain, argc, argv, scope.config());
  int ret = tin::WaitForPowerOff();

  // scope 析构自动 Deinitialize
  return ret;
}
```

#### 用法 3：信号驱动的优雅退出

```cpp
#include "tin/tin.h"
#include <csignal>
#include <absl/log/log.h>

std::atomic<bool> g_stop{false};

void SignalHandler(int) {
  g_stop.store(true);
  tin::Stop(0);
}

int TinMain(int argc, char** argv) {
  auto listener = tin::net::ListenTcp("0.0.0.0", 2222).value();
  while (!g_stop.load()) {
    auto conn = listener.Accept().value();
    tin::Spawn(&HandleClient, std::move(conn));
  }
  LOG(INFO) << "graceful shutdown complete";
  return 0;
}

int main(int argc, char** argv) {
  tin::RuntimeScope scope;
  scope.MutableConfig().SetMaxProcs(4);

  tin::PowerOn(TinMain, argc, argv, scope.config());

  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  int ret = tin::WaitForPowerOff();
  // scope 析构 → Deinitialize
  return ret;
}
```

#### 用法 4：异常安全（RAII 的核心价值）

```cpp
int main(int argc, char** argv) {
  tin::RuntimeScope scope;
  scope.MutableConfig().SetMaxProcs(4);

  try {
    // 可能在 PowerOn 之前抛异常
    ValidateEnvironment();

    tin::PowerOn(TinMain, argc, argv, scope.config());
    int ret = tin::WaitForPowerOff();

    // 即使这里 return，scope 也会析构清理
    return ret;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Fatal: " << e.what();
    // scope 析构 → Deinitialize，不会泄漏
    return 1;
  }
}
```

**优点**：
- RAII 保证清理，异常安全
- `RuntimeScope` 名字明确表达"作用域"而非"运行时实例"
- `PowerOn`/`WaitForPowerOff` 保留，异步语义完整
- `tin::Run` 便捷入口仍在

**缺点**：
- `RuntimeScope` 和 `PowerOn` 两层 API，比方案 A 多一层概念
- `MutableConfig()` 返回引用可能被误用（在 PowerOn 后修改）

---

## 三、对比总结

| 维度 | A: PowerOn 四步 | B: Run + 底层 | C: Builder | D: Go 风格 | E: Scope Guard |
|------|:---:|:---:|:---:|:---:|:---:|
| **简洁性** | ★★★☆☆ | ★★★★★ | ★★★★☆ | ★★★★★ | ★★★★☆ |
| **灵活性** | ★★★★★ | ★★★★★ | ★★★★☆ | ★★★☆☆ | ★★★★★ |
| **安全性** | ★★☆☆☆ | ★★★☆☆ | ★★★★☆ | ★★★☆☆ | ★★★★★ |
| **改动量** | 极小 | 小 | 中 | 大 | 小 |
| **与现有兼容** | ✅ | ✅ | ❌ | ❌ | ✅ |
| **服务器场景** | ✅ | ✅ | ✅ | ⚠️ | ✅ |
| **学习成本** | 低 | 低 | 中 | 低 | 低 |
| **RAII** | ❌ | ❌ | 半 RAII | ❌ | ✅ |
| **异常安全** | ❌ | ❌ | ✅ | ❌ | ✅ |

---

## 四、推荐

**推荐方案 B**（`Run` + 底层 API）。

理由：
1. **90% 的用户用 `tin::Run` 一行搞定**，足够简洁
2. **底层 API 完整保留**，服务器场景可用 `PowerOn`/`WaitForPowerOff` 异步启动
3. **去掉 `Runtime` 类**，避免"RAII 承诺多实例但做不到"的误导
4. **改动量最小**，与现有 echo/simple 示例完全兼容
5. **新增 `Stop()`**，让优雅退出有标准 API

### 实施计划（方案 B）

1. 删除 `Runtime` 类和 `[[deprecated]]` 标记
2. 去掉 `Initialize`/`PowerOn`/`WaitForPowerOff`/`Deinitialize` 上的 `[[deprecated]]`
3. 新增 `void Stop(int exit_code = 0)` 函数
4. 保留 `tin::Run` 便捷函数
5. 更新 echo/simple 示例
6. 更新文档

**预计改动量**：`tin.h`、`tin.cc`、`echo.cc`、`simple.cc`，约 50 行。

---

## 五、附录：各方案完整头文件

### 方案 A 头文件

```cpp
#ifndef TIN_TIN_H_
#define TIN_TIN_H_

#include <functional>
#include "tin/config.h"

namespace tin {

using EntryFn = std::function<int(int argc, char** argv)>;

// Configuration
Config DefaultConfig();
Config* GetWorkingConfig();

// Lifecycle: Initialize → PowerOn → WaitForPowerOff → Deinitialize
void Initialize();
void PowerOn(EntryFn entry, int argc, char** argv, Config* new_conf = nullptr);
int  WaitForPowerOff();
void Deinitialize();

// Request the scheduler to stop. Can be called from any greenlet.
// After this, WaitForPowerOff() returns.
void Stop(int exit_code = 0);

}  // namespace tin

#endif  // TIN_TIN_H_
```

### 方案 B 头文件

```cpp
#ifndef TIN_TIN_H_
#define TIN_TIN_H_

#include <functional>
#include "tin/config.h"

namespace tin {

using EntryFn = std::function<int(int argc, char** argv)>;

// ── Convenient entry (recommended for most users) ────
// Equivalent to: Initialize → PowerOn → WaitForPowerOff → Deinitialize
int Run(EntryFn entry, int argc, char** argv);
int Run(EntryFn entry, int argc, char** argv, const Config& conf);

// ── Low-level lifecycle (for servers needing async start) ──
void Initialize();
void PowerOn(EntryFn entry, int argc, char** argv, Config* new_conf = nullptr);
int  WaitForPowerOff();
void Deinitialize();

// ── Stop ──────────────────────────────────────────────
void Stop(int exit_code = 0);

// ── Config ────────────────────────────────────────────
Config DefaultConfig();
Config* GetWorkingConfig();

}  // namespace tin

#endif  // TIN_TIN_H_
```

### 方案 C 头文件

```cpp
#ifndef TIN_TIN_H_
#define TIN_TIN_H_

#include <functional>
#include "tin/config.h"

namespace tin {

using EntryFn = std::function<int(int argc, char** argv)>;

class RuntimeHandle {
 public:
  int Wait();
  void Stop(int exit_code = 0);
  ~RuntimeHandle();
  RuntimeHandle(RuntimeHandle&&) noexcept;
  RuntimeHandle& operator=(RuntimeHandle&&) noexcept;
  RuntimeHandle(const RuntimeHandle&) = delete;
  RuntimeHandle& operator=(const RuntimeHandle&) = delete;
 private:
  friend class RuntimeBuilder;
  RuntimeHandle();
  bool owns_ = false;
};

class RuntimeBuilder {
 public:
  RuntimeBuilder();
  RuntimeBuilder& SetMaxProcs(int n);
  RuntimeBuilder& SetStackSize(int bytes);
  RuntimeBuilder& SetOsThreadStackSize(int bytes);
  RuntimeBuilder& EnableStackProtection(bool enable = true);
  RuntimeBuilder& IgnoreSigpipe(bool ignore = true);
  RuntimeBuilder& SetConfig(const Config& conf);
  int Run(EntryFn entry, int argc, char** argv);
  RuntimeHandle Build(EntryFn entry, int argc, char** argv);
 private:
  Config conf_;
};

Config DefaultConfig();
void Stop(int exit_code = 0);

}  // namespace tin

#endif  // TIN_TIN_H_
```

### 方案 D 头文件

```cpp
#ifndef TIN_TIN_H_
#define TIN_TIN_H_

#include <functional>
#include "tin/config.h"

namespace tin {

using EntryFn = std::function<int(int argc, char** argv)>;

// Single entry point: init → run → wait → cleanup.
// Automatically handles SIGINT/SIGTERM by calling Stop().
int Run(EntryFn entry, int argc, char** argv);

// Configuration (must be called before Run)
Config& MutableConfig();
Config DefaultConfig();

// Explicit lifecycle (advanced)
void Init(const Config& conf = DefaultConfig());
void Shutdown();

// Stop the scheduler
void Stop(int exit_code = 0);

}  // namespace tin

#endif  // TIN_TIN_H_
```

### 方案 E 头文件

```cpp
#ifndef TIN_TIN_H_
#define TIN_TIN_H_

#include <functional>
#include "tin/config.h"

namespace tin {

using EntryFn = std::function<int(int argc, char** argv)>;

// RAII scope guard: Initialize() on construct, Deinitialize() on destruct.
// Does NOT manage PowerOn/WaitForPowerOff — those are explicit.
class RuntimeScope {
 public:
  RuntimeScope();
  explicit RuntimeScope(const Config& conf);
  ~RuntimeScope();
  RuntimeScope(const RuntimeScope&) = delete;
  RuntimeScope& operator=(const RuntimeScope&) = delete;
  Config& MutableConfig() { return conf_; }
  const Config& config() const { return conf_; }
 private:
  Config conf_;
};

// Lifecycle (PowerOn / Wait / Stop)
void PowerOn(EntryFn entry, int argc, char** argv);
void PowerOn(EntryFn entry, int argc, char** argv, const Config& conf);
int  WaitForPowerOff();
void Stop(int exit_code = 0);

// Convenient entry
int Run(EntryFn entry, int argc, char** argv);
int Run(EntryFn entry, int argc, char** argv, const Config& conf);

// Config
Config DefaultConfig();
Config* GetWorkingConfig();

}  // namespace tin

#endif  // TIN_TIN_H_
```
