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
  pagegenerator.cpp
*/

#include <qfile.h>
#include <qfileinfo.h>
#include <qdebug.h>
#include "codemarker.h"
#include "pagegenerator.h"
#include "tree.h"

QT_BEGIN_NAMESPACE

/*!
  Nothing to do in the constructor.
 */
PageGenerator::PageGenerator()
    : outputCodec(0)
{
    // nothing.
}

/*!
  The destructor 
 */
PageGenerator::~PageGenerator()
{
    while (!outStreamStack.isEmpty())
	endSubPage();
}

bool PageGenerator::parseArg(const QString& src,
                             const QString& tag,
                             int* pos,
                             int n,
                             QStringRef* contents,
                             QStringRef* par1,
                             bool debug)
{
#define SKIP_CHAR(c) \
    if (debug) \
        qDebug() << "looking for " << c << " at " << QString(src.data() + i, n - i); \
    if (i >= n || src[i] != c) { \
        if (debug) \
            qDebug() << " char '" << c << "' not found"; \
        return false; \
    } \
    ++i;


#define SKIP_SPACE \
    while (i < n && src[i] == ' ') \
        ++i;

    int i = *pos;
    int j = i;

    // assume "<@" has been parsed outside
    //SKIP_CHAR('<');
    //SKIP_CHAR('@');

    if (tag != QStringRef(&src, i, tag.length())) {
        if (0 && debug)
            qDebug() << "tag " << tag << " not found at " << i;
        return false;
    }

    if (debug)
        qDebug() << "haystack:" << src << "needle:" << tag << "i:" <<i;

    // skip tag
    i += tag.length();

    // parse stuff like:  linkTag("(<@link node=\"([^\"]+)\">).*(</@link>)");
    if (par1) {
        SKIP_SPACE;
        // read parameter name
        j = i;
        while (i < n && src[i].isLetter())
            ++i;
        if (src[i] == '=') {
            if (debug)
                qDebug() << "read parameter" << QString(src.data() + j, i - j);
            SKIP_CHAR('=');
            SKIP_CHAR('"');
            // skip parameter name
            j = i;
            while (i < n && src[i] != '"')
                ++i;
            *par1 = QStringRef(&src, j, i - j);
            SKIP_CHAR('"');
            SKIP_SPACE;
        } else {
            if (debug)
                qDebug() << "no optional parameter found";
        }
    }
    SKIP_SPACE;
    SKIP_CHAR('>');

    // find contents up to closing "</@tag>
    j = i;
    for (; true; ++i) {
        if (i + 4 + tag.length() > n)
            return false;
        if (src[i] != '<')
            continue;
        if (src[i + 1] != '/')
            continue;
        if (src[i + 2] != '@')
            continue;
        if (tag != QStringRef(&src, i + 3, tag.length()))
            continue;
        if (src[i + 3 + tag.length()] != '>')
            continue;
        break;
    }

    *contents = QStringRef(&src, j, i - j);

    i += tag.length() + 4;

    *pos = i;
    if (debug)
        qDebug() << " tag " << tag << " found: pos now: " << i;
    return true;
#undef SKIP_CHAR
}

/*!
  This function is recursive.
 */
void PageGenerator::generateTree(const Tree *tree)
{
    generateInnerNode(tree->root());
}

