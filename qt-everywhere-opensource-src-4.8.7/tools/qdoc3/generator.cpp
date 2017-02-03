/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

/*
  generator.cpp
*/
#include <qdir.h>
#ifdef DEBUG_MULTIPLE_QDOCCONF_FILES
#include <qdebug.h>
#endif
#include "codemarker.h"
#include "config.h"
#include "doc.h"
#include "editdistance.h"
#include "generator.h"
#include "node.h"
#include "openedlist.h"
#include "quoter.h"
#include "separator.h"
#include "tokenizer.h"
#include "ditaxmlgenerator.h"

QT_BEGIN_NAMESPACE

QList<Generator *> Generator::generators;
QMap<QString, QMap<QString, QString> > Generator::fmtLeftMaps;
QMap<QString, QMap<QString, QString> > Generator::fmtRightMaps;
QMap<QString, QStringList> Generator::imgFileExts;
QSet<QString> Generator::outputFormats;
QStringList Generator::imageFiles;
QStringList Generator::imageDirs;
QStringList Generator::exampleDirs;
QStringList Generator::exampleImgExts;
QStringList Generator::scriptFiles;
QStringList Generator::scriptDirs;
QStringList Generator::styleFiles;
QStringList Generator::styleDirs;
QString Generator::outDir;
QString Generator::project;
QHash<QString, QString> Generator::outputPrefixes;

QString Generator::sinceTitles[] =
    {
        "    New Namespaces",
        "    New Classes",
        "    New Member Functions",
        "    New Functions in Namespaces",
        "    New Global Functions",
        "    New Macros",
        "    New Enum Types",
        "    New Typedefs",
        "    New Properties",
        "    New Variables",
        "    New QML Elements",
        "    New QML Properties",
        "    New QML Signals",
        "    New QML Methods",
        ""
    };

static void singularPlural(Text& text, const NodeList& nodes)
{
    if (nodes.count() == 1)
        text << " is";
    else
        text << " are";
}

Generator::Generator()
    : amp("&amp;"),
      lt("&lt;"),
      gt("&gt;"),
      quot("&quot;"),
      tag("</?@[^>]*>")
{
    generators.prepend(this);
}

Generator::~Generator()
{
    generators.removeAll(this);
}

void Generator::initializeGenerator(const Config & /* config */)
{
}

void Generator::terminateGenerator()
{
}

