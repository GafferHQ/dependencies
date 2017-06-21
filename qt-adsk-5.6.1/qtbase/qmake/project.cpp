/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the qmake application of the Qt Toolkit.
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

#include "project.h"

#include "option.h"
#include <qmakeevaluator_p.h>

#include <qdir.h>

#include <stdio.h>

using namespace QMakeInternal;

QT_BEGIN_NAMESPACE

QMakeProject::QMakeProject()
    : QMakeEvaluator(Option::globals, Option::parser, Option::vfs, &Option::evalHandler)
{
}

QMakeProject::QMakeProject(QMakeProject *p)
    : QMakeEvaluator(Option::globals, Option::parser, Option::vfs, &Option::evalHandler)
{
    initFrom(p);
}

bool QMakeProject::boolRet(VisitReturn vr)
{
    if (vr == ReturnError)
        exit(3);
    Q_ASSERT(vr == ReturnTrue || vr == ReturnFalse);
    return vr != ReturnFalse;
}

bool QMakeProject::read(const QString &project, LoadFlags what)
{
    m_projectFile = project;
    setOutputDir(Option::output_dir);
    QString absproj = (project == QLatin1String("-"))
            ? QLatin1String("(stdin)")
            : QDir::cleanPath(QDir(qmake_getpwd()).absoluteFilePath(project));
    m_projectDir = QFileInfo(absproj).path();
    return boolRet(evaluateFile(absproj, QMakeHandler::EvalProjectFile, what));
}

static ProStringList prepareBuiltinArgs(const QList<ProStringList> &args)
{
    ProStringList ret;
    ret.reserve(args.size());
    foreach (const ProStringList &arg, args)
        ret << arg.join(' ');
    return ret;
}

bool QMakeProject::test(const ProKey &func, const QList<ProStringList> &args)
{
    m_current.clear();

    if (int func_t = statics.functions.value(func))
        return boolRet(evaluateBuiltinConditional(func_t, func, prepareBuiltinArgs(args)));

    QHash<ProKey, ProFunctionDef>::ConstIterator it =
            m_functionDefs.testFunctions.constFind(func);
    if (it != m_functionDefs.testFunctions.constEnd())
        return boolRet(evaluateBoolFunction(*it, args, func));

    evalError(QStringLiteral("'%1' is not a recognized test function.")
              .arg(func.toQString(m_tmp1)));
    return false;
}

QStringList QMakeProject::expand(const ProKey &func, const QList<ProStringList> &args)
{
    m_current.clear();

    if (int func_t = statics.expands.value(func))
        return evaluateBuiltinExpand(func_t, func, prepareBuiltinArgs(args)).toQStringList();

    QHash<ProKey, ProFunctionDef>::ConstIterator it =
            m_functionDefs.replaceFunctions.constFind(func);
    if (it != m_functionDefs.replaceFunctions.constEnd()) {
        QMakeProject::VisitReturn vr;
        ProStringList ret = evaluateFunction(*it, args, &vr);
        if (vr == QMakeProject::ReturnError)
            exit(3);
        return ret.toQStringList();
    }

    evalError(QStringLiteral("'%1' is not a recognized replace function.")
              .arg(func.toQString(m_tmp1)));
    return QStringList();
}

ProString QMakeProject::expand(const QString &expr, const QString &where, int line)
{
    ProString ret;
    ProFile *pro = m_parser->parsedProBlock(expr, where, line, QMakeParser::ValueGrammar);
    if (pro->isOk()) {
        m_current.pro = pro;
        m_current.line = 0;
        const ushort *tokPtr = pro->tokPtr();
        ProStringList result = expandVariableReferences(tokPtr, 1, true);
        if (!result.isEmpty())
            ret = result.at(0);
    }
    pro->deref();
    return ret;
}

bool QMakeProject::isEmpty(const ProKey &v) const
{
    ProValueMap::ConstIterator it = m_valuemapStack.first().constFind(v);
    return it == m_valuemapStack.first().constEnd() || it->isEmpty();
}

void QMakeProject::dump() const
{
    QStringList out;
    for (ProValueMap::ConstIterator it = m_valuemapStack.first().begin();
         it != m_valuemapStack.first().end(); ++it) {
        if (!it.key().startsWith('.')) {
            QString str = it.key() + " =";
            foreach (const ProString &v, it.value())
                str += ' ' + formatValue(v);
            out << str;
        }
    }
    out.sort();
    foreach (const QString &v, out)
        puts(qPrintable(v));
}

QT_END_NAMESPACE
