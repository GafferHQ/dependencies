/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljsast_p.h>
#include <private/qv4codegen_p.h>
#include <private/qv4value_p.h>
#include <private/qqmlirbuilder_p.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QVariant>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QLibraryInfo>

#include <iostream>
#include <algorithm>

QT_USE_NAMESPACE

QStringList g_qmlImportPaths;

static void printUsage(const QString &appNameIn)
{
    const std::wstring appName = appNameIn.toStdWString();
#ifndef QT_BOOTSTRAPPED
    const QString qmlPath = QLibraryInfo::location(QLibraryInfo::Qml2ImportsPath);
#else
    const QString qmlPath = QStringLiteral("/home/user/dev/qt-install/qml");
#endif
    std::wcerr
        << "Usage: " << appName << " -rootPath path/to/app/qml/directory -importPath path/to/qt/qml/directory\n"
           "       " << appName << " -qmlFiles file1 file2 -importPath path/to/qt/qml/directory\n\n"
           "Example: " << appName << " -rootPath . -importPath "
        << QDir::toNativeSeparators(qmlPath).toStdWString()
        << '\n';
}

QVariantList findImportsInAst(QQmlJS::AST::UiHeaderItemList *headerItemList, const QString &code, const QString &path)
{
    QVariantList imports;

    // extract uri and version from the imports (which look like "import Foo.Bar 1.2.3")
    for (QQmlJS::AST::UiHeaderItemList *headerItemIt = headerItemList; headerItemIt; headerItemIt = headerItemIt->next) {
        QVariantMap import;
        QQmlJS::AST::UiImport *importNode = QQmlJS::AST::cast<QQmlJS::AST::UiImport *>(headerItemIt->headerItem);
        if (!importNode)
            continue;
        // handle directory imports
        if (!importNode->fileName.isEmpty()) {
            QString name = importNode->fileName.toString();
            import[QStringLiteral("name")] = name;
            if (name.endsWith(QLatin1String(".js"))) {
                import[QStringLiteral("type")] = QStringLiteral("javascript");
            } else {
                import[QStringLiteral("type")] = QStringLiteral("directory");
            }

            import[QStringLiteral("path")] = QDir::cleanPath(path + QLatin1Char('/') + name);
        } else {
            // Walk the id chain ("Foo" -> "Bar" -> etc)
            QString  name;
            QQmlJS::AST::UiQualifiedId *uri = importNode->importUri;
            while (uri) {
                name.append(uri->name);
                name.append(QLatin1Char('.'));
                uri = uri->next;
            }
            name.chop(1); // remove trailing "."
            if (!name.isEmpty())
                import[QStringLiteral("name")] = name;
            import[QStringLiteral("type")] = QStringLiteral("module");
            import[QStringLiteral("version")] = code.mid(importNode->versionToken.offset, importNode->versionToken.length);
        }

        imports.append(import);
    }

    return imports;
}

// Read the qmldir file, extract a list of plugins by
// parsing the "plugin"  and "classname" lines.
QVariantMap pluginsForModulePath(const QString &modulePath) {
    QFile qmldirFile(modulePath + QStringLiteral("/qmldir"));
    if (!qmldirFile.exists())
        return QVariantMap();

    qmldirFile.open(QIODevice::ReadOnly | QIODevice::Text);

    // a qml import may contain several plugins
    QString plugins;
    QString classnames;
    QStringList dependencies;
    QByteArray line;
    do {
        line = qmldirFile.readLine();
        if (line.startsWith("plugin")) {
            plugins += QString::fromUtf8(line.split(' ').at(1));
            plugins += QLatin1Char(' ');
        } else if (line.startsWith("classname")) {
            classnames += QString::fromUtf8(line.split(' ').at(1));
            classnames += QLatin1Char(' ');
        } else if (line.startsWith("depends")) {
            QList<QByteArray> dep = line.split(' ');
            if (dep.length() != 3)
                std::cerr << "depends: expected 2 arguments: module identifier and version" << std::endl;
            else
                dependencies << QString::fromUtf8(dep[1]) + QStringLiteral(" ") + QString::fromUtf8(dep[2]).simplified();
        }

    } while (line.length() > 0);

    QVariantMap pluginInfo;
    pluginInfo[QStringLiteral("plugins")] = plugins.simplified();
    pluginInfo[QStringLiteral("classnames")] = classnames.simplified();
    if (dependencies.length())
        pluginInfo[QStringLiteral("dependencies")] = dependencies;
    return pluginInfo;
}

