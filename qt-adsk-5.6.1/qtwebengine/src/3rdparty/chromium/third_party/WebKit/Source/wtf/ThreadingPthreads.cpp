/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "wtf/Threading.h"

#if USE(PTHREADS)

#include "wtf/DateMath.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/WTFThreadData.h"
#include "wtf/dtoa.h"
#include "wtf/dtoa/cached-powers.h"
#include <errno.h>

#if !COMPILER(MSVC)
#include <limits.h>
#include <sched.h>
#include <sys/time.h>
#endif

#if OS(MACOSX)
#include <objc/objc-auto.h>
#endif

#if OS(LINUX)
#include <sys/syscall.h>
#endif

#if OS(LINUX) || OS(ANDROID)
#include <unistd.h>
#endif

namespace WTF {

static Mutex* atomicallyInitializedStaticMutex;

void initializeThreading()
{
    // This should only be called once.
    ASSERT(!atomicallyInitializedStaticMutex);

    // StringImpl::empty() does not construct its static string in a threadsafe fashion,
    // so ensure it has been initialized from here.
    StringImpl::empty();
    StringImpl::empty16Bit();
    atomicallyInitializedStaticMutex = new Mutex;
    wtfThreadData();
    s_dtoaP5Mutex = new Mutex;
    initializeDates();
    // Force initialization of static DoubleToStringConverter converter variable
    // inside EcmaScriptConverter function while we are in single thread mode.
    double_conversion::DoubleToStringConverter::EcmaScriptConverter();
}

void lockAtomicallyInitializedStaticMutex()
{
    ASSERT(atomicallyInitializedStaticMutex);
    atomicallyInitializedStaticMutex->lock();
}

void unlockAtomicallyInitializedStaticMutex()
{
    atomicallyInitializedStaticMutex->unlock();
}

ThreadIdentifier currentThread()
{
#if OS(MACOSX)
    return pthread_mach_thread_np(pthread_self());
#elif OS(LINUX)
    return syscall(__NR_gettid);
#elif OS(ANDROID)
    return gettid();
#else
    return reinterpret_cast<uintptr_t>(pthread_self());
#endif
}

MutexBase::MutexBase(bool recursive)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, recursive ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL);

    int result = pthread_mutex_init(&m_mutex.m_internalMutex, &attr);
    ASSERT_UNUSED(result, !result);
#if ENABLE(ASSERT)
    m_mutex.m_recursionCount = 0;
#endif

    pthread_mutexattr_destroy(&attr);
}

MutexBase::~MutexBase()
{
    int result = pthread_mutex_destroy(&m_mutex.m_internalMutex);
    ASSERT_UNUSED(result, !result);
}

void MutexBase::lock()
{
    int result = pthread_mutex_lock(&m_mutex.m_internalMutex);
    ASSERT_UNUSED(result, !result);
#if ENABLE(ASSERT)
    ++m_mutex.m_recursionCount;
#endif
}

void MutexBase::unlock()
{
#if ENABLE(ASSERT)
    ASSERT(m_mutex.m_recursionCount);
    --m_mutex.m_recursionCount;
#endif
    int result = pthread_mutex_unlock(&m_mutex.m_internalMutex);
    ASSERT_UNUSED(result, !result);
}

// There is a separate tryLock implementation for the Mutex and the
// RecursiveMutex since on Windows we need to manually check if tryLock should
// succeed or not for the non-recursive mutex. On Linux the two implementations
// are equal except we can assert the recursion count is always zero for the
// non-recursive mutex.
bool Mutex::tryLock()
{
    int result = pthread_mutex_trylock(&m_mutex.m_internalMutex);
    if (result == 0) {
#if ENABLE(ASSERT)
        // The Mutex class is not recursive, so the recursionCount should be
        // zero after getting the lock.
        ASSERT(!m_mutex.m_recursionCount);
        ++m_mutex.m_recursionCount;
#endif
        return true;
    }
    if (result == EBUSY)
        return false;

    ASSERT_NOT_REACHED();
    return false;
}

bool RecursiveMutex::tryLock()
{
    int result = pthread_mutex_trylock(&m_mutex.m_internalMutex);
    if (result == 0) {
#if ENABLE(ASSERT)
        ++m_mutex.m_recursionCount;
#endif
        return true;
    }
    if (result == EBUSY)
        return false;

    ASSERT_NOT_REACHED();
    return false;
}

ThreadCondition::ThreadCondition()
{
    pthread_cond_init(&m_condition, NULL);
}

ThreadCondition::~ThreadCondition()
{
    pthread_cond_destroy(&m_condition);
}

void ThreadCondition::wait(MutexBase& mutex)
{
    PlatformMutex& platformMutex = mutex.impl();
    int result = pthread_cond_wait(&m_condition, &platformMutex.m_internalMutex);
    ASSERT_UNUSED(result, !result);
#if ENABLE(ASSERT)
    ++platformMutex.m_recursionCount;
#endif
}

bool ThreadCondition::timedWait(MutexBase& mutex, double absoluteTime)
{
    if (absoluteTime < currentTime())
        return false;

    if (absoluteTime > INT_MAX) {
        wait(mutex);
        return true;
    }

    int timeSeconds = static_cast<int>(absoluteTime);
    int timeNanoseconds = static_cast<int>((absoluteTime - timeSeconds) * 1E9);

    timespec targetTime;
    targetTime.tv_sec = timeSeconds;
    targetTime.tv_nsec = timeNanoseconds;

    PlatformMutex& platformMutex = mutex.impl();
    int result = pthread_cond_timedwait(&m_condition, &platformMutex.m_internalMutex, &targetTime);
#if ENABLE(ASSERT)
    ++platformMutex.m_recursionCount;
#endif
    return result == 0;
}

void ThreadCondition::signal()
{
    int result = pthread_cond_signal(&m_condition);
    ASSERT_UNUSED(result, !result);
}

void ThreadCondition::broadcast()
{
    int result = pthread_cond_broadcast(&m_condition);
    ASSERT_UNUSED(result, !result);
}

#if ENABLE(ASSERT)
static bool s_threadCreated = false;

bool isAtomicallyInitializedStaticMutexLockHeld()
{
    return atomicallyInitializedStaticMutex && atomicallyInitializedStaticMutex->locked();
}

bool isBeforeThreadCreated()
{
    return !s_threadCreated;
}

void willCreateThread()
{
    s_threadCreated = true;
}
#endif

} // namespace WTF

#endif // USE(PTHREADS)
