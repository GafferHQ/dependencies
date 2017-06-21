//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// debug.h: Debugging utilities.

#ifndef COMMON_DEBUG_H_
#define COMMON_DEBUG_H_

#include <assert.h>
#include <stdio.h>
#include <string>

#include "common/angleutils.h"

#if !defined(TRACE_OUTPUT_FILE)
#define TRACE_OUTPUT_FILE "debug.txt"
#endif

namespace gl
{

enum MessageType
{
    MESSAGE_TRACE,
    MESSAGE_FIXME,
    MESSAGE_ERR,
    MESSAGE_EVENT,
};

// Outputs text to the debugging log, or the debugging window
void trace(bool traceInDebugOnly, MessageType messageType, const char *format, ...);

// Pairs a D3D begin event with an end event.
class ScopedPerfEventHelper : angle::NonCopyable
{
  public:
    ScopedPerfEventHelper(const char* format, ...);
    ~ScopedPerfEventHelper();
};

// Wraps the D3D9/D3D11 debug annotation functions.
class DebugAnnotator : angle::NonCopyable
{
  public:
    DebugAnnotator() { };
    virtual ~DebugAnnotator() { };
    virtual void beginEvent(const std::wstring &eventName) = 0;
    virtual void endEvent() = 0;
    virtual void setMarker(const std::wstring &markerName) = 0;
    virtual bool getStatus() = 0;
};

void InitializeDebugAnnotations(DebugAnnotator *debugAnnotator);
void UninitializeDebugAnnotations();
bool DebugAnnotationsActive();

}

#if defined(ANGLE_ENABLE_DEBUG_TRACE) || defined(ANGLE_ENABLE_DEBUG_ANNOTATIONS)
#define ANGLE_TRACE_ENABLED
#endif

// A macro to output a trace of a function call and its arguments to the debugging log
#if defined(ANGLE_TRACE_ENABLED)
#define TRACE(message, ...) gl::trace(true, gl::MESSAGE_TRACE, "trace: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define TRACE(message, ...) (void(0))
#endif

// A macro to output a function call and its arguments to the debugging log, to denote an item in need of fixing.
#if defined(ANGLE_TRACE_ENABLED)
#define FIXME(message, ...) gl::trace(false, gl::MESSAGE_FIXME, "fixme: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define FIXME(message, ...) (void(0))
#endif

// A macro to output a function call and its arguments to the debugging log, in case of error.
#if defined(ANGLE_TRACE_ENABLED)
#define ERR(message, ...) gl::trace(false, gl::MESSAGE_ERR, "err: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define ERR(message, ...) (void(0))
#endif

// A macro to log a performance event around a scope.
#if defined(ANGLE_TRACE_ENABLED)
#if defined(_MSC_VER)
#define EVENT(message, ...) gl::ScopedPerfEventHelper scopedPerfEventHelper ## __LINE__("%s" message "\n", __FUNCTION__, __VA_ARGS__);
#else
#define EVENT(message, ...) gl::ScopedPerfEventHelper scopedPerfEventHelper(message "\n", ##__VA_ARGS__);
#endif // _MSC_VER
#else
#define EVENT(message, ...) (void(0))
#endif

#if defined(ANGLE_TRACE_ENABLED)
#undef ANGLE_TRACE_ENABLED
#endif

// A macro asserting a condition and outputting failures to the debug log
#if !defined(NDEBUG)
#define ASSERT(expression) do { \
    if(!(expression)) \
        ERR("\t! Assert failed in %s(%d): "#expression"\n", __FUNCTION__, __LINE__); \
        assert(expression); \
    } while(0)
#define UNUSED_ASSERTION_VARIABLE(variable)
#else
#define ASSERT(expression) (void(0))
#define UNUSED_ASSERTION_VARIABLE(variable) ((void)variable)
#endif

#ifndef ANGLE_ENABLE_DEBUG_TRACE
#define UNUSED_TRACE_VARIABLE(variable) ((void)variable)
#else
#define UNUSED_TRACE_VARIABLE(variable)
#endif

// A macro to indicate unimplemented functionality

#if defined (ANGLE_TEST_CONFIG)
#define NOASSERT_UNIMPLEMENTED 1
#endif

// Define NOASSERT_UNIMPLEMENTED to non zero to skip the assert fail in the unimplemented checks
// This will allow us to test with some automated test suites (eg dEQP) without crashing
#ifndef NOASSERT_UNIMPLEMENTED
#define NOASSERT_UNIMPLEMENTED 0
#endif

#if !defined(NDEBUG)
#define UNIMPLEMENTED() do { \
    FIXME("\t! Unimplemented: %s(%d)\n", __FUNCTION__, __LINE__); \
    assert(NOASSERT_UNIMPLEMENTED); \
    } while(0)
#else
    #define UNIMPLEMENTED() FIXME("\t! Unimplemented: %s(%d)\n", __FUNCTION__, __LINE__)
#endif

// A macro for code which is not expected to be reached under valid assumptions
#if !defined(NDEBUG)
#define UNREACHABLE() do { \
    ERR("\t! Unreachable reached: %s(%d)\n", __FUNCTION__, __LINE__); \
    assert(false); \
    } while(0)
#else
    #define UNREACHABLE() ERR("\t! Unreachable reached: %s(%d)\n", __FUNCTION__, __LINE__)
#endif

// A macro that determines whether an object has a given runtime type.
#if !defined(NDEBUG) && (!defined(_MSC_VER) || defined(_CPPRTTI)) && (!defined(__GNUC__) || __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 3) || defined(__GXX_RTTI))
#define HAS_DYNAMIC_TYPE(type, obj) (dynamic_cast<type >(obj) != NULL)
#else
#define HAS_DYNAMIC_TYPE(type, obj) true
#endif

#endif   // COMMON_DEBUG_H_