// Search for a given qml import in g_qmlImportPaths.
QString resolveImportPath(const QString &uri, const QString &version)
{
    // Create path from uri (QtQuick.Controls -> QtQuick/Controls)
    QString path = uri;
    path.replace(QLatin1Char('.'), QLatin1Char('/'));
    // search for the most spesifc version first
    QString versionedName = path + QLatin1Char('.') + version;
    while (true) {
        // look in all g_qmlImportPaths
        foreach (const QString &qmlImportPath, g_qmlImportPaths) {
            QString candidatePath = QDir::cleanPath(qmlImportPath + QLatin1Char('/') + versionedName);
            if (QDir(candidatePath).exists())
                return candidatePath; // import found
        }

        // remove the last version digit; stop if there are none left
        int lastDot = versionedName.lastIndexOf(QLatin1Char('.'));
        if (lastDot == -1)
            break;
        versionedName = versionedName.mid(0, lastDot);
    }

    return QString(); // not found
}

// Find absolute file system paths and plugins for a list of modules.
QVariantList findPathsForModuleImports(const QVariantList &imports)
{
    QVariantList done;
    QVariantList importsCopy(imports);

    for (int i = 0; i < importsCopy.length(); ++i) {
        QVariantMap import = qvariant_cast<QVariantMap>(importsCopy[i]);
        if (import[QStringLiteral("type")] == QLatin1String("module")) {
            QString path = resolveImportPath(import.value(QStringLiteral("name")).toString(), import.value(QStringLiteral("version")).toString());
            if (!path.isEmpty())
                import[QStringLiteral("path")] = path;
            QVariantMap plugininfo = pluginsForModulePath(import.value(QStringLiteral("path")).toString());
            QString plugins = plugininfo.value(QStringLiteral("plugins")).toString();
            QString classnames = plugininfo.value(QStringLiteral("classnames")).toString();
            if (!plugins.isEmpty())
                import[QStringLiteral("plugin")] = plugins;
            if (!classnames.isEmpty())
                import[QStringLiteral("classname")] = classnames;
            if (plugininfo.contains(QStringLiteral("dependencies"))) {
                QStringList dependencies = plugininfo.value(QStringLiteral("dependencies")).toStringList();
                foreach (const QString &line, dependencies) {
                    QList<QString> dep = line.split(QLatin1Char(' '));
                    QVariantMap depImport;
                    depImport[QStringLiteral("type")] = QStringLiteral("module");
                    depImport[QStringLiteral("name")] = dep[0];
                    depImport[QStringLiteral("version")] = dep[1];
                    importsCopy.append(depImport);
                }
            }
        }
        done.append(import);
    }
    return done;
}

// Scan a single qml file for import statements
static QVariantList findQmlImportsInQmlCode(const QString &filePath, const QString &code)
{
    QQmlJS::Engine engine;
    QQmlJS::Lexer lexer(&engine);
    lexer.setCode(code, /*line = */ 1);
    QQmlJS::Parser parser(&engine);

    if (!parser.parse() || !parser.diagnosticMessages().isEmpty()) {
        // Extract errors from the parser
        foreach (const QQmlJS::DiagnosticMessage &m, parser.diagnosticMessages()) {
            std::cerr << QDir::toNativeSeparators(filePath).toStdString() << ':'
                      << m.loc.startLine << ':' << m.message.toStdString() << std::endl;
        }
        return QVariantList();
    }
    return findImportsInAst(parser.ast()->headers, code, filePath);
}

// Scan a single qml file for import statements
static QVariantList findQmlImportsInQmlFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
           std::cerr << "Cannot open input file " << QDir::toNativeSeparators(file.fileName()).toStdString()
                     << ':' << file.errorString().toStdString() << std::endl;
           return QVariantList();
    }
    QString code = QString::fromUtf8(file.readAll());
    return findQmlImportsInQmlCode(filePath, code);
}

struct ImportCollector : public QQmlJS::Directives
{
    QVariantList imports;

