// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Public API: lifecycle (Run, Initialize, PowerOn, Stop, ...).
// This is the clean public header — it does not include any runtime/
// internals.

#ifndef TIN_H_
#define TIN_H_

#include <functional>

#include "tin/config.h"

namespace tin {

// Entry function signature.
using EntryFn = std::function<int(int argc, char** argv)>;

// ── Convenient entry (recommended for most users) ────────────────────
// One-liner: Initialize → PowerOn → WaitForPowerOff → Deinitialize.
// Returns the entry function's exit code.
int Run(EntryFn entry, int argc, char** argv);
int Run(EntryFn entry, int argc, char** argv, const Config& conf);

// ── Low-level lifecycle (for servers needing async start) ────────────
// Call order: Initialize → PowerOn → WaitForPowerOff → Deinitialize.

// Step 1: Initialize platform layer and global config. Idempotent.
void Initialize();

// Step 2: Start the scheduler and run entry in a coroutine. Returns
//         immediately (non-blocking). If new_conf is null, uses the
//         config from Initialize().
void PowerOn(EntryFn entry, int argc, char** argv, Config* new_conf = nullptr);

// Step 3: Block until the scheduler exits (entry returns or Stop is
//         called). Returns the entry function's exit code.
int WaitForPowerOff();

// Step 4: Stop the scheduler and release all resources.
void Deinitialize();

// ── Stop ─────────────────────────────────────────────────────────────
// Request the scheduler to stop. Can be called from any coroutine or
// from a signal handler on the main thread. After this call,
// WaitForPowerOff() will return.
void Stop(int exit_code = 0);

// Returns true if Stop() has been called (or the runtime is shutting
// down). Check this in your main loop for graceful shutdown.
bool StopRequested();

// ── Config ───────────────────────────────────────────────────────────
Config DefaultConfig();
Config* GetWorkingConfig();

}  // namespace tin

#endif  // TIN_H_
