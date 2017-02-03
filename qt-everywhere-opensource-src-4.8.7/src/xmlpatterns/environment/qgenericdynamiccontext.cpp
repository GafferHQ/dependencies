/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qcommonvalues_p.h"
#include "qfocus_p.h"
#include "qtemplatemode_p.h"

#include "qgenericdynamiccontext_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

GenericDynamicContext::GenericDynamicContext(const NamePool::Ptr &np,
                                             QAbstractMessageHandler *const errHandler,
                                             const LocationHash &locations) : m_messageHandler(errHandler)
                                                                            , m_currentDateTime(QDateTime::currentDateTime().toTimeSpec(Qt::UTC))
                                                                            , m_outputReceiver(0)
                                                                            , m_namePool(np)
                                                                            , m_locations(locations)
                                                                            , m_uriResolver(0)
{
    Q_ASSERT(m_messageHandler);
    Q_ASSERT(m_namePool);
}

QExplicitlySharedDataPointer<DayTimeDuration> GenericDynamicContext::implicitTimezone() const
{
    /* Or what do you prefer, sir? */
    return CommonValues::DayTimeDurationZero;
}

QAbstractMessageHandler * GenericDynamicContext::messageHandler() const
{
    return m_messageHandler;
}

QDateTime GenericDynamicContext::currentDateTime() const
{
    return m_currentDateTime;
}

xsInteger GenericDynamicContext::contextPosition() const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "That this function is called makes no sense. A Focus should be used.");
    return 0;
}

Item GenericDynamicContext::contextItem() const
{
    return Item();
}

xsInteger GenericDynamicContext::contextSize()
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "That this function is called makes no sense. A Focus should be used.");
    return 0;
}

void GenericDynamicContext::setFocusIterator(const Item::Iterator::Ptr &)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "That this function is called makes no sense. A Focus should be used.");
}

Item::Iterator::Ptr GenericDynamicContext::focusIterator() const
{
    return Item::Iterator::Ptr();
}

QAbstractXmlReceiver *GenericDynamicContext::outputReceiver() const
{
    return m_outputReceiver;
}

void GenericDynamicContext::setOutputReceiver(QAbstractXmlReceiver *const receiver)
{
    m_outputReceiver = receiver;
}

void GenericDynamicContext::setNodeBuilder(NodeBuilder::Ptr &builder)
{
    m_nodeBuilder = builder;
}

NodeBuilder::Ptr GenericDynamicContext::nodeBuilder(const QUrl &baseURI) const
{
    return m_nodeBuilder->create(baseURI);
}

ResourceLoader::Ptr GenericDynamicContext::resourceLoader() const
{
    return m_resourceLoader;
}

void GenericDynamicContext::setResourceLoader(const ResourceLoader::Ptr &loader)
{
    m_resourceLoader = loader;
}

ExternalVariableLoader::Ptr GenericDynamicContext::externalVariableLoader() const
{
    return m_externalVariableLoader;
}

void GenericDynamicContext::setExternalVariableLoader(const ExternalVariableLoader::Ptr &loader)
{
    m_externalVariableLoader = loader;
}

NamePool::Ptr GenericDynamicContext::namePool() const
{
    return m_namePool;
}

QSourceLocation GenericDynamicContext::locationFor(const SourceLocationReflection *const reflection) const
{

    return m_locations.value(reflection->actualReflection());
}

void GenericDynamicContext::addNodeModel(const QAbstractXmlNodeModel::Ptr &nm)
{
    m_nodeModels.append(nm);
}

const QAbstractUriResolver *GenericDynamicContext::uriResolver() const
{
    return m_uriResolver;
}

ItemCacheCell &GenericDynamicContext::globalItemCacheCell(const VariableSlotID slot)
{
    if(slot >= m_globalItemCacheCells.size())
        m_globalItemCacheCells.resize(qMax(slot + 1, m_globalItemCacheCells.size()));

    return m_globalItemCacheCells[slot];
}

ItemSequenceCacheCell::Vector &GenericDynamicContext::globalItemSequenceCacheCells(const VariableSlotID slot)
{
    if(slot >= m_globalItemSequenceCacheCells.size())
        m_globalItemSequenceCacheCells.resize(qMax(slot + 1, m_globalItemSequenceCacheCells.size()));

    return m_globalItemSequenceCacheCells;
}

void GenericDynamicContext::setUriResolver(const QAbstractUriResolver *const resolver)
{
    m_uriResolver = resolver;
}

Item GenericDynamicContext::currentItem() const
{
    return Item();
}

DynamicContext::Ptr GenericDynamicContext::previousContext() const
{
    return DynamicContext::Ptr();
}

QExplicitlySharedDataPointer<TemplateMode> GenericDynamicContext::currentTemplateMode() const
{
    return QExplicitlySharedDataPointer<TemplateMode>();
}

QT_END_NAMESPACE
