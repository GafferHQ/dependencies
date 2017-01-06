/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#ifndef PROFILEEVALUATOR_H
#define PROFILEEVALUATOR_H

#include "proparser_global.h"
#include "proitems.h"

#include <QtCore/QHash>
#include <QtCore/QStringList>
#ifndef QT_BOOTSTRAPPED
# include <QtCore/QProcess>
#endif
#ifdef PROEVALUATOR_THREAD_SAFE
# include <QtCore/QMutex>
# include <QtCore/QWaitCondition>
#endif

QT_BEGIN_NAMESPACE

struct ProFileOption;
class ProFileParser;

class PROPARSER_EXPORT ProFileEvaluatorHandler
{
public:
    // qmake/project configuration error
    virtual void configError(const QString &msg) = 0;
    // Some error during evaluation
    virtual void evalError(const QString &filename, int lineNo, const QString &msg) = 0;
    // error() and message() from .pro file
    virtual void fileMessage(const QString &msg) = 0;

    enum EvalFileType { EvalProjectFile, EvalIncludeFile, EvalConfigFile, EvalFeatureFile, EvalAuxFile };
    virtual void aboutToEval(ProFile *parent, ProFile *proFile, EvalFileType type) = 0;
    virtual void doneWithEval(ProFile *parent) = 0;
};


class PROPARSER_EXPORT ProFileEvaluator
{
    class Private;

public:
    class FunctionDef {
    public:
        FunctionDef(ProFile *pro, int offset) : m_pro(pro), m_offset(offset) { m_pro->ref(); }
        FunctionDef(const FunctionDef &o) : m_pro(o.m_pro), m_offset(o.m_offset) { m_pro->ref(); }
        ~FunctionDef() { m_pro->deref(); }
        FunctionDef &operator=(const FunctionDef &o)
        {
            if (this != &o) {
                m_pro->deref();
                m_pro = o.m_pro;
                m_pro->ref();
                m_offset = o.m_offset;
            }
            return *this;
        }
        ProFile *pro() const { return m_pro; }
        const ushort *tokPtr() const { return m_pro->tokPtr() + m_offset; }
    private:
        ProFile *m_pro;
        int m_offset;
    };

    struct FunctionDefs {
        QHash<ProString, FunctionDef> testFunctions;
        QHash<ProString, FunctionDef> replaceFunctions;
    };

    enum TemplateType {
        TT_Unknown = 0,
        TT_Application,
        TT_Library,
        TT_Script,
        TT_Aux,
        TT_Subdirs
    };

    // Call this from a concurrency-free context
    static void initialize();

    ProFileEvaluator(ProFileOption *option, ProFileParser *parser, ProFileEvaluatorHandler *handler);
    ~ProFileEvaluator();

    ProFileEvaluator::TemplateType templateType() const;
#ifdef PROEVALUATOR_CUMULATIVE
    void setCumulative(bool on); // Default is true!
#endif
    void setOutputDir(const QString &dir); // Default is empty

    enum LoadFlag {
        LoadProOnly = 0,
        LoadPreFiles = 1,
        LoadPostFiles = 2,
        LoadAll = LoadPreFiles|LoadPostFiles
    };
    Q_DECLARE_FLAGS(LoadFlags, LoadFlag)
    bool accept(ProFile *pro, LoadFlags flags = LoadAll);

    bool contains(const QString &variableName) const;
    QString value(const QString &variableName) const;
    QStringList values(const QString &variableName) const;
    QStringList values(const QString &variableName, const ProFile *pro) const;
    QStringList absolutePathValues(const QString &variable, const QString &baseDirectory) const;
    QStringList absoluteFileValues(
            const QString &variable, const QString &baseDirectory, const QStringList &searchDirs,
            const ProFile *pro) const;
    QString propertyValue(const QString &val) const;

private:
    Private *d;

    friend struct ProFileOption;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ProFileEvaluator::LoadFlags)

// This struct is from qmake, but we are not using everything.
struct PROPARSER_EXPORT ProFileOption
{
    ProFileOption();
    ~ProFileOption();

    //simply global convenience
    //QString libtool_ext;
    //QString pkgcfg_ext;
    //QString prf_ext;
    //QString prl_ext;
    //QString ui_ext;
    //QStringList h_ext;
    //QStringList cpp_ext;
    //QString h_moc_ext;
    //QString cpp_moc_ext;
    //QString obj_ext;
    //QString lex_ext;
    //QString yacc_ext;
    //QString h_moc_mod;
    //QString cpp_moc_mod;
    //QString lex_mod;
    //QString yacc_mod;
    QString dir_sep;
    QString dirlist_sep;
    QString qmakespec;
    QString cachefile;
    QHash<QString, QString> properties;
#ifndef QT_BOOTSTRAPPED
    QProcessEnvironment environment;
#endif
    QString sysroot;

    //QString pro_ext;
    //QString res_ext;

    // -nocache, -cache, -spec, QMAKESPEC
    // -set persistent value
    void setCommandLineArguments(const QStringList &args);
#ifdef PROEVALUATOR_INIT_PROPS
    bool initProperties(const QString &qmake);
#endif

  private:
    friend class ProFileEvaluator;
    friend class ProFileEvaluator::Private;

    void applyHostMode();
    QString getEnv(const QString &) const;

    QHash<ProString, ProStringList> base_valuemap; // Cached results of qmake.conf, .qmake.cache & default_pre.prf
    ProFileEvaluator::FunctionDefs base_functions;
    QStringList feature_roots;
    QString qmakespec_name;
    QString precmds, postcmds;
    enum HOST_MODE { HOST_UNKNOWN_MODE, HOST_UNIX_MODE, HOST_WIN_MODE, HOST_MACX_MODE };
    HOST_MODE host_mode;
    enum TARG_MODE { TARG_UNKNOWN_MODE, TARG_UNIX_MODE, TARG_WIN_MODE, TARG_MACX_MODE,
                     TARG_SYMBIAN_MODE };
    TARG_MODE target_mode;
#ifdef PROEVALUATOR_THREAD_SAFE
    QMutex mutex;
    QWaitCondition cond;
    bool base_inProgress;
#endif
};

Q_DECLARE_TYPEINFO(ProFileEvaluator::FunctionDef, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif // PROFILEEVALUATOR_H
