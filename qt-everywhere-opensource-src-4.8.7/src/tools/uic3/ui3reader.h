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

#ifndef UI3READER_H
#define UI3READER_H

#include <QDomDocument>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QTextStream>
#include <QVariant>
#include <QByteArray>
#include <QPair>

QT_BEGIN_NAMESPACE

class DomUI;
class DomFont;
class DomWidget;
class DomProperty;
class DomLayout;
class DomLayoutItem;
class DomActionGroup;
class Porting;
struct Color;

typedef QList<QPair<int, Color> > ColorGroup;

class Ui3Reader
{
public:
    enum Options { CustomWidgetForwardDeclarations = 0x1, ImplicitIncludes = 0x2,
                   PreserveLayoutNames = 0x4, LimitXPM_LineLength = 0x8 };

    explicit Ui3Reader(QTextStream &stream, unsigned options);
    ~Ui3Reader();

    void computeDeps(const QDomElement &e, QStringList &globalIncludes, QStringList &localIncludes, bool impl = false);
    void generateUi4(const QString &fn, const QString &outputFn, QDomDocument doc);

    void generate(const QString &fn, const QString &outputFn,
         QDomDocument doc, bool decl, bool subcl, const QString &trm,
         const QString& subclname, const QString &convertedUiFile);

    void embed(const char *project, const QStringList &images);

    void setTrMacro(const QString &trmacro);
    void setOutputFileName(const QString &fileName);

    void createFormDecl(const QDomElement &e);
    void createFormImpl(const QDomElement &e);

    void createWrapperDecl(const QDomElement &e, const QString &convertedUiFile);

    void createSubDecl(const QDomElement &e, const QString& subclname);
    void createSubImpl(const QDomElement &e, const QString& subclname);

    void createColorGroupImpl(const QString& cg, const QDomElement& e);
    ColorGroup loadColorGroup(const QDomElement &e);

    QDomElement getObjectProperty(const QDomElement& e, const QString& name);
    QString getPixmapLoaderFunction(const QDomElement& e);
    QString getFormClassName(const QDomElement& e);
    QString getClassName(const QDomElement& e);
    QString getObjectName(const QDomElement& e);
    QString getLayoutName(const QDomElement& e);

    QString registerObject(const QString& name);
    QString registeredName(const QString& name);
    bool isObjectRegistered(const QString& name);
    QStringList unique(const QStringList&);

    QString trcall(const QString& sourceText, const QString& comment = QString());

    QDomElement parse(const QDomDocument &doc);

    void setExtractImages(bool extract, const QString &qrcOutputFile);

private:
    void init();

    void createWrapperDeclContents(const QDomElement &e);

    void errorInvalidProperty(const QString &propertyName, const QString &widgetName, const QString &widgetClass,
                              int line, int col);
    void errorInvalidSignal(const QString &signal, const QString &widgetName, const QString &widgetClass,
                            int line, int col);
    void errorInvalidSlot(const QString &slot, const QString &widgetName, const QString &widgetClass,
                          int line, int col);

    DomUI *generateUi4(const QDomElement &e);
    DomWidget *createWidget(const QDomElement &w, const QString &widgetClass = QString());
    void createProperties(const QDomElement &e, QList<DomProperty*> *properties, const QString &className);
    void createAttributes(const QDomElement &e, QList<DomProperty*> *properties, const QString &className);
    DomLayout *createLayout(const QDomElement &e);
    DomLayoutItem *createLayoutItem(const QDomElement &e);
    DomProperty *readProperty(const QDomElement &e);
    void fixActionGroup(DomActionGroup *g);
    QString fixActionProperties(QList<DomProperty*> &properties, bool isActionGroup = false);

    QString fixHeaderName(const QString &headerName) const;
    QString fixClassName(const QString &className) const;
    QString fixDeclaration(const QString &declaration) const;
    QString fixType(const QString &type) const;
    QString fixMethod(const QString &method) const;

    QDomElement findDerivedFontProperties(const QDomElement &n) const;

    void fixLayoutMargin(DomLayout *ui_layout);

    const unsigned m_options;

    QTextStream &out;
    QTextOStream trout;
    QString languageChangeBody;
    QString outputFileName;
    QStringList objectNames;
    QMap<QString,QString> objectMapper;
    QString indent;
    QStringList tags;
    QStringList layouts;
    QString formName;
    QString lastItem;
    QString trmacro;

    struct Buddy
    {
        Buddy(const QString& k, const QString& b)
            : key(k), buddy(b) {}
        Buddy(){} // for valuelist
        QString key;
        QString buddy;
        bool operator==(const Buddy& other) const
            { return (key == other.key); }
    };
    struct CustomInclude
    {
        QString header;
        QString location;
        Q_DUMMY_COMPARISON_OPERATOR(CustomInclude)
    };
    QList<Buddy> buddies;

    QStringList layoutObjects;
    bool isLayout(const QString& name) const;

    uint item_used : 1;
    uint cg_used : 1;
    uint pal_used : 1;
    uint stdsetdef : 1;
    uint externPixmaps : 1;

    QString uiFileVersion;
    QString nameOfClass;
    QStringList namespaces;
    QString bareNameOfClass;
    QString pixmapLoaderFunction;

    void registerDatabases(const QDomElement& e);
    bool isWidgetInTable(const QDomElement& e, const QString& connection, const QString& table);
    bool isFrameworkCodeGenerated(const QDomElement& e);
    QString getDatabaseInfo(const QDomElement& e, const QString& tag);
    void createFormImpl(const QDomElement& e, const QString& form, const QString& connection, const QString& table);
    void writeFunctionsDecl(const QStringList &fuLst, const QStringList &typLst, const QStringList &specLst);
    void writeFunctionsSubDecl(const QStringList &fuLst, const QStringList &typLst, const QStringList &specLst);
    void writeFunctionsSubImpl(const QStringList &fuLst, const QStringList &typLst, const QStringList &specLst,
                                const QString &subClass, const QString &descr);
    QStringList dbConnections;
    QMap<QString, QStringList> dbCursors;
    QMap<QString, QStringList> dbForms;

    static bool isMainWindow;
    static QString mkBool(bool b);
    static QString mkBool(const QString& s);
    bool toBool(const QString& s);
    static QString fixString(const QString &str, bool encode = false);
    static bool onlyAscii;
    static QString mkStdSet(const QString& prop);
    static QString getComment(const QDomNode& n);
    QVariant defSpacing, defMargin;
    QString fileName;
    bool writeFunctImpl;

    QDomElement root;
    QDomElement widget;

    QMap<QString, bool> candidateCustomWidgets;
    Porting *m_porting;

    bool m_extractImages;
    QString m_qrcOutputFile;
    QMap<QString, QString> m_imageMap;
};

QT_END_NAMESPACE

#endif // UI3READER_H