void Generator::initialize(const Config &config)
{
    outputFormats = config.getStringSet(CONFIG_OUTPUTFORMATS);
    if (!outputFormats.isEmpty()) {
        outDir = config.getString(CONFIG_OUTPUTDIR);
        if (outDir.isEmpty())
            config.lastLocation().fatal(tr("No output directory specified in configuration file"));

        QDir dirInfo;
        if (dirInfo.exists(outDir)) {
            if (!Config::removeDirContents(outDir))
                config.lastLocation().error(tr("Cannot empty output directory '%1'").arg(outDir));
        }
        else {
            if (!dirInfo.mkpath(outDir))
                config.lastLocation().fatal(tr("Cannot create output directory '%1'").arg(outDir));
        }

        if (!dirInfo.mkdir(outDir + "/images"))
            config.lastLocation().fatal(tr("Cannot create output directory '%1'")
                                        .arg(outDir + "/images"));
        if (!dirInfo.mkdir(outDir + "/images/used-in-examples"))
            config.lastLocation().fatal(tr("Cannot create output directory '%1'")
                                        .arg(outDir + "/images/used-in-examples"));
        if (!dirInfo.mkdir(outDir + "/scripts"))
            config.lastLocation().fatal(tr("Cannot create output directory '%1'")
                                        .arg(outDir + "/scripts"));
        if (!dirInfo.mkdir(outDir + "/style"))
            config.lastLocation().fatal(tr("Cannot create output directory '%1'")
                                        .arg(outDir + "/style"));
    }

    imageFiles = config.getStringList(CONFIG_IMAGES);
    imageDirs = config.getStringList(CONFIG_IMAGEDIRS);
    scriptFiles = config.getStringList(CONFIG_SCRIPTS);
    scriptDirs = config.getStringList(CONFIG_SCRIPTDIRS);
    styleFiles = config.getStringList(CONFIG_STYLES);
    styleDirs = config.getStringList(CONFIG_STYLEDIRS);
    exampleDirs = config.getStringList(CONFIG_EXAMPLEDIRS);
    exampleImgExts = config.getStringList(CONFIG_EXAMPLES + Config::dot +
                                          CONFIG_IMAGEEXTENSIONS);

    QString imagesDotFileExtensions =
        CONFIG_IMAGES + Config::dot + CONFIG_FILEEXTENSIONS;
    QSet<QString> formats = config.subVars(imagesDotFileExtensions);
    QSet<QString>::ConstIterator f = formats.begin();
    while (f != formats.end()) {
        imgFileExts[*f] = config.getStringList(imagesDotFileExtensions +
                                               Config::dot + *f);
        ++f;
    }

    QList<Generator *>::ConstIterator g = generators.begin();
    while (g != generators.end()) {
        if (outputFormats.contains((*g)->format())) {
            (*g)->initializeGenerator(config);
            QStringList extraImages =
                config.getStringList(CONFIG_EXTRAIMAGES+Config::dot+(*g)->format());
            QStringList::ConstIterator e = extraImages.begin();
            while (e != extraImages.end()) {
                QString userFriendlyFilePath;
                QString filePath = Config::findFile(config.lastLocation(),
                                                    imageFiles,
                                                    imageDirs,
                                                    *e,
                                                    imgFileExts[(*g)->format()],
                                                    userFriendlyFilePath);
                if (!filePath.isEmpty())
                    Config::copyFile(config.lastLocation(),
                                     filePath,
                                     userFriendlyFilePath,
                                     (*g)->outputDir() +
                                     "/images");
                ++e;
            }

            // Documentation template handling
            QString templateDir = config.getString(
                (*g)->format() + Config::dot + CONFIG_TEMPLATEDIR);

            if (!templateDir.isEmpty()) {
                QStringList noExts;
                QStringList searchDirs = QStringList() << templateDir;
                QStringList scripts =
                    config.getStringList((*g)->format()+Config::dot+CONFIG_SCRIPTS);
                e = scripts.begin();
                while (e != scripts.end()) {
                    QString userFriendlyFilePath;
                    QString filePath = Config::findFile(config.lastLocation(),
                                                        scriptFiles,
                                                        searchDirs,
                                                        *e,
                                                        noExts,
                                                        userFriendlyFilePath);
                    if (!filePath.isEmpty())
                        Config::copyFile(config.lastLocation(),
                                         filePath,
                                         userFriendlyFilePath,
                                         (*g)->outputDir() +
                                         "/scripts");
                    ++e;
                }

                QStringList styles =
                    config.getStringList((*g)->format()+Config::dot+CONFIG_STYLESHEETS);
                e = styles.begin();
                while (e != styles.end()) {
                    QString userFriendlyFilePath;
                    QString filePath = Config::findFile(config.lastLocation(),
                                                        styleFiles,
                                                        searchDirs,
                                                        *e,
                                                        noExts,
                                                        userFriendlyFilePath);
                    if (!filePath.isEmpty())
                        Config::copyFile(config.lastLocation(),
                                         filePath,
                                         userFriendlyFilePath,
                                         (*g)->outputDir() +
                                         "/style");
                    ++e;
                }
            }
        }
        ++g;
    }

    QRegExp secondParamAndAbove("[\2-\7]");
    QSet<QString> formattingNames = config.subVars(CONFIG_FORMATTING);
    QSet<QString>::ConstIterator n = formattingNames.begin();
    while (n != formattingNames.end()) {
        QString formattingDotName = CONFIG_FORMATTING + Config::dot + *n;

        QSet<QString> formats = config.subVars(formattingDotName);
        QSet<QString>::ConstIterator f = formats.begin();
        while (f != formats.end()) {
            QString def = config.getString(formattingDotName +
                                           Config::dot + *f);
            if (!def.isEmpty()) {
                int numParams = Config::numParams(def);
                int numOccs = def.count("\1");

                if (numParams != 1) {
                    config.lastLocation().warning(tr("Formatting '%1' must "
                                                     "have exactly one "
                                                     "parameter (found %2)")
                                                  .arg(*n).arg(numParams));
                }
                else if (numOccs > 1) {
                    config.lastLocation().fatal(tr("Formatting '%1' must "
                                                   "contain exactly one "
                                                   "occurrence of '\\1' "
                                                   "(found %2)")
                                                .arg(*n).arg(numOccs));
                }
                else {
                    int paramPos = def.indexOf("\1");
                    fmtLeftMaps[*f].insert(*n, def.left(paramPos));
                    fmtRightMaps[*f].insert(*n, def.mid(paramPos + 1));
                }
            }
            ++f;
        }
        ++n;
    }

    project = config.getString(CONFIG_PROJECT);

    QStringList prefixes = config.getStringList(CONFIG_OUTPUTPREFIXES);
    if (!prefixes.isEmpty()) {
        foreach (QString prefix, prefixes)
            outputPrefixes[prefix] = config.getString(
                CONFIG_OUTPUTPREFIXES + Config::dot + prefix);
    } else
        outputPrefixes[QLatin1String("QML")] = QLatin1String("qml-");
}

void Generator::terminate()
{
    QList<Generator *>::ConstIterator g = generators.begin();
    while (g != generators.end()) {
        if (outputFormats.contains((*g)->format()))
            (*g)->terminateGenerator();
        ++g;
    }

    fmtLeftMaps.clear();
    fmtRightMaps.clear();
    imgFileExts.clear();
    imageFiles.clear();
    imageDirs.clear();
    outDir = "";
    QmlClassNode::clear();
}

Generator *Generator::generatorForFormat(const QString& format)
{
    QList<Generator *>::ConstIterator g = generators.begin();
    while (g != generators.end()) {
        if ((*g)->format() == format)
            return *g;
        ++g;
    }
    return 0;
}

void Generator::startText(const Node * /* relative */,
                          CodeMarker * /* marker */)
{
}

void Generator::endText(const Node * /* relative */,
                        CodeMarker * /* marker */)
{
}

int Generator::generateAtom(const Atom * /* atom */,
                            const Node * /* relative */,
                            CodeMarker * /* marker */)
{
    return 0;
}

void Generator::generateClassLikeNode(const InnerNode * /* classe */,
                                      CodeMarker * /* marker */)
{
}

void Generator::generateFakeNode(const FakeNode * /* fake */,
                                 CodeMarker * /* marker */)
{
}

bool Generator::generateText(const Text& text,
                             const Node *relative,
                             CodeMarker *marker)
{
    bool result = false;
    if (text.firstAtom() != 0) {
        int numAtoms = 0;
        startText(relative, marker);
        generateAtomList(text.firstAtom(),
                         relative,
                         marker,
                         true,
                         numAtoms);
        endText(relative, marker);
        result = true;
    }
    return result;
}

#ifdef QDOC_QML
/*!
  Extract sections of markup text surrounded by \e qmltext
  and \e endqmltext and output them.
 */
