// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tin/error/error.h"

int TinTranslateSysError(int sys_errno) {
  if (sys_errno <= 0) {
    return sys_errno;  /* If < 0 then it's already a libuv error. */
  }
  return -sys_errno;
}

int TinGetaddrinfoTranslateError(int sys_errno) {
  if (sys_errno <= 0) {
    return sys_errno;  /* If < 0 then it's already a libuv error. */
  }
  return -sys_errno;
}

