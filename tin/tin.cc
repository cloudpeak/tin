// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"

#include "tin/runtime/env.h"
#include "tin/platform/platform.h"

#include "tin/tin.h"

namespace tin {

namespace {
Config* conf = NULL;
base::AtExitManager* atexit = NULL;
}

void Initialize() {
  atexit = new base::AtExitManager;
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
  delete atexit;
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

}  // namespace tin
