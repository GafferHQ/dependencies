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
  htmlgenerator.h
*/

#ifndef HTMLGENERATOR_H
#define HTMLGENERATOR_H

#include <qmap.h>
#include <qregexp.h>
#include <QXmlStreamWriter>

#include "codemarker.h"
#include "config.h"
#include "pagegenerator.h"

QT_BEGIN_NAMESPACE

class HelpProjectWriter;

class HtmlGenerator : public PageGenerator
{
 public:
    enum SinceType { 
        Namespace, 
        Class, 
        MemberFunction,
        NamespaceFunction,
        GlobalFunction,
        Macro,
        Enum, 
        Typedef, 
        Property,
        Variable, 
        QmlClass,
        QmlProperty,
        QmlSignal,
        QmlMethod,
        LastSinceType
    };

 public:
    HtmlGenerator();
    ~HtmlGenerator();

    virtual void initializeGenerator(const Config& config);
    virtual void terminateGenerator();
    virtual QString format();
    virtual void generateTree(const Tree *tree);
    void generateManifestFiles();

    QString protectEnc(const QString &string);
    static QString protect(const QString &string, const QString &encoding = "ISO-8859-1");
    static QString cleanRef(const QString& ref);
    static QString sinceTitle(int i) { return sinceTitles[i]; }
    static QString fullDocumentLocation(const Node *node);

 protected:
    virtual void startText(const Node *relative, CodeMarker *marker);
    virtual int generateAtom(const Atom *atom, 
                             const Node *relative,
                             CodeMarker *marker);
    virtual void generateClassLikeNode(const InnerNode *inner, CodeMarker *marker);
    virtual void generateFakeNode(const FakeNode *fake, CodeMarker *marker);
    virtual QString fileExtension(const Node *node) const;
    virtual QString refForNode(const Node *node);
    virtual QString linkForNode(const Node *node, const Node *relative);
    virtual QString refForAtom(Atom *atom, const Node *node);

    void generateManifestFile(QString manifest, QString element);

 private:
    enum SubTitleSize { SmallSubTitle, LargeSubTitle };
    enum ExtractionMarkType {
        BriefMark,
        DetailedDescriptionMark,
        MemberMark,
        EndMark
    };

    const QPair<QString,QString> anchorForNode(const Node *node);
    const Node *findNodeForTarget(const QString &target, 
                                  const Node *relative,
                                  CodeMarker *marker, 
                                  const Atom *atom = 0);
    void generateBreadCrumbs(const QString& title,
                             const Node *node,
                             CodeMarker *marker);
    void generateHeader(const QString& title, 
                        const Node *node = 0,
                        CodeMarker *marker = 0);
    void generateTitle(const QString& title, 
                       const Text &subTitle, 
                       SubTitleSize subTitleSize,
                       const Node *relative, 
                       CodeMarker *marker);
    void generateFooter(const Node *node = 0);
    void generateBrief(const Node *node, 
                       CodeMarker *marker,
                       const Node *relative = 0);
    void generateIncludes(const InnerNode *inner, CodeMarker *marker);
    void generateTableOfContents(const Node *node, 
                                 CodeMarker *marker, 
                                 QList<Section>* sections = 0);
    QString generateListOfAllMemberFile(const InnerNode *inner, 
                                        CodeMarker *marker);
    QString generateAllQmlMembersFile(const QmlClassNode* qml_cn, 
                                      CodeMarker* marker);
    QString generateLowStatusMemberFile(const InnerNode *inner, 
                                        CodeMarker *marker,
                                        CodeMarker::Status status);
    void generateClassHierarchy(const Node *relative, 
                                CodeMarker *marker,
				const NodeMap &classMap);
    void generateAnnotatedList(const Node *relative, 
                               CodeMarker *marker,
			       const NodeMap &nodeMap);
    void generateCompactList(const Node *relative, 
                             CodeMarker *marker,
			     const NodeMap &classMap,
                             bool includeAlphabet,
                             QString commonPrefix = QString());
    void generateFunctionIndex(const Node *relative, CodeMarker *marker);
    void generateLegaleseList(const Node *relative, CodeMarker *marker);
    void generateOverviewList(const Node *relative, CodeMarker *marker);
    void generateSectionList(const Section& section, 
                             const Node *relative,
			     CodeMarker *marker, 
                             CodeMarker::SynopsisStyle style);
#ifdef QDOC_QML
    void generateQmlSummary(const Section& section,
                            const Node *relative,
                            CodeMarker *marker);
    void generateQmlItem(const Node *node,
                         const Node *relative,
                         CodeMarker *marker,
                         bool summary);
    void generateDetailedQmlMember(const Node *node,
                                   const InnerNode *relative,
                                   CodeMarker *marker);
    void generateQmlInherits(const QmlClassNode* cn, CodeMarker* marker);
    void generateQmlInheritedBy(const QmlClassNode* cn, CodeMarker* marker);
    void generateQmlInstantiates(const QmlClassNode* qcn, CodeMarker* marker);
    void generateInstantiatedBy(const ClassNode* cn, CodeMarker* marker);
#endif

