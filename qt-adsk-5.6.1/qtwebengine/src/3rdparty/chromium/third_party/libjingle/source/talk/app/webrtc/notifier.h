/*
 * libjingle
 * Copyright 2011 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_APP_WEBRTC_NOTIFIER_H_
#define TALK_APP_WEBRTC_NOTIFIER_H_

#include <list>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "webrtc/base/common.h"

namespace webrtc {

// Implement a template version of a notifier.
template <class T>
class Notifier : public T {
 public:
  Notifier() {
  }

  virtual void RegisterObserver(ObserverInterface* observer) {
    ASSERT(observer != NULL);
    observers_.push_back(observer);
  }

  virtual void UnregisterObserver(ObserverInterface* observer) {
    for (std::list<ObserverInterface*>::iterator it = observers_.begin();
         it != observers_.end(); it++) {
      if (*it == observer) {
        observers_.erase(it);
        break;
      }
    }
  }

  void FireOnChanged() {
    // Copy the list of observers to avoid a crash if the observer object
    // unregisters as a result of the OnChanged() call. If the same list is used
    // UnregisterObserver will affect the list make the iterator invalid.
    std::list<ObserverInterface*> observers = observers_;
    for (std::list<ObserverInterface*>::iterator it = observers.begin();
         it != observers.end(); ++it) {
      (*it)->OnChanged();
    }
  }

 protected:
  std::list<ObserverInterface*> observers_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_NOTIFIER_H_