bool Generator::generateQmlText(const Text& text,
                                const Node *relative,
                                CodeMarker *marker,
                                const QString& /* qmlName */ )
{
    const Atom* atom = text.firstAtom();
    bool result = false;

    if (atom != 0) {
        startText(relative, marker);
        while (atom) {
            if (atom->type() != Atom::QmlText)
                atom = atom->next();
            else {
                atom = atom->next();
                while (atom && (atom->type() != Atom::EndQmlText)) {
                    int n = 1 + generateAtom(atom, relative, marker);
                    while (n-- > 0)
                        atom = atom->next();
                }
            }
        }
        endText(relative, marker);
        result = true;
    }
    return result;
}
#endif

void Generator::generateBody(const Node *node, CodeMarker *marker)
{
    bool quiet = false;

    if (node->type() == Node::Fake) {
        const FakeNode *fake = static_cast<const FakeNode *>(node);
        if (fake->subType() == Node::Example) {
            generateExampleFiles(fake, marker);
        }
        else if ((fake->subType() == Node::File) || (fake->subType() == Node::Image)) {
            quiet = true;
        }
    }

    if (node->doc().isEmpty()) {
        if (!quiet && !node->isReimp()) { // ### might be unnecessary
            node->location().warning(tr("No documentation for '%1'")
                            .arg(marker->plainFullName(node)));
        }
    }
    else {
        if (node->type() == Node::Function) {
            const FunctionNode *func = static_cast<const FunctionNode *>(node);
            if (func->reimplementedFrom() != 0)
                generateReimplementedFrom(func, marker);
        }

        if (!generateText(node->doc().body(), node, marker)) {
            if (node->isReimp())
                return;
        }

        if (node->type() == Node::Enum) {
            const EnumNode *enume = (const EnumNode *) node;

            QSet<QString> definedItems;
            QList<EnumItem>::ConstIterator it = enume->items().begin();
            while (it != enume->items().end()) {
                definedItems.insert((*it).name());
                ++it;
            }

            QSet<QString> documentedItems = enume->doc().enumItemNames().toSet();
            QSet<QString> allItems = definedItems + documentedItems;
            if (allItems.count() > definedItems.count() ||
                 allItems.count() > documentedItems.count()) {
                QSet<QString>::ConstIterator a = allItems.begin();
                while (a != allItems.end()) {
                    if (!definedItems.contains(*a)) {
                        QString details;
                        QString best = nearestName(*a, definedItems);
                        if (!best.isEmpty() && !documentedItems.contains(best))
                            details = tr("Maybe you meant '%1'?").arg(best);

                        node->doc().location().warning(
                            tr("No such enum item '%1' in %2").arg(*a).arg(marker->plainFullName(node)),
                            details);
                    }
                    else if (!documentedItems.contains(*a)) {
                        node->doc().location().warning(
                            tr("Undocumented enum item '%1' in %2").arg(*a).arg(marker->plainFullName(node)));
                    }
                    ++a;
                }
            }
        }
        else if (node->type() == Node::Function) {
            const FunctionNode *func = static_cast<const FunctionNode *>(node);
            QSet<QString> definedParams;
            QList<Parameter>::ConstIterator p = func->parameters().begin();
            while (p != func->parameters().end()) {
                if ((*p).name().isEmpty() && (*p).leftType() != QLatin1String("...")
                        && func->name() != QLatin1String("operator++")
                        && func->name() != QLatin1String("operator--")) {
                    node->doc().location().warning(tr("Missing parameter name"));
                }
                else {
                    definedParams.insert((*p).name());
                }
                ++p;
            }

            QSet<QString> documentedParams = func->doc().parameterNames();
            QSet<QString> allParams = definedParams + documentedParams;
            if (allParams.count() > definedParams.count()
                    || allParams.count() > documentedParams.count()) {
                QSet<QString>::ConstIterator a = allParams.begin();
                while (a != allParams.end()) {
                    if (!definedParams.contains(*a)) {
                        QString details;
                        QString best = nearestName(*a, definedParams);
                        if (!best.isEmpty())
                            details = tr("Maybe you meant '%1'?").arg(best);

                        node->doc().location().warning(
                            tr("No such parameter '%1' in %2").arg(*a).arg(marker->plainFullName(node)),
                            details);
                    }
                    else if (!(*a).isEmpty() && !documentedParams.contains(*a)) {
                        bool needWarning = (func->status() > Node::Obsolete);
                        if (func->overloadNumber() > 1) {
                            FunctionNode *primaryFunc =
                                    func->parent()->findFunctionNode(func->name());
                            if (primaryFunc) {
                                foreach (const Parameter &param,
                                         primaryFunc->parameters()) {
                                    if (param.name() == *a) {
                                        needWarning = false;
                                        break;
                                    }
                                }
                            }
                        }
                        if (needWarning && !func->isReimp())
                            node->doc().location().warning(
                                tr("Undocumented parameter '%1' in %2")
                                .arg(*a).arg(marker->plainFullName(node)));
                    }
                    ++a;
                }
            }
            /*
              Something like this return value check should
              be implemented at some point.
            */
            if (func->status() > Node::Obsolete && func->returnType() == "bool"
                    && func->reimplementedFrom() == 0 && !func->isOverload()) {
                QString body = func->doc().body().toString();
                if (!body.contains("return", Qt::CaseInsensitive))
                    node->doc().location().warning(tr("Undocumented return value"));
            }
        }
    }

    if (node->type() == Node::Fake) {
        const FakeNode *fake = static_cast<const FakeNode *>(node);
        if (fake->subType() == Node::File) {
            Text text;
            Quoter quoter;
            Doc::quoteFromFile(fake->doc().location(), quoter, fake->name());
            QString code = quoter.quoteTo(fake->location(), "", "");
            CodeMarker *codeMarker = CodeMarker::markerForFileName(fake->name());
            text << Atom(codeMarker->atomType(), code);
            generateText(text, fake, codeMarker);
        }
    }
}

