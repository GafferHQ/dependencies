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

#include "translator.h"

#include <profileparser.h>
#include <profileevaluator.h>

#ifndef QT_BOOTSTRAPPED
#include <QtCore/QCoreApplication>
#include <QtCore/QTranslator>
#endif
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegExp>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>

QT_USE_NAMESPACE

#ifdef QT_BOOTSTRAPPED
static QString binDir;

static void initBinaryDir(
#ifndef Q_OS_WIN
        const char *argv0
#endif
        );

struct LR {
    static inline QString tr(const char *sourceText, const char *comment = 0)
    {
        return QCoreApplication::translate("LRelease", sourceText, comment);
    }
};
#else
class LR {
    Q_DECLARE_TR_FUNCTIONS(LRelease)
};
#endif

static void printOut(const QString & out)
{
    QTextStream stream(stdout);
    stream << out;
}

static void printErr(const QString & out)
{
    QTextStream stream(stderr);
    stream << out;
}

static void printUsage()
{
    printOut(LR::tr(
        "Usage:\n"
        "    lrelease [options] project-file\n"
        "    lrelease [options] ts-files [-qm qm-file]\n\n"
        "lrelease is part of Qt's Linguist tool chain. It can be used as a\n"
        "stand-alone tool to convert XML-based translations files in the TS\n"
        "format into the 'compiled' QM format used by QTranslator objects.\n\n"
        "Options:\n"
        "    -help  Display this information and exit\n"
        "    -idbased\n"
        "           Use IDs instead of source strings for message keying\n"
        "    -compress\n"
        "           Compress the QM files\n"
        "    -nounfinished\n"
        "           Do not include unfinished translations\n"
        "    -removeidentical\n"
        "           If the translated text is the same as\n"
        "           the source text, do not include the message\n"
        "    -markuntranslated <prefix>\n"
        "           If a message has no real translation, use the source text\n"
        "           prefixed with the given string instead\n"
        "    -silent\n"
        "           Do not explain what is being done\n"
        "    -version\n"
        "           Display the version of lrelease and exit\n"
    ));
}

static bool loadTsFile(Translator &tor, const QString &tsFileName, bool /* verbose */)
{
    ConversionData cd;
    bool ok = tor.load(tsFileName, cd, QLatin1String("auto"));
    if (!ok) {
        printErr(LR::tr("lrelease error: %1").arg(cd.error()));
    } else {
        if (!cd.errors().isEmpty())
            printOut(cd.error());
    }
    cd.clearErrors();
    return ok;
}

static bool releaseTranslator(Translator &tor, const QString &qmFileName,
    ConversionData &cd, bool removeIdentical)
{
    tor.reportDuplicates(tor.resolveDuplicates(), qmFileName, cd.isVerbose());

    if (cd.isVerbose())
        printOut(LR::tr("Updating '%1'...\n").arg(qmFileName));
    if (removeIdentical) {
        if (cd.isVerbose())
            printOut(LR::tr("Removing translations equal to source text in '%1'...\n").arg(qmFileName));
        tor.stripIdenticalSourceTranslations();
    }

    QFile file(qmFileName);
    if (!file.open(QIODevice::WriteOnly)) {
        printErr(LR::tr("lrelease error: cannot create '%1': %2\n")
                                .arg(qmFileName, file.errorString()));
        return false;
    }

    tor.normalizeTranslations(cd);
    bool ok = saveQM(tor, file, cd);
    file.close();

    if (!ok) {
        printErr(LR::tr("lrelease error: cannot save '%1': %2")
                                .arg(qmFileName, cd.error()));
    } else if (!cd.errors().isEmpty()) {
        printOut(cd.error());
    }
    cd.clearErrors();
    return ok;
}

static bool releaseTsFile(const QString& tsFileName,
    ConversionData &cd, bool removeIdentical)
{
    Translator tor;
    if (!loadTsFile(tor, tsFileName, cd.isVerbose()))
        return false;

    QString qmFileName = tsFileName;
    foreach (const Translator::FileFormat &fmt, Translator::registeredFileFormats()) {
        if (qmFileName.endsWith(QLatin1Char('.') + fmt.extension)) {
            qmFileName.chop(fmt.extension.length() + 1);
            break;
        }
    }
    qmFileName += QLatin1String(".qm");

    return releaseTranslator(tor, qmFileName, cd, removeIdentical);
}

