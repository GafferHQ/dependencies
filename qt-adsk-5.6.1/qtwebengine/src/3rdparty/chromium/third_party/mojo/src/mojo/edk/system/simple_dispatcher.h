// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_SIMPLE_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_SIMPLE_DISPATCHER_H_

#include <list>

#include "mojo/edk/system/awakable_list.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace system {

// A base class for simple dispatchers. "Simple" means that there's a one-to-one
// correspondence between handles and dispatchers (see the explanatory comment
// in core.cc). This class implements the standard waiter-signalling mechanism
// in that case.
class MOJO_SYSTEM_IMPL_EXPORT SimpleDispatcher : public Dispatcher {
 protected:
  SimpleDispatcher();
  ~SimpleDispatcher() override;

  // To be called by subclasses when the state changes (so
  // |GetHandleSignalsStateImplNoLock()| should be checked again). Must be
  // called under lock.
  void HandleSignalsStateChangedNoLock();

  // |Dispatcher| protected methods:
  void CancelAllAwakablesNoLock() override;
  MojoResult AddAwakableImplNoLock(Awakable* awakable,
                                   MojoHandleSignals signals,
                                   uint32_t context,
                                   HandleSignalsState* signals_state) override;
  void RemoveAwakableImplNoLock(Awakable* awakable,
                                HandleSignalsState* signals_state) override;

 private:
  // Protected by |lock()|:
  AwakableList awakable_list_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(SimpleDispatcher);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_SIMPLE_DISPATCHER_H_
