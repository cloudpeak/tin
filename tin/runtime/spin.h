// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_SPIN_H_
#define TIN_RUNTIME_SPIN_H_
namespace tin::runtime {
namespace spin {

const int kActiveSpin = 4;
const int kActiveSpinCount = 30;
const int kPassiveSpin = 1;

}  // namespace spin

bool CanSpin(int i);

void DoSpin();

}  // namespace tin::runtime
#endif  // TIN_RUNTIME_SPIN_H_
