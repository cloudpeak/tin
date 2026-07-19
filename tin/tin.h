// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <functional>

#include "tin/config/config.h"

namespace tin {

// Entry function signature. Upgraded from a raw C function pointer to
// std::function so that stateful callables (lambdas with captures,
// std::bind results, etc.) can be used as the tin main entry point.
// Raw function pointers and function references convert implicitly, so
// existing code passing `&TinMain` continues to work unchanged.
using EntryFn = std::function<int(int argc, char** argv)>;

// ---------------------------------------------------------------------------
// Legacy free-function lifecycle API.
//
// These are retained for backwards compatibility. New code is encouraged to
// use the `tin::Runtime` class below, which wraps the same machinery with
// RAII semantics. The free functions operate on the same single global
// runtime instance that `Runtime` uses, so mixing the two styles within one
// process is not supported (yet).
// ---------------------------------------------------------------------------

void Initialize();

void PowerOn(EntryFn fn, int argc, char** argv, Config* new_conf);

void PowerOn(EntryFn fn, Config* new_conf);

int WaitForPowerOff();

void Deinitialize();

Config* GetWorkingConfig();

Config DefaultConfig();

// ---------------------------------------------------------------------------
// RAII runtime API.
//
// `Runtime` owns the lifecycle of a tin scheduler instance. Constructing a
// `Runtime` calls `Initialize()`; destroying it calls `Deinitialize()`;
// `Run()` starts the scheduler, blocks until the entry function returns or
// the scheduler exits, and returns the entry function's exit code.
//
// NOTE: the current implementation is backed by the global runtime state
// (`rtm_env` / `sched` / `timer_q`), so only one `Runtime` instance may be
// live per process at this time. The class API is designed so that the
// eventual move to per-instance state is transparent to callers.
// ---------------------------------------------------------------------------

class Runtime {
 public:
  // Default-constructs a runtime using `DefaultConfig()`.
  Runtime();

  // Constructs a runtime using the supplied configuration. The configuration
  // is copied and may be mutated later via `set_config()` provided `Run()`
  // has not yet been called.
  explicit Runtime(const Config& conf);

  // Calls `Deinitialize()`. Must not be called more than once.
  ~Runtime();

  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  // Starts the scheduler and blocks until the entry function returns (or the
  // scheduler exits). Returns the value returned by `entry`.
  //
  // `entry` may be a raw function pointer, a lambda, or any callable
  // convertible to `std::function<int(int, char**)>`.
  int Run(EntryFn entry, int argc, char** argv);

  // Accessors for the runtime configuration. `set_config()` must be called
  // before `Run()`; configuration changes after the scheduler has started
  // are ignored.
  const Config& config() const { return conf_; }
  void set_config(const Config& conf) { conf_ = conf; }

 private:
  Config conf_;
};

// Convenience function: constructs a `Runtime` with `DefaultConfig()`, runs
// `entry`, and returns its exit code.
int Run(EntryFn entry, int argc, char** argv);

// Convenience function: constructs a `Runtime` with `conf`, runs `entry`, and
// returns its exit code.
int Run(EntryFn entry, int argc, char** argv, const Config& conf);

}  // namespace tin
