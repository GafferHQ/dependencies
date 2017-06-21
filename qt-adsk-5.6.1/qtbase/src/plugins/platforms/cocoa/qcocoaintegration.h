/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QPLATFORMINTEGRATION_COCOA_H
#define QPLATFORMINTEGRATION_COCOA_H

#include <Cocoa/Cocoa.h>

#include "qcocoacursor.h"
#include "qcocoawindow.h"
#include "qcocoanativeinterface.h"
#include "qcocoainputcontext.h"
#include "qcocoaaccessibility.h"
#include "qcocoaclipboard.h"
#include "qcocoadrag.h"
#include "qcocoaservices.h"
#include "qcocoakeymapper.h"

#include <QtCore/QScopedPointer>
#include <qpa/qplatformintegration.h>
#include <QtPlatformSupport/private/qcoretextfontdatabase_p.h>

QT_BEGIN_NAMESPACE

class QCocoaScreen : public QPlatformScreen
{
public:
    QCocoaScreen(int screenIndex);
    ~QCocoaScreen();

    // ----------------------------------------------------
    // Virtual methods overridden from QPlatformScreen
    QPixmap grabWindow(WId window, int x, int y, int width, int height) const Q_DECL_OVERRIDE;
    QRect geometry() const Q_DECL_OVERRIDE { return m_geometry; }
    QRect availableGeometry() const Q_DECL_OVERRIDE { return m_availableGeometry; }
    int depth() const Q_DECL_OVERRIDE { return m_depth; }
    QImage::Format format() const Q_DECL_OVERRIDE { return m_format; }
    qreal devicePixelRatio() const Q_DECL_OVERRIDE;
    QSizeF physicalSize() const Q_DECL_OVERRIDE { return m_physicalSize; }
    QDpi logicalDpi() const Q_DECL_OVERRIDE { return m_logicalDpi; }
    qreal refreshRate() const Q_DECL_OVERRIDE { return m_refreshRate; }
    QString name() const Q_DECL_OVERRIDE { return m_name; }
    QPlatformCursor *cursor() const Q_DECL_OVERRIDE { return m_cursor; }
    QWindow *topLevelAt(const QPoint &point) const Q_DECL_OVERRIDE;
    QList<QPlatformScreen *> virtualSiblings() const Q_DECL_OVERRIDE { return m_siblings; }
    QPlatformScreen::SubpixelAntialiasingType subpixelAntialiasingTypeHint() const Q_DECL_OVERRIDE;

    // ----------------------------------------------------
    // Additional methods
    void setVirtualSiblings(const QList<QPlatformScreen *> &siblings) { m_siblings = siblings; }
    NSScreen *osScreen() const;
    void updateGeometry();

public:
    int m_screenIndex;
    QRect m_geometry;
    QRect m_availableGeometry;
    QDpi m_logicalDpi;
    qreal m_refreshRate;
    int m_depth;
    QString m_name;
    QImage::Format m_format;
    QSizeF m_physicalSize;
    QCocoaCursor *m_cursor;
    QList<QPlatformScreen *> m_siblings;
};

class QCocoaIntegration : public QPlatformIntegration
{
public:
    enum Option {
        UseFreeTypeFontEngine = 0x1
    };
    Q_DECLARE_FLAGS(Options, Option)

    QCocoaIntegration(const QStringList &paramList);
    ~QCocoaIntegration();

    static QCocoaIntegration *instance();
    Options options() const;

    bool hasCapability(QPlatformIntegration::Capability cap) const Q_DECL_OVERRIDE;
    QPlatformWindow *createPlatformWindow(QWindow *window) const Q_DECL_OVERRIDE;
#ifndef QT_NO_OPENGL
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const Q_DECL_OVERRIDE;
#endif
    QPlatformBackingStore *createPlatformBackingStore(QWindow *widget) const Q_DECL_OVERRIDE;

    QAbstractEventDispatcher *createEventDispatcher() const Q_DECL_OVERRIDE;

    QCoreTextFontDatabase *fontDatabase() const Q_DECL_OVERRIDE;
    QCocoaNativeInterface *nativeInterface() const Q_DECL_OVERRIDE;
    QPlatformInputContext *inputContext() const Q_DECL_OVERRIDE;
#ifndef QT_NO_ACCESSIBILITY
    QCocoaAccessibility *accessibility() const Q_DECL_OVERRIDE;
#endif
    QCocoaClipboard *clipboard() const Q_DECL_OVERRIDE;
    QCocoaDrag *drag() const Q_DECL_OVERRIDE;

    QStringList themeNames() const Q_DECL_OVERRIDE;
    QPlatformTheme *createPlatformTheme(const QString &name) const Q_DECL_OVERRIDE;
    QCocoaServices *services() const Q_DECL_OVERRIDE;
    QVariant styleHint(StyleHint hint) const Q_DECL_OVERRIDE;

    Qt::KeyboardModifiers queryKeyboardModifiers() const Q_DECL_OVERRIDE;
    QList<int> possibleKeys(const QKeyEvent *event) const Q_DECL_OVERRIDE;

    void updateScreens();
    QCocoaScreen *screenAtIndex(int index);

    void setToolbar(QWindow *window, NSToolbar *toolbar);
    NSToolbar *toolbar(QWindow *window) const;
    void clearToolbars();

    void pushPopupWindow(QCocoaWindow *window);
    QCocoaWindow *popPopupWindow();
    QCocoaWindow *activePopupWindow() const;
    QList<QCocoaWindow *> *popupWindowStack();

    void setApplicationIcon(const QIcon &icon) const Q_DECL_OVERRIDE;
private:
    static QCocoaIntegration *mInstance;
    Options mOptions;

    QScopedPointer<QCoreTextFontDatabase> mFontDb;

    QScopedPointer<QPlatformInputContext> mInputContext;
#ifndef QT_NO_ACCESSIBILITY
    QScopedPointer<QCocoaAccessibility> mAccessibility;
#endif
    QScopedPointer<QPlatformTheme> mPlatformTheme;
    QList<QCocoaScreen *> mScreens;
    QCocoaClipboard  *mCocoaClipboard;
    QScopedPointer<QCocoaDrag> mCocoaDrag;
    QScopedPointer<QCocoaNativeInterface> mNativeInterface;
    QScopedPointer<QCocoaServices> mServices;
    QScopedPointer<QCocoaKeyMapper> mKeyboardMapper;

    QHash<QWindow *, NSToolbar *> mToolbars;
    QList<QCocoaWindow *> m_popupWindowStack;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QCocoaIntegration::Options)

QT_END_NAMESPACE

#endif