    virtual void importFile(const QString &jsfile, const QString &module, int line, int column)
    {
        QVariantMap entry;
        entry[QLatin1String("type")] = QStringLiteral("javascript");
        entry[QLatin1String("path")] = jsfile;
        imports << entry;

        Q_UNUSED(module);
        Q_UNUSED(line);
        Q_UNUSED(column);
    }

    virtual void importModule(const QString &uri, const QString &version, const QString &module, int line, int column)
    {
        QVariantMap entry;
        if (uri.contains(QLatin1Char('/'))) {
            entry[QLatin1String("type")] = QStringLiteral("directory");
            entry[QLatin1String("name")] = uri;
        } else {
            entry[QLatin1String("type")] = QStringLiteral("module");
            entry[QLatin1String("name")] = uri;
            entry[QLatin1String("version")] = version;
        }
        imports << entry;

        Q_UNUSED(module);
        Q_UNUSED(line);
        Q_UNUSED(column);
    }
};

// Scan a single javascrupt file for import statements
QVariantList findQmlImportsInJavascriptFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
           std::cerr << "Cannot open input file " << QDir::toNativeSeparators(file.fileName()).toStdString()
                     << ':' << file.errorString().toStdString() << std::endl;
           return QVariantList();
    }

    QString sourceCode = QString::fromUtf8(file.readAll());
    file.close();

    QQmlJS::Engine ee;
    ImportCollector collector;
    ee.setDirectives(&collector);
    QQmlJS::Lexer lexer(&ee);
    lexer.setCode(sourceCode, /*line*/1, /*qml mode*/false);
    QQmlJS::Parser parser(&ee);
    parser.parseProgram();

    foreach (const QQmlJS::DiagnosticMessage &m, parser.diagnosticMessages())
        if (m.isError())
            return QVariantList();

    return collector.imports;
}

// Scan a single qml or js file for import statements
QVariantList findQmlImportsInFile(const QString &filePath)
{
    QVariantList imports;
    if (filePath == QLatin1String("-")) {
        QFile f;
        if (f.open(stdin, QIODevice::ReadOnly))
            imports = findQmlImportsInQmlCode(QLatin1String("<stdin>"), QString::fromUtf8(f.readAll()));
    } else if (filePath.endsWith(QLatin1String(".qml"))) {
        imports = findQmlImportsInQmlFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".js"))) {
        imports = findQmlImportsInJavascriptFile(filePath);
    }

    return findPathsForModuleImports(imports);
}

// Merge two lists of imports, discard duplicates.
QVariantList mergeImports(const QVariantList &a, const QVariantList &b)
{
    QVariantList merged = a;
    foreach (const QVariant &variant, b) {
        if (!merged.contains(variant))
            merged.append(variant);
    }
    return merged;
}

// Predicates needed by findQmlImportsInDirectory.

struct isMetainfo {
    bool operator() (const QFileInfo &x) const {
        return x.suffix() == QLatin1String("metainfo");
    }
};

struct pathStartsWith {
    pathStartsWith(const QString &path) : _path(path) {}
    bool operator() (const QString &x) const {
        return _path.startsWith(x);
    }
    const QString _path;
};



// Scan all qml files in directory for import statements
QVariantList findQmlImportsInDirectory(const QString &qmlDir)
{
    QVariantList ret;
    if (qmlDir.isEmpty())
        return ret;

    QDirIterator iterator(qmlDir, QDir::AllDirs | QDir::NoDotDot, QDirIterator::Subdirectories);
    QStringList blacklist;

    while (iterator.hasNext()) {
        iterator.next();
        const QString path = iterator.filePath();
        const QFileInfoList entries = QDir(path).entryInfoList();

        // Skip designer related stuff
        if (std::find_if(entries.cbegin(), entries.cend(), isMetainfo()) != entries.cend()) {
            blacklist << path;
            continue;
        }

        if (std::find_if(blacklist.cbegin(), blacklist.cend(), pathStartsWith(path)) != blacklist.cend())
            continue;

        // skip obvious build output directories
        if (path.contains(QLatin1String("Debug-iphoneos")) || path.contains(QLatin1String("Release-iphoneos")) ||
            path.contains(QLatin1String("Debug-iphonesimulator")) || path.contains(QLatin1String("Release-iphonesimulator"))
#ifdef Q_OS_WIN
            || path.contains(QLatin1String("/release/")) || path.contains(QLatin1String("/debug/"))
#endif
        ){
            continue;
        }

        foreach (const QFileInfo &x, entries)
            if (x.isFile())
                ret = mergeImports(ret, findQmlImportsInFile(x.absoluteFilePath()));
     }
     return ret;
}

