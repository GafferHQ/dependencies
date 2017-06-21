/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKWEBENGINEVIEW_P_P_H
#define QQUICKWEBENGINEVIEW_P_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qquickwebengineview_p.h"
#include "web_contents_adapter_client.h"

#include <QScopedPointer>
#include <QSharedData>
#include <QString>
#include <QtCore/qcompilerdetection.h>
#include <QtGui/qaccessibleobject.h>

namespace QtWebEngineCore {
class WebContentsAdapter;
class UIDelegatesManager;
}

QT_BEGIN_NAMESPACE
class QQuickWebEngineView;
class QQmlComponent;
class QQmlContext;
class QQuickWebEngineSettings;

#ifdef ENABLE_QML_TESTSUPPORT_API
class QQuickWebEngineTestSupport;
#endif

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineViewport : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE setDevicePixelRatio NOTIFY devicePixelRatioChanged)
public:
    QQuickWebEngineViewport(QQuickWebEngineViewPrivate *viewPrivate);

    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal);

Q_SIGNALS:
    void devicePixelRatioChanged();

private:
    QQuickWebEngineViewPrivate *d_ptr;

    Q_DECLARE_PRIVATE(QQuickWebEngineView)
};

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineViewExperimental : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickWebEngineViewport *viewport READ viewport)
    Q_PROPERTY(QQmlComponent *extraContextMenuEntriesComponent READ extraContextMenuEntriesComponent WRITE setExtraContextMenuEntriesComponent NOTIFY extraContextMenuEntriesComponentChanged)

    QQuickWebEngineViewport *viewport() const;
    void setExtraContextMenuEntriesComponent(QQmlComponent *);
    QQmlComponent *extraContextMenuEntriesComponent() const;

Q_SIGNALS:
    void extraContextMenuEntriesComponentChanged();

private:
    QQuickWebEngineViewExperimental(QQuickWebEngineViewPrivate* viewPrivate);
    QQuickWebEngineView *q_ptr;
    QQuickWebEngineViewPrivate *d_ptr;

    Q_DECLARE_PRIVATE(QQuickWebEngineView)
    Q_DECLARE_PUBLIC(QQuickWebEngineView)
};

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineViewPrivate : public QtWebEngineCore::WebContentsAdapterClient
{
public:
    Q_DECLARE_PUBLIC(QQuickWebEngineView)
    QQuickWebEngineView *q_ptr;
    QQuickWebEngineViewPrivate();
    ~QQuickWebEngineViewPrivate();

    QQuickWebEngineViewExperimental *experimental() const;
    QQuickWebEngineViewport *viewport() const;
    QtWebEngineCore::UIDelegatesManager *ui();

    virtual QtWebEngineCore::RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegate(QtWebEngineCore::RenderWidgetHostViewQtDelegateClient *client) Q_DECL_OVERRIDE;
    virtual QtWebEngineCore::RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegateForPopup(QtWebEngineCore::RenderWidgetHostViewQtDelegateClient *client) Q_DECL_OVERRIDE;
    virtual void titleChanged(const QString&) Q_DECL_OVERRIDE;
    virtual void urlChanged(const QUrl&) Q_DECL_OVERRIDE;
    virtual void iconChanged(const QUrl&) Q_DECL_OVERRIDE;
    virtual void loadProgressChanged(int progress) Q_DECL_OVERRIDE;
    virtual void didUpdateTargetURL(const QUrl&) Q_DECL_OVERRIDE;
    virtual void selectionChanged() Q_DECL_OVERRIDE { }
    virtual QRectF viewportRect() const Q_DECL_OVERRIDE;
    virtual qreal dpiScale() const Q_DECL_OVERRIDE;
    virtual QColor backgroundColor() const Q_DECL_OVERRIDE;
    virtual void loadStarted(const QUrl &provisionalUrl, bool isErrorPage = false) Q_DECL_OVERRIDE;
    virtual void loadCommitted() Q_DECL_OVERRIDE;
    virtual void loadVisuallyCommitted() Q_DECL_OVERRIDE;
    virtual void loadFinished(bool success, const QUrl &url, bool isErrorPage = false, int errorCode = 0, const QString &errorDescription = QString()) Q_DECL_OVERRIDE;
    virtual void focusContainer() Q_DECL_OVERRIDE;
    virtual void unhandledKeyEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    virtual void adoptNewWindow(QtWebEngineCore::WebContentsAdapter *newWebContents, WindowOpenDisposition disposition, bool userGesture, const QRect &) Q_DECL_OVERRIDE;
    virtual bool isBeingAdopted() Q_DECL_OVERRIDE;
    virtual void close() Q_DECL_OVERRIDE;
    virtual void windowCloseRejected() Q_DECL_OVERRIDE;
    virtual void requestFullScreenMode(const QUrl &origin, bool fullscreen) Q_DECL_OVERRIDE;
    virtual bool isFullScreenMode() const Q_DECL_OVERRIDE;
    virtual bool contextMenuRequested(const QtWebEngineCore::WebEngineContextMenuData &) Q_DECL_OVERRIDE;
    virtual void navigationRequested(int navigationType, const QUrl &url, int &navigationRequestAction, bool isMainFrame) Q_DECL_OVERRIDE;
    virtual void javascriptDialog(QSharedPointer<QtWebEngineCore::JavaScriptDialogController>) Q_DECL_OVERRIDE;
    virtual void runFileChooser(QtWebEngineCore::FilePickerController *controller) Q_DECL_OVERRIDE;
    virtual void didRunJavaScript(quint64, const QVariant&) Q_DECL_OVERRIDE;
    virtual void didFetchDocumentMarkup(quint64, const QString&) Q_DECL_OVERRIDE { }
    virtual void didFetchDocumentInnerText(quint64, const QString&) Q_DECL_OVERRIDE { }
    virtual void didFindText(quint64, int) Q_DECL_OVERRIDE;
    virtual void passOnFocus(bool reverse) Q_DECL_OVERRIDE;
    virtual void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID) Q_DECL_OVERRIDE;
    virtual void authenticationRequired(QSharedPointer<QtWebEngineCore::AuthenticationDialogController>) Q_DECL_OVERRIDE;
    virtual void runMediaAccessPermissionRequest(const QUrl &securityOrigin, MediaRequestFlags requestFlags) Q_DECL_OVERRIDE;
    virtual void runMouseLockPermissionRequest(const QUrl &securityOrigin) Q_DECL_OVERRIDE;