    void generateSection(const NodeList& nl,
                         const Node *relative,
                         CodeMarker *marker,
                         CodeMarker::SynopsisStyle style);
    void generateSynopsis(const Node *node, 
                          const Node *relative, 
                          CodeMarker *marker,
			  CodeMarker::SynopsisStyle style,
                          bool alignNames = false);
    void generateSectionInheritedList(const Section& section, 
                                      const Node *relative,
                                      CodeMarker *marker);
    QString highlightedCode(const QString& markedCode, 
                            CodeMarker* marker, 
                            const Node* relative,
                            bool alignNames = false,
                            const Node* self = 0);

    void generateFullName(const Node *apparentNode, 
                          const Node *relative, 
                          CodeMarker *marker,
			  const Node *actualNode = 0);
    void generateDetailedMember(const Node *node, 
                                const InnerNode *relative, 
                                CodeMarker *marker);
    void generateLink(const Atom *atom, 
                      const Node *relative, 
                      CodeMarker *marker);
    void generateStatus(const Node *node, CodeMarker *marker);
    
    QString registerRef(const QString& ref);
    virtual QString fileBase(const Node *node) const;
    QString fileName(const Node *node);
    void findAllClasses(const InnerNode *node);
    void findAllFunctions(const InnerNode *node);
    void findAllLegaleseTexts(const InnerNode *node);
    void findAllNamespaces(const InnerNode *node);
    static int hOffset(const Node *node);
    static bool isThreeColumnEnumValueTable(const Atom *atom);
    virtual QString getLink(const Atom *atom, 
                            const Node *relative, 
                            CodeMarker *marker, 
                            const Node** node);
    virtual void generateIndex(const QString &fileBase, 
                               const QString &url,
                               const QString &title);
#ifdef GENERATE_MAC_REFS    
    void generateMacRef(const Node *node, CodeMarker *marker);
#endif
    void beginLink(const QString &link, 
                   const Node *node, 
                   const Node *relative, 
                   CodeMarker *marker);
    void endLink();
    bool generatePageElement(QXmlStreamWriter& writer, 
                             const Node* node, 
                             CodeMarker* marker) const;
    void generatePageElements(QXmlStreamWriter& writer, 
                              const Node* node, 
                              CodeMarker* marker) const;
    void generatePageIndex(const QString& fileName) const;
    void generateExtractionMark(const Node *node, ExtractionMarkType markType);

    QMap<QString, QString> refMap;
    int codeIndent;
    HelpProjectWriter *helpProjectWriter;
    bool inLink;
    bool inObsoleteLink;
    bool inContents;
    bool inSectionHeading;
    bool inTableHeader;
    int numTableRows;
    bool threeColumnEnumValueTable;
    QString link;
    QStringList sectionNumber;
    QRegExp funcLeftParen;
    QString style;
    QString headerScripts;
    QString headerStyles;
    QString endHeader;
    QString postHeader;
    QString postPostHeader;
    QString footer;
    QString address;
    bool pleaseGenerateMacRef;
    bool noBreadCrumbs;
    QString project;
    QString projectDescription;
    QString projectUrl;
    QString navigationLinks;
    QString manifestDir;
    QStringList stylesheets;
    QStringList customHeadElements;
    const Tree *myTree;
    bool obsoleteLinks;
    QMap<QString, NodeMap > moduleClassMap;
    QMap<QString, NodeMap > moduleNamespaceMap;
    NodeMap nonCompatClasses;
    NodeMap mainClasses;
    NodeMap compatClasses;
    NodeMap obsoleteClasses;
    NodeMap namespaceIndex;
    NodeMap serviceClasses;
    NodeMap qmlClasses;
    QMap<QString, NodeMap > funcIndex;
    QMap<Text, const Node *> legaleseTexts;
    static int id;
 public:
    static bool debugging_on;
    static QString divNavTop;
};

#define HTMLGENERATOR_ADDRESS           "address"
#define HTMLGENERATOR_FOOTER            "footer"
#define HTMLGENERATOR_GENERATEMACREFS   "generatemacrefs" // ### document me
#define HTMLGENERATOR_POSTHEADER        "postheader"
#define HTMLGENERATOR_POSTPOSTHEADER    "postpostheader"
#define HTMLGENERATOR_NOBREADCRUMBS     "nobreadcrumbs"

QT_END_NAMESPACE

#endif

