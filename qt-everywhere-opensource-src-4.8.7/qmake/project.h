/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the qmake application of the Qt Toolkit.
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

#ifndef PROJECT_H
#define PROJECT_H

#include <qstringlist.h>
#include <qtextstream.h>
#include <qstring.h>
#include <qstack.h>
#include <qmap.h>
#include <qmetatype.h>

QT_BEGIN_NAMESPACE

class QMakeProperty;

struct ParsableBlock;
struct IteratorBlock;
struct FunctionBlock;

class QMakeProject
{
    struct ScopeBlock
    {
        enum TestStatus { TestNone, TestFound, TestSeek };
        ScopeBlock() : iterate(0), ignore(false), else_status(TestNone) { }
        ScopeBlock(bool i) : iterate(0), ignore(i), else_status(TestNone) { }
        ~ScopeBlock();
        IteratorBlock *iterate;
        uint ignore : 1, else_status : 2;
    };
    friend struct ParsableBlock;
    friend struct IteratorBlock;
    friend struct FunctionBlock;

    QStack<ScopeBlock> scope_blocks;
    QStack<FunctionBlock *> function_blocks;
    IteratorBlock *iterator;
    FunctionBlock *function;
    QMap<QString, FunctionBlock*> testFunctions, replaceFunctions;

    bool recursive;
    bool own_prop;
    bool backslashWarned;
    QString pfile, cfile;
    QMakeProperty *prop;
    void reset();
    QMap<QString, QStringList> vars, base_vars, cache;
    bool parse(const QString &text, QMap<QString, QStringList> &place, int line_count=1);

    enum IncludeStatus {
        IncludeSuccess,
        IncludeFeatureAlreadyLoaded,
        IncludeFailure,
        IncludeNoExist,
        IncludeParseFailure
    };
    enum IncludeFlags {
        IncludeFlagNone = 0x00,
        IncludeFlagFeature = 0x01,
        IncludeFlagNewParser = 0x02,
        IncludeFlagNewProject = 0x04
    };
    IncludeStatus doProjectInclude(QString file, uchar flags, QMap<QString, QStringList> &place);

    bool doProjectCheckReqs(const QStringList &deps, QMap<QString, QStringList> &place);
    bool doVariableReplace(QString &str, QMap<QString, QStringList> &place);
    QStringList doVariableReplaceExpand(const QString &str, QMap<QString, QStringList> &place, bool *ok=0);
    void init(QMakeProperty *, const QMap<QString, QStringList> *);
    QStringList &values(const QString &v, QMap<QString, QStringList> &place);
    void validateModes();

public:
    QMakeProject() { init(0, 0); }
    QMakeProject(QMakeProperty *p) { init(p, 0); }
    QMakeProject(QMakeProject *p, const QMap<QString, QStringList> *nvars=0);
    QMakeProject(const QMap<QString, QStringList> &nvars) { init(0, &nvars); }
    QMakeProject(QMakeProperty *p, const QMap<QString, QStringList> &nvars) { init(p, &nvars); }
    ~QMakeProject();

    enum { ReadCache=0x01, ReadConf=0x02, ReadCmdLine=0x04, ReadProFile=0x08,
           ReadFeatures=0x20, ReadConfigs=0x40, ReadAll=0xFF };
    inline bool parse(const QString &text) { return parse(text, vars); }
    bool read(const QString &project, uchar cmd=ReadAll);
    bool read(uchar cmd=ReadAll);

    QStringList userExpandFunctions() { return replaceFunctions.keys(); }
    QStringList userTestFunctions() { return testFunctions.keys(); }

    QString projectFile();
    inline QMakeProperty *properties() { return prop; }

    bool doProjectTest(QString str, QMap<QString, QStringList> &place);
    bool doProjectTest(QString func, const QString &params,
                       QMap<QString, QStringList> &place);
    bool doProjectTest(QString func, QStringList args,
                       QMap<QString, QStringList> &place);
    bool doProjectTest(QString func, QList<QStringList> args,
                       QMap<QString, QStringList> &place);
    QStringList doProjectExpand(QString func, const QString &params,
                                QMap<QString, QStringList> &place);
    QStringList doProjectExpand(QString func, QStringList args,
                                QMap<QString, QStringList> &place);
    QStringList doProjectExpand(QString func, QList<QStringList> args,
                                QMap<QString, QStringList> &place);

    QStringList expand(const QString &v);
    QString expand(const QString &v, const QString &file, int line);
    QStringList expand(const QString &func, const QList<QStringList> &args);
    bool test(const QString &v);
    bool test(const QString &func, const QList<QStringList> &args);
    bool isActiveConfig(const QString &x, bool regex=false,
                        QMap<QString, QStringList> *place=NULL);

    bool isSet(const QString &v); // No compat mapping, no magic variables
    bool isEmpty(const QString &v); // With compat mapping, but no magic variables
    QStringList &values(const QString &v); // With compat mapping and magic variables
    QString first(const QString &v); // ditto
    QMap<QString, QStringList> &variables(); // No compat mapping and magic, obviously
    int intValue(const QString &v, int defaultValue = 0); // ditto

    bool isRecursive() const { return recursive; }

protected:
    friend class MakefileGenerator;
    bool read(const QString &file, QMap<QString, QStringList> &place);
    bool read(QTextStream &file, QMap<QString, QStringList> &place);

};
Q_DECLARE_METATYPE(QMakeProject*)

inline QString QMakeProject::projectFile()
{
    if (pfile == "-")
        return QString("(stdin)");
    return pfile;
}

inline QStringList &QMakeProject::values(const QString &v)
{ return values(v, vars); }

inline bool QMakeProject::isSet(const QString &v)
{ return vars.contains(v); }

inline QString QMakeProject::first(const QString &v)
{
    const QStringList vals = values(v);
    if(vals.isEmpty())
        return QString("");
    return vals.first();
}

/*!
 * For variables that are supposed to contain a single int,
 * this method returns the numeric value.
 * Only the first value of the variable is taken into account.
 * The string representation is assumed to look like a C int literal.
 */
inline int QMakeProject::intValue(const QString &v, int defaultValue)
{
    const QString str = first(v);
    if (!str.isEmpty()) {
        bool ok;
        int i = str.toInt(&ok, 0);
        if (ok)
            return i;
    }
    return defaultValue;
}

inline QMap<QString, QStringList> &QMakeProject::variables()
{ return vars; }

QT_END_NAMESPACE

#endif // PROJECT_H