void Generator::generateAlsoList(const Node *node, CodeMarker *marker)
{
    QList<Text> alsoList = node->doc().alsoList();
    supplementAlsoList(node, alsoList);

    if (!alsoList.isEmpty()) {
        Text text;
        text << Atom::ParaLeft
             << Atom(Atom::FormattingLeft,ATOM_FORMATTING_BOLD)
             << "See also "
             << Atom(Atom::FormattingRight,ATOM_FORMATTING_BOLD);

        for (int i = 0; i < alsoList.size(); ++i)
            text << alsoList.at(i) << separator(i, alsoList.size());

        text << Atom::ParaRight;
        generateText(text, node, marker);
    }
}

/*!
  Generate a list of maintainers in the output
 */
void Generator::generateMaintainerList(const InnerNode* node, CodeMarker* marker)
{
    QStringList sl = getMetadataElements(node,"maintainer");

    if (!sl.isEmpty()) {
        Text text;
        text << Atom::ParaLeft
             << Atom(Atom::FormattingLeft,ATOM_FORMATTING_BOLD)
             << "Maintained by: "
             << Atom(Atom::FormattingRight,ATOM_FORMATTING_BOLD);

        for (int i = 0; i < sl.size(); ++i)
            text << sl.at(i) << separator(i, sl.size());

        text << Atom::ParaRight;
        generateText(text, node, marker);
    }
}

void Generator::generateInherits(const ClassNode *classe, CodeMarker *marker)
{
    QList<RelatedClass>::ConstIterator r;
    int index;

    if (!classe->baseClasses().isEmpty()) {
        Text text;
        text << Atom::ParaLeft
             << Atom(Atom::FormattingLeft,ATOM_FORMATTING_BOLD)
             << "Inherits: "
             << Atom(Atom::FormattingRight,ATOM_FORMATTING_BOLD);

        r = classe->baseClasses().begin();
        index = 0;
        while (r != classe->baseClasses().end()) {
            text << Atom(Atom::LinkNode, CodeMarker::stringForNode((*r).node))
                 << Atom(Atom::FormattingLeft, ATOM_FORMATTING_LINK)
                 << Atom(Atom::String, (*r).dataTypeWithTemplateArgs)
                 << Atom(Atom::FormattingRight, ATOM_FORMATTING_LINK);

            if ((*r).access == Node::Protected) {
                text << " (protected)";
            }
            else if ((*r).access == Node::Private) {
                text << " (private)";
            }
            text << separator(index++, classe->baseClasses().count());
            ++r;
        }
        text << Atom::ParaRight;
        generateText(text, classe, marker);
    }
}

#ifdef QDOC_QML
/*!
 */
void Generator::generateQmlInherits(const QmlClassNode* , CodeMarker* )
{
    // stub.
}
#endif

void Generator::generateInheritedBy(const ClassNode *classe,
                                    CodeMarker *marker)
{
    if (!classe->derivedClasses().isEmpty()) {
        Text text;
        text << Atom::ParaLeft
             << Atom(Atom::FormattingLeft,ATOM_FORMATTING_BOLD)
             << "Inherited by: "
             << Atom(Atom::FormattingRight,ATOM_FORMATTING_BOLD);

        appendSortedNames(text, classe, classe->derivedClasses(), marker);
        text << Atom::ParaRight;
        generateText(text, classe, marker);
    }
}

/*!
  This function is called when the documentation for an
  example is being formatted. It outputs the list of source
  files comprising the example, and the list of images used
  by the example. The images are copied into a subtree of
  \c{...doc/html/images/used-in-examples/...}
 */
void Generator::generateFileList(const FakeNode* fake,
                                 CodeMarker* marker,
                                 Node::SubType subtype,
                                 const QString& tag)
{
    int count = 0;
    Text text;
    OpenedList openedList(OpenedList::Bullet);

    text << Atom::ParaLeft << tag << Atom::ParaRight
         << Atom(Atom::ListLeft, openedList.styleString());

    foreach (const Node* child, fake->childNodes()) {
        if (child->subType() == subtype) {
            ++count;
            QString file = child->name();
            if (subtype == Node::Image) {
                if (!file.isEmpty()) {
                    QDir dirInfo;
                    QString userFriendlyFilePath;
                    QString srcPath = Config::findFile(fake->location(),
                                                       QStringList(),
                                                       exampleDirs,
                                                       file,
                                                       exampleImgExts,
                                                       userFriendlyFilePath);
                    userFriendlyFilePath.truncate(userFriendlyFilePath.lastIndexOf('/'));

                    QString imgOutDir = outDir + "/images/used-in-examples/" + userFriendlyFilePath;
                    if (!dirInfo.mkpath(imgOutDir))
                        fake->location().fatal(tr("Cannot create output directory '%1'")
                                               .arg(imgOutDir));

                    QString imgOutName = Config::copyFile(fake->location(),
                                                          srcPath,
                                                          file,
                                                          imgOutDir);
                }

            }

            openedList.next();
            text << Atom(Atom::ListItemNumber, openedList.numberString())
                 << Atom(Atom::ListItemLeft, openedList.styleString())
                 << Atom::ParaLeft
                 << Atom(Atom::Link, file)
                 << Atom(Atom::FormattingLeft, ATOM_FORMATTING_LINK)
                 << file
                 << Atom(Atom::FormattingRight, ATOM_FORMATTING_LINK)
                 << Atom::ParaRight
                 << Atom(Atom::ListItemRight, openedList.styleString());
        }
    }
    text << Atom(Atom::ListRight, openedList.styleString());
    if (count > 0)
        generateText(text, fake, marker);
}

void Generator::generateExampleFiles(const FakeNode *fake, CodeMarker *marker)
{
    if (fake->childNodes().isEmpty())
        return;
    generateFileList(fake, marker, Node::File, QString("Files:"));
    generateFileList(fake, marker, Node::Image, QString("Images:"));
}

