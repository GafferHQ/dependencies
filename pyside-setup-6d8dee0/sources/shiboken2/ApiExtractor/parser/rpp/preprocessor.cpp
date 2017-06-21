/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2005 Harald Fernengel <harry@kdevelop.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of PySide2.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "preprocessor.h"

#include <string>
#include <cstdio>

// register callback for include hooks
static void includeFileHook(const std::string &, const std::string &, FILE *);

#define PP_HOOK_ON_FILE_INCLUDED(A, B, C) includeFileHook(A, B, C)
#include "pp.h"

using namespace rpp;

#include <QtCore/QtCore>

class PreprocessorPrivate
{
public:
    QByteArray result;
    pp_environment env;
    QStringList includePaths;

    void initPP(pp &proc) {
        foreach(QString path, includePaths)
        proc.push_include_path(path.toStdString());
    }
};

QHash<QString, QStringList> includedFiles;

void includeFileHook(const std::string &fileName, const std::string &filePath, FILE *)
{
    includedFiles[QString::fromStdString(fileName)].append(QString::fromStdString(filePath));
}

Preprocessor::Preprocessor()
{
    d = new PreprocessorPrivate;
    includedFiles.clear();
}

Preprocessor::~Preprocessor()
{
    delete d;
}

void Preprocessor::processFile(const QString &fileName)
{
    pp proc(d->env);
    d->initPP(proc);

    d->result.reserve(d->result.size() + 20 * 1024);

    d->result += "# 1 \"" + fileName.toLatin1() + "\"\n"; // ### REMOVE ME
    proc.file(fileName.toLocal8Bit().constData(), std::back_inserter(d->result));
}

void Preprocessor::processString(const QByteArray &str)
{
    pp proc(d->env);
    d->initPP(proc);

    proc(str.begin(), str.end(), std::back_inserter(d->result));
}

QByteArray Preprocessor::result() const
{
    return d->result;
}

void Preprocessor::addIncludePaths(const QStringList &includePaths)
{
    d->includePaths += includePaths;
}

QStringList Preprocessor::macroNames() const
{
    QStringList macros;

    pp_environment::const_iterator it = d->env.first_macro();
    while (it != d->env.last_macro()) {
        const pp_macro *m = *it;
        macros += QString::fromLatin1(m->name->begin(), m->name->size());
        ++it;
    }

    return macros;
}

QList<Preprocessor::MacroItem> Preprocessor::macros() const
{
    QList<MacroItem> items;

    pp_environment::const_iterator it = d->env.first_macro();
    while (it != d->env.last_macro()) {
        const pp_macro *m = *it;
        MacroItem item;
        item.name = QString::fromLatin1(m->name->begin(), m->name->size());
        item.definition = QString::fromLatin1(m->definition->begin(),
                                              m->definition->size());
        for (size_t i = 0; i < m->formals.size(); ++i) {
            item.parameters += QString::fromLatin1(m->formals[i]->begin(),
                                                   m->formals[i]->size());
        }
        item.isFunctionLike = m->function_like;

#ifdef PP_WITH_MACRO_POSITION
        item.fileName = QString::fromLatin1(m->file->begin(), m->file->size());
#endif
        items += item;

        ++it;
    }

    return items;
}

/*
int main()
{
    Preprocessor pp;

    QStringList paths;
    paths << "/usr/include";
    pp.addIncludePaths(paths);

    pp.processFile("pp-configuration");
    pp.processFile("/usr/include/stdio.h");

    qDebug() << pp.result();

    return 0;
}
*/