#ifndef QT_NO_ACCESSIBILITY
    virtual QObject *accessibilityParentObject() Q_DECL_OVERRIDE;
#endif // QT_NO_ACCESSIBILITY
    virtual QtWebEngineCore::WebEngineSettings *webEngineSettings() const Q_DECL_OVERRIDE;
    virtual void allowCertificateError(const QSharedPointer<CertificateErrorController> &errorController);
    virtual void runGeolocationPermissionRequest(QUrl const&) Q_DECL_OVERRIDE;
    virtual void showValidationMessage(const QRect &anchor, const QString &mainText, const QString &subText) Q_DECL_OVERRIDE;
    virtual void hideValidationMessage() Q_DECL_OVERRIDE;
    virtual void moveValidationMessage(const QRect &anchor) Q_DECL_OVERRIDE;
    virtual void renderProcessTerminated(RenderProcessTerminationStatus terminationStatus,
                                     int exitCode) Q_DECL_OVERRIDE;
    virtual void requestGeometryChange(const QRect &geometry) Q_DECL_OVERRIDE { Q_UNUSED(geometry); }

    virtual QSharedPointer<QtWebEngineCore::BrowserContextAdapter> browserContextAdapter() Q_DECL_OVERRIDE;

    void setDevicePixelRatio(qreal);
    void adoptWebContents(QtWebEngineCore::WebContentsAdapter *webContents);
    void setProfile(QQuickWebEngineProfile *profile);
    void ensureContentsAdapter();
    void setFullScreenMode(bool);

    // QQmlListPropertyHelpers
    static void userScripts_append(QQmlListProperty<QQuickWebEngineScript> *p, QQuickWebEngineScript *script);
    static int userScripts_count(QQmlListProperty<QQuickWebEngineScript> *p);
    static QQuickWebEngineScript *userScripts_at(QQmlListProperty<QQuickWebEngineScript> *p, int idx);
    static void userScripts_clear(QQmlListProperty<QQuickWebEngineScript> *p);

    QExplicitlySharedDataPointer<QtWebEngineCore::WebContentsAdapter> adapter;
    QScopedPointer<QQuickWebEngineViewExperimental> e;
    QScopedPointer<QQuickWebEngineViewport> v;
    QScopedPointer<QQuickWebEngineHistory> m_history;
    QQuickWebEngineProfile *m_profile;
    QScopedPointer<QQuickWebEngineSettings> m_settings;
#ifdef ENABLE_QML_TESTSUPPORT_API
    QQuickWebEngineTestSupport *m_testSupport;
#endif
    QQmlComponent *contextMenuExtraItems;
    QtWebEngineCore::WebEngineContextMenuData contextMenuData;
    QUrl explicitUrl;
    QUrl icon;
    int loadProgress;
    bool m_fullscreenMode;
    bool isLoading;
    bool m_activeFocusOnPress;
    qreal devicePixelRatio;
    QMap<quint64, QJSValue> m_callbacks;
    QList<QSharedPointer<CertificateErrorController> > m_certificateErrorControllers;
    QQmlWebChannel *m_webChannel;

private:
    QScopedPointer<QtWebEngineCore::UIDelegatesManager> m_uIDelegatesManager;
    QList<QQuickWebEngineScript *> m_userScripts;
    qreal m_dpiScale;
    QColor m_backgroundColor;
    qreal m_defaultZoomFactor;
};

#ifndef QT_NO_ACCESSIBILITY
class QQuickWebEngineViewAccessible : public QAccessibleObject
{
public:
    QQuickWebEngineViewAccessible(QQuickWebEngineView *o);
    QAccessibleInterface *parent() const Q_DECL_OVERRIDE;
    int childCount() const Q_DECL_OVERRIDE;
    QAccessibleInterface *child(int index) const Q_DECL_OVERRIDE;
    int indexOfChild(const QAccessibleInterface*) const Q_DECL_OVERRIDE;
    QString text(QAccessible::Text) const Q_DECL_OVERRIDE;
    QAccessible::Role role() const Q_DECL_OVERRIDE;
    QAccessible::State state() const Q_DECL_OVERRIDE;

private:
    QQuickWebEngineView *engineView() const { return static_cast<QQuickWebEngineView*>(object()); }
};
#endif // QT_NO_ACCESSIBILITY
QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickWebEngineViewExperimental)
QML_DECLARE_TYPE(QQuickWebEngineViewport)

#endif // QQUICKWEBENGINEVIEW_P_P_H