QString Generator::indent(int level, const QString& markedCode)
{
    if (level == 0)
        return markedCode;

    QString t;
    int column = 0;

    int i = 0;
    while (i < (int) markedCode.length()) {
        if (markedCode.at(i) == QLatin1Char('\n')) {
            column = 0;
        }
        else {
            if (column == 0) {
                for (int j = 0; j < level; j++)
                    t += QLatin1Char(' ');
            }
            column++;
        }
        t += markedCode.at(i++);
    }
    return t;
}

QString Generator::plainCode(const QString& markedCode)
{
    QString t = markedCode;
    t.replace(tag, QString());
    t.replace(quot, QLatin1String("\""));
    t.replace(gt, QLatin1String(">"));
    t.replace(lt, QLatin1String("<"));
    t.replace(amp, QLatin1String("&"));
    return t;
}

QString Generator::typeString(const Node *node)
{
    switch (node->type()) {
    case Node::Namespace:
        return "namespace";
    case Node::Class:
        return "class";
    case Node::Fake:
    {
        switch (node->subType()) {
        case Node::QmlClass:
            return "element";
        case Node::QmlPropertyGroup:
            return "property group";
        case Node::QmlBasicType:
            return "type";
        default:
            return "documentation";
        }
    }
    case Node::Enum:
        return "enum";
    case Node::Typedef:
        return "typedef";
    case Node::Function:
        return "function";
    case Node::Property:
        return "property";
    default:
        return "documentation";
    }
}

QString Generator::imageFileName(const Node *relative, const QString& fileBase)
{
    QString userFriendlyFilePath;
    QString filePath = Config::findFile(
        relative->doc().location(), imageFiles, imageDirs, fileBase,
        imgFileExts[format()], userFriendlyFilePath);

    if (filePath.isEmpty())
        return QString();

    QString path = Config::copyFile(relative->doc().location(),
                                    filePath,
                                    userFriendlyFilePath,
                                    outputDir() + QLatin1String("/images"));
    if (path[0] != '/')
        return QLatin1String("images/") + path;
    return QLatin1String("images") + path;
}

void Generator::setImageFileExtensions(const QStringList& extensions)
{
    imgFileExts[format()] = extensions;
}

void Generator::unknownAtom(const Atom *atom)
{
    Location::internalError(tr("unknown atom type '%1' in %2 generator")
                            .arg(atom->typeString()).arg(format()));
}

bool Generator::matchAhead(const Atom *atom, Atom::Type expectedAtomType)
{
    return atom->next() != 0 && atom->next()->type() == expectedAtomType;
}

void Generator::supplementAlsoList(const Node *node, QList<Text> &alsoList)
{
    if (node->type() == Node::Function) {
        const FunctionNode *func = static_cast<const FunctionNode *>(node);
        if (func->overloadNumber() == 1) {
            QString alternateName;
            const FunctionNode *alternateFunc = 0;

            if (func->name().startsWith("set") && func->name().size() >= 4) {
                alternateName = func->name()[3].toLower();
                alternateName += func->name().mid(4);
                alternateFunc = func->parent()->findFunctionNode(alternateName);

                if (!alternateFunc) {
                    alternateName = "is" + func->name().mid(3);
                    alternateFunc = func->parent()->findFunctionNode(alternateName);
                    if (!alternateFunc) {
                        alternateName = "has" + func->name().mid(3);
                        alternateFunc = func->parent()->findFunctionNode(alternateName);
                    }
                }
            }
            else if (!func->name().isEmpty()) {
                alternateName = "set";
                alternateName += func->name()[0].toUpper();
                alternateName += func->name().mid(1);
                alternateFunc = func->parent()->findFunctionNode(alternateName);
            }

            if (alternateFunc && alternateFunc->access() != Node::Private) {
                int i;
                for (i = 0; i < alsoList.size(); ++i) {
                    if (alsoList.at(i).toString().contains(alternateName))
                        break;
                }

                if (i == alsoList.size()) {
                    alternateName += "()";

                    Text also;
                    also << Atom(Atom::Link, alternateName)
                         << Atom(Atom::FormattingLeft, ATOM_FORMATTING_LINK)
                         << alternateName
                         << Atom(Atom::FormattingRight, ATOM_FORMATTING_LINK);
                    alsoList.prepend(also);
                }
            }
        }
    }
}

QMap<QString, QString>& Generator::formattingLeftMap()
{
    return fmtLeftMaps[format()];
}

QMap<QString, QString>& Generator::formattingRightMap()
{
    return fmtRightMaps[format()];
}

/*
  Trims trailimng whitespace off the \a string and returns
  the trimmed string.
 */
QString Generator::trimmedTrailing(const QString& string)
{
    QString trimmed = string;
    while (trimmed.length() > 0 && trimmed[trimmed.length() - 1].isSpace())
        trimmed.truncate(trimmed.length() - 1);
    return trimmed;
}

