// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <functional>

#include "context/zcontext.h"
#include "tin/runtime/m.h"
#include "tin/runtime/p.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/timer/timer_queue.h"

#include "tin/runtime/greenlet.h"

namespace tin {
namespace runtime {

Greenlet::Greenlet()
  : lockedm_(NULL)
  , error_code_(0)
  , timer_(NULL) {
}

Greenlet::~Greenlet() {
  delete timer_;
}

void Greenlet::SetName(const char* name) {
  // TODO
/*  if (name != NULL)
    base::strlcpy(name_, name, arraysize(name_));
  else
    base::strlcpy(name_, "greenlet", arraysize(name_));*/
}

Timer* Greenlet::GetTimer() {
  if (timer_ == NULL) {
    timer_ = new Timer;
  }
  return timer_;
}




Greenlet* Greenlet::Create(GreenletFunc entry,
                           std::function<void()>*  closure,
                           bool sysg0 /*= false*/,
                           intptr_t args /*= 0*/,
                           bool joinable /*= false*/,
                           int stack_size /*= kDefaultStackSize*/,
                           const char* name /*= "greenlet"*/) {
  // joinable, not implement
  if (stack_size == 0)
    stack_size = kDefaultStackSize;
  scoped_ptr<Greenlet> glet(new Greenlet);
  {
    glet->flags_ = 0;
    glet->args_ = 0;
  }
  glet->state_ = GLET_RUNNABLE;
  glet->args_ = args;
  glet->entry_ = entry;
  if (closure != NULL) {
    std::swap(glet->closure_, *closure);
  }
  glet->SetName(name);
  if (rtm_conf->IsStackProtectionEnabled()) {
    glet->stack_.reset(NewStack(kProtectedFixedStack, stack_size));
  } else {
    glet->stack_.reset(NewStack(kFixedStack, stack_size));
  }
  // make_zcontext round address internally.
  glet->context_ =
    make_zcontext(glet->stack_->Pointer(), stack_size, StaticProc);
  if (!sysg0) {
    GetP()->RunqPut(glet.get(), true);
    sched->WakePIfNecessary();
  } else {
    glet->SetG0Flag();
    glet_tls->Set(glet.get());
  }
  return glet.release();
}

void Greenlet::StaticProc(intptr_t args) {
  Greenlet* glet = reinterpret_cast<Greenlet*>(args);
  glet->Proc();
}

void Greenlet::Proc() {
  if (closure_) {
    closure_();
  } else {
    retval_ = entry_(args_);
  }

  // glet exit.
  // add to m local dead queue.
  M()->AddToDeadQueue(this);
  Park();
  // never return.
}

void SpawnSimple(GreenletFunc entry, void* args,  const char* name) {
  Greenlet::Create(entry,
                   NULL,
                   false,
                   reinterpret_cast<intptr_t>(args),
                   false,
                   0,
                   name);
}

void SpawnSimple(std::function<void()> closure,  const char* name) {
  Greenlet::Create(NULL,
                   &closure,
                   false,
                   0,
                   false,
                   0,
                   name);
}


}  // namespace runtime

void RuntimeSpawn(std::function<void()>* closure) {
  runtime::Greenlet::Create(NULL,
                            closure,
                            false,
                            0,
                            false,
                            0);
}

}  // namespace tin