QSet<QString> importModulePaths(QVariantList imports) {
    QSet<QString> ret;
    foreach (const QVariant &importVariant, imports) {
        QVariantMap import = qvariant_cast<QVariantMap>(importVariant);
        QString path = import.value(QStringLiteral("path")).toString();
        QString type = import.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("module") && !path.isEmpty())
            ret.insert(QDir(path).canonicalPath());
    }
    return ret;
}

// Find Qml Imports Recursively from a root set of qml files.
// The directories in qmlDirs are searched recursively.
// The files in qmlFiles parsed directly.
QVariantList findQmlImportsRecursively(const QStringList &qmlDirs, const QStringList &scanFiles)
{
    QVariantList ret;

    // scan all app root qml directories for imports
    foreach (const QString &qmlDir, qmlDirs) {
        QVariantList imports = findQmlImportsInDirectory(qmlDir);
        ret = mergeImports(ret, imports);
    }

    // scan app qml files for imports
    foreach (const QString &file, scanFiles) {
        QVariantList imports = findQmlImportsInFile(file);
        ret = mergeImports(ret, imports);
    }


    // get the paths to theimports found in the app qml
    QSet<QString> toVisit = importModulePaths(ret);

    // recursivly scan for import dependencies.
    QSet<QString> visited;
    while (!toVisit.isEmpty()) {
        QString qmlDir = *toVisit.begin();
        toVisit.erase(toVisit.begin());
        visited.insert(qmlDir);

        QVariantList imports = findQmlImportsInDirectory(qmlDir);
        ret = mergeImports(ret, imports);

        QSet<QString> candidatePaths = importModulePaths(ret);
        candidatePaths.subtract(visited);
        toVisit.unite(candidatePaths);
    }
    return ret;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    const QString appName = QFileInfo(app.applicationFilePath()).baseName();
    if (args.size() < 2) {
        printUsage(appName);
        return 1;
    }

    QStringList qmlRootPaths;
    QStringList scanFiles;
    QStringList qmlImportPaths;

    int i = 1;
    while (i < args.count()) {
        const QString &arg = args.at(i);
        ++i;
        QStringList *argReceiver = 0;
        if (!arg.startsWith(QLatin1Char('-')) || arg == QLatin1String("-")) {
            qmlRootPaths += arg;
        } else if (arg == QLatin1String("-rootPath")) {
            if (i >= args.count())
                std::cerr << "-rootPath requires an argument\n";
            argReceiver = &qmlRootPaths;
        } else if (arg == QLatin1String("-qmlFiles")) {
            if (i >= args.count())
                std::cerr << "-qmlFiles requires an argument\n";
            argReceiver = &scanFiles;
        } else if (arg == QLatin1String("-jsFiles")) {
            if (i >= args.count())
                std::cerr << "-jsFiles requires an argument\n";
            argReceiver = &scanFiles;
        } else if (arg == QLatin1String("-importPath")) {
            if (i >= args.count())
                std::cerr << "-importPath requires an argument\n";
            argReceiver = &qmlImportPaths;
        } else {
            std::cerr << "Invalid argument: \"" << qPrintable(arg) << "\"\n";
            return 1;
        }

        while (i < args.count()) {
            const QString arg = args.at(i);
            if (arg.startsWith(QLatin1Char('-')) && arg != QLatin1String("-"))
                break;
            ++i;
            *argReceiver += arg;
        }
    }

    g_qmlImportPaths = qmlImportPaths;

    // Find the imports!
    QVariantList imports = findQmlImportsRecursively(qmlRootPaths, scanFiles);

    // Convert to JSON
    QByteArray json = QJsonDocument(QJsonArray::fromVariantList(imports)).toJson();
    std::cout << json.constData() << std::endl;
    return 0;
}