void Generator::generateStatus(const Node *node, CodeMarker *marker)
{
    Text text;

    switch (node->status()) {
    case Node::Commendable:
    case Node::Main:
        break;
    case Node::Preliminary:
        text << Atom::ParaLeft
             << Atom(Atom::FormattingLeft, ATOM_FORMATTING_BOLD)
             << "This "
             << typeString(node)
             << " is under development and is subject to change."
             << Atom(Atom::FormattingRight, ATOM_FORMATTING_BOLD)
             << Atom::ParaRight;
        break;
    case Node::Deprecated:
        text << Atom::ParaLeft;
        if (node->isInnerNode())
            text << Atom(Atom::FormattingLeft, ATOM_FORMATTING_BOLD);
        text << "This " << typeString(node) << " is deprecated.";
        if (node->isInnerNode())
            text << Atom(Atom::FormattingRight, ATOM_FORMATTING_BOLD);
        text << Atom::ParaRight;
        break;
    case Node::Obsolete:
        text << Atom::ParaLeft;
        if (node->isInnerNode())
            text << Atom(Atom::FormattingLeft, ATOM_FORMATTING_BOLD);
        text << "This " << typeString(node) << " is obsolete.";
        if (node->isInnerNode())
            text << Atom(Atom::FormattingRight, ATOM_FORMATTING_BOLD);
        text << " It is provided to keep old source code working. "
             << "We strongly advise against "
             << "using it in new code." << Atom::ParaRight;
        break;
    case Node::Compat:
        // reimplemented in HtmlGenerator subclass
        if (node->isInnerNode()) {
            text << Atom::ParaLeft
                 << Atom(Atom::FormattingLeft, ATOM_FORMATTING_BOLD)
                 << "This "
                 << typeString(node)
                 << " is part of the Qt 3 compatibility layer."
                 << Atom(Atom::FormattingRight, ATOM_FORMATTING_BOLD)
                 << " It is provided to keep old source code working. "
                 << "We strongly advise against "
                 << "using it in new code. See "
                 << Atom(Atom::AutoLink, "Porting to Qt 4")
                 << " for more information."
                 << Atom::ParaRight;
        }
        break;
    case Node::Internal:
    default:
        break;
    }
    generateText(text, node, marker);
}

void Generator::generateThreadSafeness(const Node *node, CodeMarker *marker)
{
    Text text;
    Text theStockLink;
    Node::ThreadSafeness threadSafeness = node->threadSafeness();

    Text rlink;
    rlink << Atom(Atom::Link,"reentrant")
          << Atom(Atom::FormattingLeft, ATOM_FORMATTING_LINK)
          << "reentrant"
          << Atom(Atom::FormattingRight, ATOM_FORMATTING_LINK);

    Text tlink;
    tlink << Atom(Atom::Link,"thread-safe")
          << Atom(Atom::FormattingLeft, ATOM_FORMATTING_LINK)
          << "thread-safe"
          << Atom(Atom::FormattingRight, ATOM_FORMATTING_LINK);

    switch (threadSafeness) {
    case Node::UnspecifiedSafeness:
        break;
    case Node::NonReentrant:
        text << Atom::ParaLeft
             << Atom(Atom::FormattingLeft,ATOM_FORMATTING_BOLD)
             << "Warning:"
             << Atom(Atom::FormattingRight,ATOM_FORMATTING_BOLD)
             << " This "
             << typeString(node)
             << " is not "
             << rlink
             << "."
             << Atom::ParaRight;
        break;
    case Node::Reentrant:
    case Node::ThreadSafe:
        text << Atom::ParaLeft
             << Atom(Atom::FormattingLeft,ATOM_FORMATTING_BOLD)
             << "Note:"
             << Atom(Atom::FormattingRight,ATOM_FORMATTING_BOLD)
             << " ";

        if (node->isInnerNode()) {
            const InnerNode* innerNode = static_cast<const InnerNode*>(node);
            text << "All functions in this "
                 << typeString(node)
                 << " are ";
            if (threadSafeness == Node::ThreadSafe)
                text << tlink;
            else
                text << rlink;

            bool exceptions = false;
            NodeList reentrant;
            NodeList threadsafe;
            NodeList nonreentrant;
            NodeList::ConstIterator c = innerNode->childNodes().begin();
            while (c != innerNode->childNodes().end()) {
                
                if ((*c)->status() != Node::Obsolete){
                    switch ((*c)->threadSafeness()) {
                    case Node::Reentrant:
                        reentrant.append(*c);
                        if (threadSafeness == Node::ThreadSafe)
                            exceptions = true;
                        break;
                    case Node::ThreadSafe:
                        threadsafe.append(*c);
                        if (threadSafeness == Node::Reentrant)
                            exceptions = true;
                        break;
                    case Node::NonReentrant:
                        nonreentrant.append(*c);
                        exceptions = true;
                        break;
                    default:
                        break;
                    }
                }
                ++c;
            }
            if (!exceptions)
                text << ".";
            else if (threadSafeness == Node::Reentrant) {
                if (nonreentrant.isEmpty()) {
                    if (!threadsafe.isEmpty()) {
                        text << ", but ";
                        appendFullNames(text,threadsafe,innerNode,marker);
                        singularPlural(text,threadsafe);
                        text << " also " << tlink << ".";
                    }
                    else
                        text << ".";
                }
                else {
                    text << ", except for ";
                    appendFullNames(text,nonreentrant,innerNode,marker);
                    text << ", which";
                    singularPlural(text,nonreentrant);
                    text << " nonreentrant.";
                    if (!threadsafe.isEmpty()) {
                        text << " ";
                        appendFullNames(text,threadsafe,innerNode,marker);
                        singularPlural(text,threadsafe);
                        text << " " << tlink << ".";
                    }
                }
            }
            else { // thread-safe
                if (!nonreentrant.isEmpty() || !reentrant.isEmpty()) {
                    text << ", except for ";
                    if (!reentrant.isEmpty()) {
                        appendFullNames(text,reentrant,innerNode,marker);
                        text << ", which";
                        singularPlural(text,reentrant);
                        text << " only " << rlink;
                        if (!nonreentrant.isEmpty())
                            text << ", and ";
                    }
                    if (!nonreentrant.isEmpty()) {
                        appendFullNames(text,nonreentrant,innerNode,marker);
                        text << ", which";
                        singularPlural(text,nonreentrant);
                        text << " nonreentrant.";
                    }
                    text << ".";
                }
            }
        }
        else {
            text << "This " << typeString(node) << " is ";
            if (threadSafeness == Node::ThreadSafe)
                text << tlink;
            else
                text << rlink;
            text << ".";
        }
        text << Atom::ParaRight;
    }
    generateText(text,node,marker);
}

