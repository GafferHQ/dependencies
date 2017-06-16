/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qpatternistlocale_p.h"

#include "qoutputvalidator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

OutputValidator::OutputValidator(QAbstractXmlReceiver *const receiver,
                                 const DynamicContext::Ptr &context,
                                 const SourceLocationReflection *const r,
                                 const bool isXSLT) : DelegatingSourceLocationReflection(r)
                                                    , m_hasReceivedChildren(false)
                                                    , m_receiver(receiver)
                                                    , m_context(context)
                                                    , m_isXSLT(isXSLT)
{
    Q_ASSERT(receiver);
    Q_ASSERT(context);
}

void OutputValidator::namespaceBinding(const QXmlName &nb)
{
    m_receiver->namespaceBinding(nb);
}

void OutputValidator::startElement(const QXmlName &name)
{
    m_hasReceivedChildren = false;
    m_receiver->startElement(name);
    m_attributes.clear();
}

void OutputValidator::endElement()
{
    m_hasReceivedChildren = true;
    m_receiver->endElement();
}

void OutputValidator::attribute(const QXmlName &name,
                                const QStringRef &value)
{
    if(m_hasReceivedChildren)
    {
        m_context->error(QtXmlPatterns::tr("It's not possible to add attributes after any other kind of node."),
                         m_isXSLT ? ReportContext::XTDE0410 : ReportContext::XQTY0024, this);
    }
    else
    {
        if(!m_isXSLT && m_attributes.contains(name))
        {
            m_context->error(QtXmlPatterns::tr("An attribute by name %1 has already been created.").arg(formatKeyword(m_context->namePool(), name)),
                                ReportContext::XQDY0025, this);
        }
        else
        {
            m_attributes.insert(name);
            m_receiver->attribute(name, value);
        }
    }
}

void OutputValidator::comment(const QString &value)
{
    m_hasReceivedChildren = true;
    m_receiver->comment(value);
}

void OutputValidator::characters(const QStringRef &value)
{
    m_hasReceivedChildren = true;
    m_receiver->characters(value);
}

void OutputValidator::processingInstruction(const QXmlName &name,
                                            const QString &value)
{
    m_hasReceivedChildren = true;
    m_receiver->processingInstruction(name, value);
}

void OutputValidator::item(const Item &outputItem)
{
    /* We can't send outputItem directly to m_receiver since its item() function
     * won't dispatch to this OutputValidator, but to itself. We're not sub-classing here,
     * we're delegating. */

    if(outputItem.isNode())
        sendAsNode(outputItem);
    else
    {
        m_hasReceivedChildren = true;
        m_receiver->item(outputItem);
    }
}

void OutputValidator::startDocument()
{
    m_receiver->startDocument();
}

void OutputValidator::endDocument()
{
    m_receiver->endDocument();
}

void OutputValidator::atomicValue(const QVariant &value)
{
    Q_UNUSED(value);
    // TODO
}

void OutputValidator::endOfSequence()
{
}

void OutputValidator::startOfSequence()
{
}

QT_END_NAMESPACE
