/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InspectorDatabaseAgent_h
#define InspectorDatabaseAgent_h

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Database;
class InspectorDatabaseResource;
class InspectorFrontend;
class Page;

typedef String ErrorString;

class MODULES_EXPORT InspectorDatabaseAgent final : public InspectorBaseAgent<InspectorDatabaseAgent, InspectorFrontend::Database>, public InspectorBackendDispatcher::DatabaseCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDatabaseAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorDatabaseAgent> create(Page* page)
    {
        return adoptPtrWillBeNoop(new InspectorDatabaseAgent(page));
    }
    ~InspectorDatabaseAgent() override;
    DECLARE_VIRTUAL_TRACE();

    void disable(ErrorString*) override;
    void restore() override;
    void didCommitLoadForLocalFrame(LocalFrame*) override;

    // Called from the front-end.
    void enable(ErrorString*) override;
    void getDatabaseTableNames(ErrorString*, const String& databaseId, RefPtr<TypeBuilder::Array<String>>& names) override;
    void executeSQL(ErrorString*, const String& databaseId, const String& query, PassRefPtrWillBeRawPtr<ExecuteSQLCallback>) override;

    void didOpenDatabase(Database*, const String& domain, const String& name, const String& version);
private:
    explicit InspectorDatabaseAgent(Page*);

    Database* databaseForId(const String& databaseId);
    InspectorDatabaseResource* findByFileName(const String& fileName);

    Page* m_page;
    typedef PersistentHeapHashMapWillBeHeapHashMap<String, Member<InspectorDatabaseResource>> DatabaseResourcesHeapMap;
    DatabaseResourcesHeapMap m_resources;
    bool m_enabled;
};

} // namespace blink

#endif // !defined(InspectorDatabaseAgent_h)