void Generator::generateSince(const Node *node, CodeMarker *marker)
{
    if (!node->since().isEmpty()) {
        Text text;
        text << Atom::ParaLeft
             << "This "
             << typeString(node);
        if (node->type() == Node::Enum)
            text << " was introduced or modified in ";
        else
            text << " was introduced in ";

        QStringList since = node->since().split(" ");
        if (since.count() == 1) {
            // Handle legacy use of \since <version>.
            if (project.isEmpty())
                 text << "version";
            else
                 text << project;
            text << " " << since[0];
        } else {
            // Reconstruct the <project> <version> string.
            text << " " << since.join(" ");
        }

        text << "." << Atom::ParaRight;
        generateText(text, node, marker);
    }
}

void Generator::generateReimplementedFrom(const FunctionNode *func,
                                          CodeMarker *marker)
{
    if (func->reimplementedFrom() != 0) {
        const FunctionNode *from = func->reimplementedFrom();
        if (from->access() != Node::Private &&
            from->parent()->access() != Node::Private) {
            Text text;
            text << Atom::ParaLeft << "Reimplemented from ";
            QString fullName =  from->parent()->name() + "::" + from->name() + "()";
            appendFullName(text, from->parent(), fullName, from);
            text << "." << Atom::ParaRight;
            generateText(text, func, marker);
        }
    }
}

const Atom *Generator::generateAtomList(const Atom *atom,
                                        const Node *relative,
                                        CodeMarker *marker,
                                        bool generate,
                                        int &numAtoms)
{
    while (atom) {
        if (atom->type() == Atom::FormatIf) {
            int numAtoms0 = numAtoms;
            bool rightFormat = canHandleFormat(atom->string());
            atom = generateAtomList(atom->next(),
                                    relative,
                                    marker,
                                    generate && rightFormat,
                                    numAtoms);
            if (!atom)
                return 0;

            if (atom->type() == Atom::FormatElse) {
                ++numAtoms;
                atom = generateAtomList(atom->next(),
                                        relative,
                                        marker,
                                        generate && !rightFormat,
                                        numAtoms);
                if (!atom)
                    return 0;
            }

            if (atom->type() == Atom::FormatEndif) {
                if (generate && numAtoms0 == numAtoms) {
                    relative->location().warning(tr("Output format %1 not handled %2")
                                                 .arg(format()).arg(outFileName()));
                    Atom unhandledFormatAtom(Atom::UnhandledFormat, format());
                    generateAtomList(&unhandledFormatAtom,
                                     relative,
                                     marker,
                                     generate,
                                     numAtoms);
                }
                atom = atom->next();
            }
        }
        else if (atom->type() == Atom::FormatElse ||
                 atom->type() == Atom::FormatEndif) {
            return atom;
        }
        else {
            int n = 1;
            if (generate) {
                n += generateAtom(atom, relative, marker);
                numAtoms += n;
            }
            while (n-- > 0)
                atom = atom->next();
        }
    }
    return 0;
}

void Generator::appendFullName(Text& text,
                               const Node *apparentNode,
                               const Node *relative,
                               CodeMarker *marker,
                               const Node *actualNode)
{
    if (actualNode == 0)
        actualNode = apparentNode;
    text << Atom(Atom::LinkNode, CodeMarker::stringForNode(actualNode))
         << Atom(Atom::FormattingLeft, ATOM_FORMATTING_LINK)
         << Atom(Atom::String, marker->plainFullName(apparentNode, relative))
         << Atom(Atom::FormattingRight, ATOM_FORMATTING_LINK);
}

void Generator::appendFullName(Text& text,
                               const Node *apparentNode,
                               const QString& fullName,
                               const Node *actualNode)
{
    if (actualNode == 0)
        actualNode = apparentNode;
    text << Atom(Atom::LinkNode, CodeMarker::stringForNode(actualNode))
         << Atom(Atom::FormattingLeft, ATOM_FORMATTING_LINK)
         << Atom(Atom::String, fullName)
         << Atom(Atom::FormattingRight, ATOM_FORMATTING_LINK);
}

void Generator::appendFullNames(Text& text,
                                const NodeList& nodes,
                                const Node* relative,
                                CodeMarker* marker)
{
    NodeList::ConstIterator n = nodes.begin();
    int index = 0;
    while (n != nodes.end()) {
        appendFullName(text,*n,relative,marker);
        text << comma(index++,nodes.count());
        ++n;
    }
}

void Generator::appendSortedNames(Text& text,
                                  const ClassNode *classe,
                                  const QList<RelatedClass> &classes,
                                  CodeMarker *marker)
{
    QList<RelatedClass>::ConstIterator r;
    QMap<QString,Text> classMap;
    int index = 0;

    r = classes.begin();
    while (r != classes.end()) {
        if ((*r).node->access() == Node::Public &&
            (*r).node->status() != Node::Internal
            && !(*r).node->doc().isEmpty()) {
            Text className;
            appendFullName(className, (*r).node, classe, marker);
            classMap[className.toString().toLower()] = className;
        }
        ++r;
    }

    QStringList classNames = classMap.keys();
    classNames.sort();

    foreach (const QString &className, classNames) {
        text << classMap[className];
        text << separator(index++, classNames.count());
    }
}

void Generator::appendSortedQmlNames(Text& text,
                                     const Node* base,
                                     const NodeList& subs,
                                     CodeMarker *marker)
{
    QMap<QString,Text> classMap;
    int index = 0;

#ifdef DEBUG_MULTIPLE_QDOCCONF_FILES
    qDebug() << "Generator::appendSortedQmlNames():" << base->name() << "is inherited by...";
#endif
    for (int i = 0; i < subs.size(); ++i) {
        Text t;
#ifdef DEBUG_MULTIPLE_QDOCCONF_FILES
        qDebug() << "    " << subs[i]->name();
#endif
        appendFullName(t, subs[i], base, marker);
        classMap[t.toString().toLower()] = t;
    }

    QStringList names = classMap.keys();
    names.sort();

    foreach (const QString &name, names) {
        text << classMap[name];
        text << separator(index++, names.count());
    }
}

