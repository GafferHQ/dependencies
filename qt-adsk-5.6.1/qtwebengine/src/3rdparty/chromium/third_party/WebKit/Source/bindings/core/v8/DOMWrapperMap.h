/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOMWrapperMap_h
#define DOMWrapperMap_h

#include "bindings/core/v8/WrapperTypeInfo.h"
#include "platform/ScriptForbiddenScope.h"
#include "wtf/HashMap.h"
#include <v8-util.h>
#include <v8.h>

namespace blink {

template<class KeyType>
class DOMWrapperMap {
public:
    explicit DOMWrapperMap(v8::Isolate* isolate)
        : m_isolate(isolate)
        , m_map(isolate)
    {
    }

    v8::Local<v8::Object> newLocal(v8::Isolate* isolate, KeyType* key)
    {
        return m_map.Get(key);
    }

    bool setReturnValueFrom(v8::ReturnValue<v8::Value> returnValue, KeyType* key)
    {
        return m_map.SetReturnValue(key, returnValue);
    }

    void setReference(v8::Isolate* isolate, const v8::Persistent<v8::Object>& parent, KeyType* key)
    {
        m_map.SetReference(key, parent);
    }

    bool containsKey(KeyType* key)
    {
        return m_map.Contains(key);
    }

    bool set(KeyType* key, const WrapperTypeInfo* wrapperTypeInfo, v8::Local<v8::Object>& wrapper) WARN_UNUSED_RETURN
    {
        if (UNLIKELY(containsKey(key))) {
            wrapper = newLocal(m_isolate, key);
            return false;
        }
        v8::Global<v8::Object> global(m_isolate, wrapper);
        wrapperTypeInfo->configureWrapper(&global);
        m_map.Set(key, global.Pass());
        return true;
    }

    void clear()
    {
        m_map.Clear();
    }

    void removeAndDispose(KeyType* key)
    {
        ASSERT(containsKey(key));
        m_map.Remove(key);
    }

private:
    class PersistentValueMapTraits {
    public:
        // Map traits:
        typedef HashMap<KeyType*, v8::PersistentContainerValue> Impl;
        typedef typename Impl::iterator Iterator;
        static size_t Size(const Impl* impl) { return impl->size(); }
        static bool Empty(Impl* impl) { return impl->isEmpty(); }
        static void Swap(Impl& impl, Impl& other) { impl.swap(other); }
        static Iterator Begin(Impl* impl) { return impl->begin(); }
        static Iterator End(Impl* impl) { return impl->end(); }
        static v8::PersistentContainerValue Value(Iterator& iter)
        {
            return iter->value;
        }
        static KeyType* Key(Iterator& iter) { return iter->key; }
        static v8::PersistentContainerValue Set(
            Impl* impl, KeyType* key, v8::PersistentContainerValue value)
        {
            v8::PersistentContainerValue oldValue = Get(impl, key);
            impl->set(key, value);
            return oldValue;
        }
        static v8::PersistentContainerValue Get(const Impl* impl, KeyType* key)
        {
            return impl->get(key);
        }

        static v8::PersistentContainerValue Remove(Impl* impl, KeyType* key)
        {
            return impl->take(key);
        }

        // Weak traits:
        static const v8::PersistentContainerCallbackType kCallbackType = v8::kWeakWithInternalFields;
        typedef v8::GlobalValueMap<KeyType*, v8::Object, PersistentValueMapTraits> MapType;
        typedef MapType WeakCallbackDataType;

        static WeakCallbackDataType* WeakCallbackParameter(MapType* map, KeyType* key, v8::Local<v8::Object>& value)
        {
            return map;
        }

        static void DisposeCallbackData(WeakCallbackDataType* callbackData) { }

        static MapType* MapFromWeakCallbackInfo(const v8::WeakCallbackInfo<WeakCallbackDataType>& data)
        {
            return data.GetParameter();
        }

        static KeyType* KeyFromWeakCallbackInfo(const v8::WeakCallbackInfo<WeakCallbackDataType>& data)
        {
            return reinterpret_cast<KeyType*>(data.GetInternalField(v8DOMWrapperObjectIndex));
        }

        static void OnWeakCallback(const v8::WeakCallbackInfo<WeakCallbackDataType>& data) { }

        static void Dispose(v8::Isolate* isolate, v8::Global<v8::Object> value, KeyType* key) { }

        static void DisposeWeak(const v8::WeakCallbackInfo<WeakCallbackDataType>& data)
        {
            void* internalFields[v8::kInternalFieldsInWeakCallback] = {
                data.GetInternalField(0),
                data.GetInternalField(1)
            };
            DisposeWeak(data.GetIsolate(), internalFields, KeyFromWeakCallbackInfo(data));
        }
        static void DisposeWeak(v8::Isolate* isolate, void* internalFields[v8::kInternalFieldsInWeakCallback], KeyType* key) { }
    };

    v8::Isolate* m_isolate;
    typename PersistentValueMapTraits::MapType m_map;
};

} // namespace blink

#endif // DOMWrapperMap_h