static void print(const QString &fileName, int lineNo, const QString &msg)
{
    if (lineNo)
        printErr(QString::fromLatin1("%2(%1): %3").arg(lineNo).arg(fileName, msg));
    else
        printErr(msg);
}

class ParseHandler : public ProFileParserHandler {
public:
    virtual void parseError(const QString &fileName, int lineNo, const QString &msg)
        { if (verbose) print(fileName, lineNo, msg); }

    bool verbose;
};

class EvalHandler : public ProFileEvaluatorHandler {
public:
    virtual void configError(const QString &msg)
        { printErr(msg); }
    virtual void evalError(const QString &fileName, int lineNo, const QString &msg)
        { if (verbose) print(fileName, lineNo, msg); }
    virtual void fileMessage(const QString &msg)
        { printErr(msg); }

    virtual void aboutToEval(ProFile *, ProFile *, EvalFileType) {}
    virtual void doneWithEval(ProFile *) {}

    bool verbose;
};

static ParseHandler parseHandler;
static EvalHandler evalHandler;

int main(int argc, char **argv)
{
#ifdef QT_BOOTSTRAPPED
    initBinaryDir(
#ifndef Q_OS_WIN
            argv[0]
#endif
            );
#else
    QCoreApplication app(argc, argv);
#ifndef Q_OS_WIN32
    QTranslator translator;
    QTranslator qtTranslator;
    QString sysLocale = QLocale::system().name();
    QString resourceDir = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
    if (translator.load(QLatin1String("linguist_") + sysLocale, resourceDir)
        && qtTranslator.load(QLatin1String("qt_") + sysLocale, resourceDir)) {
        app.installTranslator(&translator);
        app.installTranslator(&qtTranslator);
    }
#endif // Q_OS_WIN32
#endif // QT_BOOTSTRAPPED

    ConversionData cd;
    cd.m_verbose = true; // the default is true starting with Qt 4.2
    bool removeIdentical = false;
    Translator tor;
    QStringList inputFiles;
    QString outputFile;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-compress")) {
            cd.m_saveMode = SaveStripped;
            continue;
        } else if (!strcmp(argv[i], "-idbased")) {
            cd.m_idBased = true;
            continue;
        } else if (!strcmp(argv[i], "-nocompress")) {
            cd.m_saveMode = SaveEverything;
            continue;
        } else if (!strcmp(argv[i], "-removeidentical")) {
            removeIdentical = true;
            continue;
        } else if (!strcmp(argv[i], "-nounfinished")) {
            cd.m_ignoreUnfinished = true;
            continue;
        } else if (!strcmp(argv[i], "-markuntranslated")) {
            if (i == argc - 1) {
                printUsage();
                return 1;
            }
            cd.m_unTrPrefix = QString::fromLocal8Bit(argv[++i]);
        } else if (!strcmp(argv[i], "-silent")) {
            cd.m_verbose = false;
            continue;
        } else if (!strcmp(argv[i], "-verbose")) {
            cd.m_verbose = true;
            continue;
        } else if (!strcmp(argv[i], "-version")) {
            printOut(LR::tr("lrelease version %1\n").arg(QLatin1String(QT_VERSION_STR)));
            return 0;
        } else if (!strcmp(argv[i], "-qm")) {
            if (i == argc - 1) {
                printUsage();
                return 1;
            }
            outputFile = QString::fromLocal8Bit(argv[++i]);
        } else if (!strcmp(argv[i], "-help")) {
            printUsage();
            return 0;
        } else if (argv[i][0] == '-') {
            printUsage();
            return 1;
        } else {
            inputFiles << QString::fromLocal8Bit(argv[i]);
        }
    }

    if (inputFiles.isEmpty()) {
        printUsage();
        return 1;
    }

    foreach (const QString &inputFile, inputFiles) {
        if (inputFile.endsWith(QLatin1String(".pro"), Qt::CaseInsensitive)
            || inputFile.endsWith(QLatin1String(".pri"), Qt::CaseInsensitive)) {
            QFileInfo fi(inputFile);

            parseHandler.verbose = evalHandler.verbose = cd.isVerbose();
            ProFileOption option;
#ifdef QT_BOOTSTRAPPED
            option.initProperties(binDir + QLatin1String("/qmake"));
#else
            option.initProperties(app.applicationDirPath() + QLatin1String("/qmake"));
#endif
            ProFileParser parser(0, &parseHandler);
            ProFileEvaluator visitor(&option, &parser, &evalHandler);

            ProFile *pro;
            if (!(pro = parser.parsedProFile(QDir::cleanPath(fi.absoluteFilePath())))) {
                printErr(LR::tr(
                          "lrelease error: cannot read project file '%1'.\n")
                          .arg(inputFile));
                continue;
            }
            if (!visitor.accept(pro)) {
                printErr(LR::tr(
                          "lrelease error: cannot process project file '%1'.\n")
                          .arg(inputFile));
                pro->deref();
                continue;
            }
            pro->deref();

            QStringList translations = visitor.values(QLatin1String("TRANSLATIONS"));
            if (translations.isEmpty()) {
                printErr(LR::tr(
                          "lrelease warning: Met no 'TRANSLATIONS' entry in project file '%1'\n")
                          .arg(inputFile));
            } else {
                QDir proDir(fi.absolutePath());
                foreach (const QString &trans, translations)
                    if (!releaseTsFile(QFileInfo(proDir, trans).filePath(), cd, removeIdentical))
                        return 1;
            }
        } else {
            if (outputFile.isEmpty()) {
                if (!releaseTsFile(inputFile, cd, removeIdentical))
                    return 1;
            } else {
                if (!loadTsFile(tor, inputFile, cd.isVerbose()))
                    return 1;
            }
        }
    }

    if (!outputFile.isEmpty())
        return releaseTranslator(tor, outputFile, cd, removeIdentical) ? 0 : 1;

    return 0;
}

