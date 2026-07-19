// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tin/runtime/env.h"
#include "tin/platform/platform.h"

#include "tin/tin.h"

namespace tin {

namespace {
Config* conf = NULL;
}

void Initialize() {
  conf = new tin::Config;
  *conf = DefaultConfig();
  PlatformInit();
}

void PowerOn(EntryFn fn, int argc, char** argv, Config* new_conf) {
  if (new_conf != NULL) {
    *conf = *new_conf;
  }
  runtime::InitializeEnv(fn, argc, argv, conf);
}

void PowerOn(EntryFn fn, Config* new_conf) {
  return PowerOn(fn, 0, NULL, new_conf);
}

int WaitForPowerOff() {
  return runtime::rtm_env->WaitMainExit();
}

void Deinitialize() {
  delete conf;
}

Config* GetWorkingConfig() {
  return conf;
}

Config DefaultConfig() {
  Config conf;
  conf.SetMaxProcs(1);
  conf.SetStackSize(kDefaultStackSize);
  conf.SetOsThreadStackSize(kDefaultOSThreadStackSize);
  conf.SetIgnoreSigpipe(true);
  conf.EnableStackPprotection(false);
  return conf;
}

// ---------------------------------------------------------------------------
// Runtime class implementation.
// ---------------------------------------------------------------------------

Runtime::Runtime()
  : conf_(DefaultConfig()) {
  Initialize();
}

Runtime::Runtime(const Config& conf)
  : conf_(conf) {
  Initialize();
}

Runtime::~Runtime() {
  Deinitialize();
}

int Runtime::Run(EntryFn entry, int argc, char** argv) {
  PowerOn(std::move(entry), argc, argv, &conf_);
  return WaitForPowerOff();
}

// ---------------------------------------------------------------------------
// Convenience functions.
// ---------------------------------------------------------------------------

int Run(EntryFn entry, int argc, char** argv) {
  Runtime rt;
  return rt.Run(std::move(entry), argc, argv);
}

int Run(EntryFn entry, int argc, char** argv, const Config& conf) {
  Runtime rt(conf);
  return rt.Run(std::move(entry), argc, argv);
}

}  // namespace tin
