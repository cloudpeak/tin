// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "tin/config/config.h"

namespace tin {

typedef int(*EntryFn)(int argc, char** argv);

void Initialize();

void PowerOn(EntryFn fn, int argc, char** argv, Config* new_conf);

void PowerOn(EntryFn fn, Config* new_conf);

int WaitForPowerOff();

void Deinitialize();

Config* GetWorkingConfig();

Config DefaultConfig();

}  // namespace tin
