#ifndef CLIFF_SHIM_BUILD_BUILD_CONFIG_H_
#define CLIFF_SHIM_BUILD_BUILD_CONFIG_H_

// cliff used to ship its own build/build_config.h. The macros it provided
// (OS_*, ARCH_CPU_*, COMPILER_*, etc.) are identical to Chromium's
// build/build_config.h, so we just re-export it.
#include "build/build_config.h"

// cliff also exposed SIZE_OF_POINTER (used by tin/runtime/guintptr.h to
// align the GUintptr struct). It is just sizeof(void*) on the target.
#ifndef SIZE_OF_POINTER
#define SIZE_OF_POINTER sizeof(void*)
#endif

#endif  // CLIFF_SHIM_BUILD_BUILD_CONFIG_H_