QString PageGenerator::fileBase(const Node *node) const
{
    if (node->relates())
	node = node->relates();
    else if (!node->isInnerNode())
	node = node->parent();
#ifdef QDOC_QML
    if (node->subType() == Node::QmlPropertyGroup) {
        node = node->parent();
    }
#endif        

    QString base = node->doc().baseName();
    if (!base.isEmpty())
        return base;

    const Node *p = node;

    forever {
        const Node *pp = p->parent();
        base.prepend(p->name());
#ifdef QDOC_QML
        /*
          To avoid file name conflicts in the html directory,
          we prepend a prefix (by default, "qml-") to the file name of QML
          element doc files.
         */
        if ((p->subType() == Node::QmlClass) ||
            (p->subType() == Node::QmlBasicType)) {
            if (!base.startsWith(QLatin1String("QML:")))
                base.prepend(outputPrefix(QLatin1String("QML")));
        }
#endif
        if (!pp || pp->name().isEmpty() || pp->type() == Node::Fake)
            break;
        base.prepend(QLatin1Char('-'));
        p = pp;
    }
    
    if (node->type() == Node::Fake) {
#ifdef QDOC2_COMPAT
        if (base.endsWith(".html"))
            base.truncate(base.length() - 5);
#endif
    }

    // the code below is effectively equivalent to:
    //   base.replace(QRegExp("[^A-Za-z0-9]+"), " ");
    //   base = base.trimmed();
    //   base.replace(" ", "-");
    //   base = base.toLower();
    // as this function accounted for ~8% of total running time
    // we optimize a bit...

    QString res;
    // +5 prevents realloc in fileName() below
    res.reserve(base.size() + 5);
    bool begun = false;
    for (int i = 0; i != base.size(); ++i) {
        QChar c = base.at(i);
        uint u = c.unicode();
        if (u >= 'A' && u <= 'Z')
            u -= 'A' - 'a';
        if ((u >= 'a' &&  u <= 'z') || (u >= '0' && u <= '9')) {
            res += QLatin1Char(u);
            begun = true;
        }
        else if (begun) {
            res += QLatin1Char('-');
            begun = false;
        }
    }
    while (res.endsWith(QLatin1Char('-')))
        res.chop(1);
    return res;
}

/*!
  If the \a node has a URL, return the URL as the file name.
  Otherwise, construct the file name from the fileBase() and
  the fileExtension(), and return the constructed name.
 */
QString PageGenerator::fileName(const Node* node) const
{
    if (!node->url().isEmpty())
        return node->url();

    QString name = fileBase(node);
    name += QLatin1Char('.');
    name += fileExtension(node);
    return name;
}

/*!
  Return the current output file name.
 */
QString PageGenerator::outFileName()
{
    return QFileInfo(static_cast<QFile*>(out().device())->fileName()).fileName();
}

/*!
  Creates the file named \a fileName in the output directory.
  Attaches a QTextStream to the created file, which is written
  to all over the place using out().
 */
void PageGenerator::beginSubPage(const Location& location,
                                 const QString& fileName)
{
    QFile* outFile = new QFile(outputDir() + "/" + fileName);
    if (!outFile->open(QFile::WriteOnly))
	location.fatal(tr("Cannot open output file '%1'").arg(outFile->fileName()));
    QTextStream* out = new QTextStream(outFile);

    if (outputCodec)
        out->setCodec(outputCodec);
    outStreamStack.push(out);
}

/*!
  Flush the text stream associated with the subpage, and
  then pop it off the text stream stack and delete it.
  This terminates output of the subpage.
 */
void PageGenerator::endSubPage()
{
    outStreamStack.top()->flush();
    delete outStreamStack.top()->device();
    delete outStreamStack.pop();
}

/*!
  Used for writing to the current output stream. Returns a
  reference to the crrent output stream, which is then used
  with the \c {<<} operator for writing.
 */
QTextStream &PageGenerator::out()
{
    return *outStreamStack.top();
}

/*!
  Recursive writing of HTML files from the root \a node.
 */
void
PageGenerator::generateInnerNode(const InnerNode* node)
{
    if (!node->url().isNull())
        return;

    if (node->type() == Node::Fake) {
        const FakeNode *fakeNode = static_cast<const FakeNode *>(node);
        if (fakeNode->subType() == Node::ExternalPage)
            return;
        if (fakeNode->subType() == Node::Image)
            return;
        if (fakeNode->subType() == Node::QmlPropertyGroup)
            return;
        if (fakeNode->subType() == Node::Page) {
            if (node->count() > 0)
                qDebug("PAGE %s HAS CHILDREN", qPrintable(fakeNode->title()));
        }
    }

    /*
      Obtain a code marker for the source file.
     */
    CodeMarker *marker = CodeMarker::markerForFileName(node->location().filePath());

    if (node->parent() != 0) {
	beginSubPage(node->location(), fileName(node));
	if (node->type() == Node::Namespace || node->type() == Node::Class) {
	    generateClassLikeNode(node, marker);
	}
        else if (node->type() == Node::Fake) {
	    generateFakeNode(static_cast<const FakeNode *>(node), marker);
	}
	endSubPage();
    }

    NodeList::ConstIterator c = node->childNodes().begin();
    while (c != node->childNodes().end()) {
	if ((*c)->isInnerNode() && (*c)->access() != Node::Private)
	    generateInnerNode((const InnerNode *) *c);
	++c;
    }
}

QT_END_NAMESPACE
