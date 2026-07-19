// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Public API: lifecycle (Runtime, Run, Initialize, ...).
// This is the clean public header — it does not include any runtime/
// internals.

#ifndef TIN_TIN_H_
#define TIN_TIN_H_

#include <functional>

#include "tin/config.h"

namespace tin {

// Entry function signature.
using EntryFn = std::function<int(int argc, char** argv)>;

// Legacy free-function lifecycle API.
void Initialize();
void PowerOn(EntryFn fn, int argc, char** argv, Config* new_conf);
void PowerOn(EntryFn fn, Config* new_conf);
int WaitForPowerOff();
void Deinitialize();
Config* GetWorkingConfig();
Config DefaultConfig();

// RAII runtime API.
class Runtime {
 public:
  Runtime();
  explicit Runtime(const Config& conf);
  ~Runtime();
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  int Run(EntryFn entry, int argc, char** argv);

  const Config& config() const { return conf_; }
  void set_config(const Config& conf) { conf_ = conf; }

 private:
  Config conf_;
};

int Run(EntryFn entry, int argc, char** argv);
int Run(EntryFn entry, int argc, char** argv, const Config& conf);

}  // namespace tin

#endif  // TIN_TIN_H_
