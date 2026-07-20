// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tin/runtime/env.h"
#include "tin/platform/platform.h"

#include "tin/tin.h"

#include <memory>

namespace tin {

namespace {
std::unique_ptr<Config> conf;
}

void Initialize() {
  conf = std::make_unique<Config>();
  *conf = DefaultConfig();
  PlatformInit();
}

void PowerOn(EntryFn entry, int argc, char** argv, Config* new_conf) {
  if (new_conf != nullptr) {
    *conf = *new_conf;
  }
  runtime::InitializeEnv(std::move(entry), argc, argv, conf.get());
}

int WaitForPowerOff() {
  return runtime::rtm_env->WaitMainExit();
}

void Deinitialize() {
  runtime::DeInitializeEnv();
  conf.reset();
}

void Stop(int exit_code) {
  runtime::RequestStop(exit_code);
}

bool StopRequested() {
  return runtime::StopRequested();
}

Config* GetWorkingConfig() {
  return conf.get();
}

Config DefaultConfig() {
  Config conf;
  conf.SetMaxProcs(1);
  conf.SetStackSize(kDefaultStackSize);
  conf.SetOsThreadStackSize(kDefaultOSThreadStackSize);
  conf.SetIgnoreSigpipe(true);
  conf.EnableStackProtection(false);
  return conf;
}

// ---------------------------------------------------------------------------
// Convenience functions.
// ---------------------------------------------------------------------------

int Run(EntryFn entry, int argc, char** argv) {
  Initialize();
  PowerOn(std::move(entry), argc, argv);
  int ret = WaitForPowerOff();
  Deinitialize();
  return ret;
}

int Run(EntryFn entry, int argc, char** argv, const Config& config) {
  Initialize();
  Config conf = config;
  PowerOn(std::move(entry), argc, argv, &conf);
  int ret = WaitForPowerOff();
  Deinitialize();
  return ret;
}

}  // namespace tin