int Generator::skipAtoms(const Atom *atom, Atom::Type type) const
{
    int skipAhead = 0;
    atom = atom->next();
    while (atom != 0 && atom->type() != type) {
        skipAhead++;
        atom = atom->next();
    }
    return skipAhead;
}

QString Generator::fullName(const Node *node,
                            const Node *relative,
                            CodeMarker *marker) const
{
    if (node->type() == Node::Fake)
        return static_cast<const FakeNode *>(node)->title();
    else if (node->type() == Node::Class &&
        !(static_cast<const ClassNode *>(node))->serviceName().isEmpty())
        return (static_cast<const ClassNode *>(node))->serviceName();
    else
        return marker->plainFullName(node, relative);
}

QString Generator::outputPrefix(const QString &nodeType)
{
    return outputPrefixes[nodeType];
}

/*!
  Looks up the tag \a t in the map of metadata values for the
  current topic in \a inner. If a value for the tag is found,
  the value is returned.

  \note If \a t is found in the metadata map, it is erased.
  i.e. Once you call this function for a particular \a t,
  you consume \a t.
 */
QString Generator::getMetadataElement(const InnerNode* inner, const QString& t)
{
    QString s;
    QStringMultiMap& metaTagMap = const_cast<QStringMultiMap&>(inner->doc().metaTagMap());
    QStringMultiMap::iterator i = metaTagMap.find(t);
    if (i != metaTagMap.end()) {
        s = i.value();
        metaTagMap.erase(i);
    }
    return s;
}

/*!
  Looks up the tag \a t in the map of metadata values for the
  current topic in \a inner. If values for the tag are found,
  they are returned in a string list.

  \note If \a t is found in the metadata map, all the pairs
  having the key \a t are erased. i.e. Once you call this
  function for a particular \a t, you consume \a t.
 */
QStringList Generator::getMetadataElements(const InnerNode* inner, const QString& t)
{
    QStringList s;
    QStringMultiMap& metaTagMap = const_cast<QStringMultiMap&>(inner->doc().metaTagMap());
    s = metaTagMap.values(t);
    if (!s.isEmpty())
        metaTagMap.remove(t);
    return s;
}

/*!
  For generating the "New Classes... in 4.6" section on the
  What's New in 4.6" page.
 */
void Generator::findAllSince(const InnerNode *node)
{
    NodeList::const_iterator child = node->childNodes().constBegin();

    // Traverse the tree, starting at the node supplied.

    while (child != node->childNodes().constEnd()) {

        QString sinceString = (*child)->since();

        if (((*child)->access() != Node::Private) && !sinceString.isEmpty()) {

            // Insert a new entry into each map for each new since string found.
            NewSinceMaps::iterator nsmap = newSinceMaps.find(sinceString);
            if (nsmap == newSinceMaps.end())
                nsmap = newSinceMaps.insert(sinceString,NodeMultiMap());

            NewClassMaps::iterator ncmap = newClassMaps.find(sinceString);
            if (ncmap == newClassMaps.end())
                ncmap = newClassMaps.insert(sinceString,NodeMap());

            NewClassMaps::iterator nqcmap = newQmlClassMaps.find(sinceString);
            if (nqcmap == newQmlClassMaps.end())
                nqcmap = newQmlClassMaps.insert(sinceString,NodeMap());

            if ((*child)->type() == Node::Function) {
                // Insert functions into the general since map.
                FunctionNode *func = static_cast<FunctionNode *>(*child);
                if ((func->status() > Node::Obsolete) &&
                    (func->metaness() != FunctionNode::Ctor) &&
                    (func->metaness() != FunctionNode::Dtor)) {
                    nsmap.value().insert(func->name(),(*child));
                }
            }
            else if ((*child)->url().isEmpty()) {
                if ((*child)->type() == Node::Class && !(*child)->doc().isEmpty()) {
                    // Insert classes into the since and class maps.
                    QString className = (*child)->name();
                    if ((*child)->parent() &&
                        (*child)->parent()->type() == Node::Namespace &&
                        !(*child)->parent()->name().isEmpty())
                        className = (*child)->parent()->name()+"::"+className;

                    nsmap.value().insert(className,(*child));
                    ncmap.value().insert(className,(*child));
                }
                else if ((*child)->subType() == Node::QmlClass) {
                    // Insert QML elements into the since and element maps.
                    QString className = (*child)->name();
                    if ((*child)->parent() &&
                        (*child)->parent()->type() == Node::Namespace &&
                        !(*child)->parent()->name().isEmpty())
                        className = (*child)->parent()->name()+"::"+className;

                    nsmap.value().insert(className,(*child));
                    nqcmap.value().insert(className,(*child));
                }
                else if ((*child)->type() == Node::QmlProperty) {
                    // Insert QML properties into the since map.
                    QString propertyName = (*child)->name();
                    nsmap.value().insert(propertyName,(*child));
                }
            }
            else {
                // Insert external documents into the general since map.
                QString name = (*child)->name();
                if ((*child)->parent() &&
                    (*child)->parent()->type() == Node::Namespace &&
                    !(*child)->parent()->name().isEmpty())
                    name = (*child)->parent()->name()+"::"+name;

                nsmap.value().insert(name,(*child));
            }

            // Find child nodes with since commands.
            if ((*child)->isInnerNode()) {
                findAllSince(static_cast<InnerNode *>(*child));
            }
        }
        ++child;
    }
}

QT_END_NAMESPACE