#ifdef QT_BOOTSTRAPPED

#ifdef Q_OS_WIN
# include <windows.h>
#endif

static void initBinaryDir(
#ifndef Q_OS_WIN
        const char *_argv0
#endif
        )
{
#ifdef Q_OS_WIN
    wchar_t module_name[MAX_PATH];
    GetModuleFileName(0, module_name, MAX_PATH);
    QFileInfo filePath = QString::fromWCharArray(module_name);
    binDir = filePath.path();
#else
    QString argv0 = QFile::decodeName(QByteArray(_argv0));
    QString absPath;

    if (!argv0.isEmpty() && argv0.at(0) == QLatin1Char('/')) {
        /*
          If argv0 starts with a slash, it is already an absolute
          file path.
        */
        absPath = argv0;
    } else if (argv0.contains(QLatin1Char('/'))) {
        /*
          If argv0 contains one or more slashes, it is a file path
          relative to the current directory.
        */
        absPath = QDir::current().absoluteFilePath(argv0);
    } else {
        /*
          Otherwise, the file path has to be determined using the
          PATH environment variable.
        */
        QByteArray pEnv = qgetenv("PATH");
        QDir currentDir = QDir::current();
        QStringList paths = QString::fromLocal8Bit(pEnv.constData()).split(QLatin1String(":"));
        for (QStringList::const_iterator p = paths.constBegin(); p != paths.constEnd(); ++p) {
            if ((*p).isEmpty())
                continue;
            QString candidate = currentDir.absoluteFilePath(*p + QLatin1Char('/') + argv0);
            QFileInfo candidate_fi(candidate);
            if (candidate_fi.exists() && !candidate_fi.isDir()) {
                binDir = candidate_fi.canonicalPath();
                return;
            }
        }
        return;
    }

    QFileInfo fi(absPath);
    if (fi.exists())
        binDir = fi.canonicalPath();
#endif
}

#endif // QT_BOOTSTRAPPED
