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

#include "qquickwebengineview_p.h"
#include "qquickwebengineview_p_p.h"

#include "authentication_dialog_controller.h"
#include "browser_context_adapter.h"
#include "certificate_error_controller.h"
#include "file_picker_controller.h"
#include "javascript_dialog_controller.h"
#include "qquickwebenginehistory_p.h"
#include "qquickwebenginecertificateerror_p.h"
#include "qquickwebengineloadrequest_p.h"
#include "qquickwebenginenavigationrequest_p.h"
#include "qquickwebenginenewviewrequest_p.h"
#include "qquickwebengineprofile_p.h"
#include "qquickwebenginesettings_p.h"
#include "qquickwebenginescript_p_p.h"

#ifdef ENABLE_QML_TESTSUPPORT_API
#include "qquickwebenginetestsupport_p.h"
#endif

#include "render_widget_host_view_qt_delegate_quick.h"
#include "render_widget_host_view_qt_delegate_quickwindow.h"
#include "ui_delegates_manager.h"
#include "user_script_controller_host.h"
#include "web_contents_adapter.h"
#include "web_engine_error.h"
#include "web_engine_settings.h"
#include "web_engine_visited_links_manager.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMimeData>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQmlWebChannel>
#include <QQuickWebEngineProfile>
#include <QScreen>
#include <QUrl>
#include <QTimer>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#ifndef QT_NO_ACCESSIBILITY
#include <private/qquickaccessibleattached_p.h>
#endif // QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE
using namespace QtWebEngineCore;

#ifndef QT_NO_ACCESSIBILITY
static QAccessibleInterface *webAccessibleFactory(const QString &, QObject *object)
{
    if (QQuickWebEngineView *v = qobject_cast<QQuickWebEngineView*>(object))
        return new QQuickWebEngineViewAccessible(v);
    return 0;
}
#endif // QT_NO_ACCESSIBILITY

QQuickWebEngineViewPrivate::QQuickWebEngineViewPrivate()
    : adapter(0)
    , e(new QQuickWebEngineViewExperimental(this))
    , v(new QQuickWebEngineViewport(this))
    , m_history(new QQuickWebEngineHistory(this))
    , m_profile(QQuickWebEngineProfile::defaultProfile())
    , m_settings(new QQuickWebEngineSettings(m_profile->settings()))
#ifdef ENABLE_QML_TESTSUPPORT_API
    , m_testSupport(0)
#endif
    , contextMenuExtraItems(0)
    , loadProgress(0)
    , m_fullscreenMode(false)
    , isLoading(false)
    , m_activeFocusOnPress(true)
    , devicePixelRatio(QGuiApplication::primaryScreen()->devicePixelRatio())
    , m_webChannel(0)
    , m_dpiScale(1.0)
    , m_backgroundColor(Qt::white)
    , m_defaultZoomFactor(1.0)
{
    // The gold standard for mobile web content is 160 dpi, and the devicePixelRatio expected
    // is the (possibly quantized) ratio of device dpi to 160 dpi.
    // However GUI toolkits on non-iOS platforms may be using different criteria than relative
    // DPI (depending on the history of that platform), dictating the choice of
    // QScreen::devicePixelRatio().
    // Where applicable (i.e. non-iOS mobile platforms), override QScreen::devicePixelRatio
    // and instead use a reasonable default value for viewport.devicePixelRatio to avoid every
    // app having to use this experimental API.
    QString platform = qApp->platformName().toLower();
    if (platform == QLatin1String("qnx")) {
        qreal webPixelRatio = QGuiApplication::primaryScreen()->physicalDotsPerInch() / 160;

        // Quantize devicePixelRatio to increments of 1 to allow JS and media queries to select
        // 1x, 2x, 3x etc assets that fit an integral number of pixels.
        setDevicePixelRatio(qMax(1, qRound(webPixelRatio)));
    }
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::installFactory(&webAccessibleFactory);
#endif // QT_NO_ACCESSIBILITY
}

QQuickWebEngineViewPrivate::~QQuickWebEngineViewPrivate()
{
}

QQuickWebEngineViewExperimental *QQuickWebEngineViewPrivate::experimental() const
{
    return e.data();
}

QQuickWebEngineViewport *QQuickWebEngineViewPrivate::viewport() const
{
    return v.data();
}

UIDelegatesManager *QQuickWebEngineViewPrivate::ui()
{
    Q_Q(QQuickWebEngineView);
    if (m_uIDelegatesManager.isNull())
        m_uIDelegatesManager.reset(new UIDelegatesManager(q));
    return m_uIDelegatesManager.data();
}

RenderWidgetHostViewQtDelegate *QQuickWebEngineViewPrivate::CreateRenderWidgetHostViewQtDelegate(RenderWidgetHostViewQtDelegateClient *client)
{
    return new RenderWidgetHostViewQtDelegateQuick(client, /*isPopup = */ false);
}

RenderWidgetHostViewQtDelegate *QQuickWebEngineViewPrivate::CreateRenderWidgetHostViewQtDelegateForPopup(RenderWidgetHostViewQtDelegateClient *client)
{
    Q_Q(QQuickWebEngineView);
    const bool hasWindowCapability = QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::MultipleWindows);
    RenderWidgetHostViewQtDelegateQuick *quickDelegate = new RenderWidgetHostViewQtDelegateQuick(client, /*isPopup = */ true);
    if (hasWindowCapability) {
        RenderWidgetHostViewQtDelegateQuickWindow *wrapperWindow = new RenderWidgetHostViewQtDelegateQuickWindow(quickDelegate);
        quickDelegate->setParentItem(wrapperWindow->contentItem());
        return wrapperWindow;
    }
    quickDelegate->setParentItem(q);
    return quickDelegate;
}

bool QQuickWebEngineViewPrivate::contextMenuRequested(const WebEngineContextMenuData &data)
{
    Q_Q(QQuickWebEngineView);

    // Assign the WebEngineView as the parent of the menu, so mouse events are properly propagated
    // on OSX.
    QObject *menu = ui()->addMenu(q, QString(), data.pos);
    if (!menu)
        return false;

    contextMenuData = data;

    // Populate our menu
    MenuItemHandler *item = 0;

    if (!data.linkText.isEmpty() && data.linkUrl.isValid()) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::OpenLinkInThisWindow); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Follow Link"));
    }

    if (data.selectedText.isEmpty()) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, q, &QQuickWebEngineView::goBack);
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Back"), QStringLiteral("go-previous"), q->canGoBack());

        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, q, &QQuickWebEngineView::goForward);
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Forward"), QStringLiteral("go-next"), q->canGoForward());

        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, q, &QQuickWebEngineView::reload);
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Reload"), QStringLiteral("view-refresh"));
    } else {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::Copy); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy"));
    }

    if (!contextMenuData.linkText.isEmpty() && contextMenuData.linkUrl.isValid()) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::CopyLinkToClipboard); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy Link URL"));
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::DownloadLinkToDisk); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Save Link"));
    }
    if (contextMenuData.mediaUrl.isValid()) {
        switch (contextMenuData.mediaType) {
        case WebEngineContextMenuData::MediaTypeImage:
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::CopyImageUrlToClipboard); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy Image URL"));
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::CopyImageToClipboard); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy Image"));
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::DownloadImageToDisk); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Save Image"));
            break;
        case WebEngineContextMenuData::MediaTypeCanvas:
            Q_UNREACHABLE();    // mediaUrl is invalid for canvases
            break;
        case WebEngineContextMenuData::MediaTypeAudio:
        case WebEngineContextMenuData::MediaTypeVideo:
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::CopyMediaUrlToClipboard); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy Media URL"));
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::DownloadMediaToDisk); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Save Media"));
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::ToggleMediaPlayPause); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Toggle Play/Pause"));
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::ToggleMediaLoop); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Toggle Looping"));
            if (contextMenuData.mediaFlags & WebEngineContextMenuData::MediaHasAudio) {
                item = new MenuItemHandler(menu);
                QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::ToggleMediaMute); });
                ui()->addMenuItem(item, QQuickWebEngineView::tr("Toggle Mute"));
            }
            if (contextMenuData.mediaFlags & WebEngineContextMenuData::MediaCanToggleControls) {
                item = new MenuItemHandler(menu);
                QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::ToggleMediaControls); });
                ui()->addMenuItem(item, QQuickWebEngineView::tr("Toggle Media Controls"));
            }
            break;
        default:
            break;
        }
    } else if (contextMenuData.mediaType == WebEngineContextMenuData::MediaTypeCanvas) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::CopyImageToClipboard); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy Image"));
    }
    if (adapter->hasInspector()) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::InspectElement); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Inspect Element"));
    }
    if (isFullScreenMode()) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::ExitFullScreen); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Exit Full Screen Mode"));
    }

    // FIXME: expose the context menu data as an attached property to make this more useful
    if (contextMenuExtraItems) {
        ui()->addMenuSeparator(menu);
        if (QObject* menuExtras = contextMenuExtraItems->create(qmlContext(q))) {
            menuExtras->setParent(menu);
            QQmlListReference entries(menu, defaultPropertyName(menu), qmlEngine(q));
            if (entries.isValid())
                entries.append(menuExtras);
        }
    }

    // Now fire the popup() method on the top level menu
    QMetaObject::invokeMethod(menu, "popup");
    return true;
}

void QQuickWebEngineViewPrivate::navigationRequested(int navigationType, const QUrl &url, int &navigationRequestAction, bool isMainFrame)
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineNavigationRequest navigationRequest(url, static_cast<QQuickWebEngineView::NavigationType>(navigationType), isMainFrame);
    Q_EMIT q->navigationRequested(&navigationRequest);

    navigationRequestAction = navigationRequest.action();
}

void QQuickWebEngineViewPrivate::javascriptDialog(QSharedPointer<JavaScriptDialogController> dialog)
{
    ui()->showDialog(dialog);
}

void QQuickWebEngineViewPrivate::allowCertificateError(const QSharedPointer<CertificateErrorController> &errorController)
{
    Q_Q(QQuickWebEngineView);

    QQuickWebEngineCertificateError *quickController = new QQuickWebEngineCertificateError(errorController);
    QQmlEngine::setObjectOwnership(quickController, QQmlEngine::JavaScriptOwnership);
    Q_EMIT q->certificateError(quickController);
    if (!quickController->deferred() && !quickController->answered())
        quickController->rejectCertificate();
    else
        m_certificateErrorControllers.append(errorController);
}

void QQuickWebEngineViewPrivate::runGeolocationPermissionRequest(const QUrl &url)
{
    Q_Q(QQuickWebEngineView);
    Q_EMIT q->featurePermissionRequested(url, QQuickWebEngineView::Geolocation);
}

void QQuickWebEngineViewPrivate::runFileChooser(FilePickerController* controller)
{
    ui()->showFilePicker(controller);
}

void QQuickWebEngineViewPrivate::passOnFocus(bool reverse)
{
    Q_Q(QQuickWebEngineView);
    // The child delegate currently has focus, find the next one from there and give it focus.
    QQuickItem *next = q->scopedFocusItem()->nextItemInFocusChain(!reverse);
    next->forceActiveFocus(reverse ? Qt::BacktabFocusReason : Qt::TabFocusReason);
}

void QQuickWebEngineViewPrivate::titleChanged(const QString &title)
{
    Q_Q(QQuickWebEngineView);
    Q_UNUSED(title);
    Q_EMIT q->titleChanged();
}

void QQuickWebEngineViewPrivate::urlChanged(const QUrl &url)
{
    Q_Q(QQuickWebEngineView);
    Q_UNUSED(url);
    explicitUrl = QUrl();
    Q_EMIT q->urlChanged();
}

void QQuickWebEngineViewPrivate::iconChanged(const QUrl &url)
{
    Q_Q(QQuickWebEngineView);
    icon = url;
    Q_EMIT q->iconChanged();
}

void QQuickWebEngineViewPrivate::loadProgressChanged(int progress)
{
    Q_Q(QQuickWebEngineView);
    loadProgress = progress;
    Q_EMIT q->loadProgressChanged();
}

void QQuickWebEngineViewPrivate::didUpdateTargetURL(const QUrl &hoveredUrl)
{
    Q_Q(QQuickWebEngineView);
    Q_EMIT q->linkHovered(hoveredUrl);
}

QRectF QQuickWebEngineViewPrivate::viewportRect() const
{
    Q_Q(const QQuickWebEngineView);
    return QRectF(q->x(), q->y(), q->width(), q->height());
}

qreal QQuickWebEngineViewPrivate::dpiScale() const
{
    return m_dpiScale;
}

QColor QQuickWebEngineViewPrivate::backgroundColor() const
{
    return m_backgroundColor;
}

void QQuickWebEngineViewPrivate::loadStarted(const QUrl &provisionalUrl, bool isErrorPage)
{
    Q_Q(QQuickWebEngineView);
    if (isErrorPage) {
#ifdef ENABLE_QML_TESTSUPPORT_API
        if (m_testSupport)
            m_testSupport->errorPage()->loadStarted(provisionalUrl);
#endif
        return;
    }

    isLoading = true;
    m_history->reset();
    m_certificateErrorControllers.clear();
    QQuickWebEngineLoadRequest loadRequest(provisionalUrl, QQuickWebEngineView::LoadStartedStatus);
    Q_EMIT q->loadingChanged(&loadRequest);
}

void QQuickWebEngineViewPrivate::loadCommitted()
{
    m_history->reset();
}

void QQuickWebEngineViewPrivate::loadVisuallyCommitted()
{
#ifdef ENABLE_QML_TESTSUPPORT_API
    if (m_testSupport)
        Q_EMIT m_testSupport->loadVisuallyCommitted();
#endif
}

Q_STATIC_ASSERT(static_cast<int>(WebEngineError::NoErrorDomain) == static_cast<int>(QQuickWebEngineView::NoErrorDomain));
Q_STATIC_ASSERT(static_cast<int>(WebEngineError::CertificateErrorDomain) == static_cast<int>(QQuickWebEngineView::CertificateErrorDomain));
Q_STATIC_ASSERT(static_cast<int>(WebEngineError::DnsErrorDomain) == static_cast<int>(QQuickWebEngineView::DnsErrorDomain));

void QQuickWebEngineViewPrivate::loadFinished(bool success, const QUrl &url, bool isErrorPage, int errorCode, const QString &errorDescription)
{
    Q_Q(QQuickWebEngineView);

    if (isErrorPage) {
#ifdef ENABLE_QML_TESTSUPPORT_API
        if (m_testSupport)
            m_testSupport->errorPage()->loadFinished(success, url);
#endif
        return;
    }

    isLoading = false;
    m_history->reset();
    if (errorCode == WebEngineError::UserAbortedError) {
        QQuickWebEngineLoadRequest loadRequest(url, QQuickWebEngineView::LoadStoppedStatus);
        Q_EMIT q->loadingChanged(&loadRequest);
        return;
    }
    if (success) {
        explicitUrl = QUrl();
        QQuickWebEngineLoadRequest loadRequest(url, QQuickWebEngineView::LoadSucceededStatus);
        Q_EMIT q->loadingChanged(&loadRequest);
        return;
    }

    Q_ASSERT(errorCode);
    QQuickWebEngineLoadRequest loadRequest(
        url,
        QQuickWebEngineView::LoadFailedStatus,
        errorDescription,
        errorCode,
        static_cast<QQuickWebEngineView::ErrorDomain>(WebEngineError::toQtErrorDomain(errorCode)));
    Q_EMIT q->loadingChanged(&loadRequest);
    return;
}

void QQuickWebEngineViewPrivate::focusContainer()
{
    Q_Q(QQuickWebEngineView);
    q->forceActiveFocus();
}

void QQuickWebEngineViewPrivate::unhandledKeyEvent(QKeyEvent *event)
{
    Q_Q(QQuickWebEngineView);
    if (q->parentItem())
        q->window()->sendEvent(q->parentItem(), event);
}

void QQuickWebEngineViewPrivate::adoptNewWindow(WebContentsAdapter *newWebContents, WindowOpenDisposition disposition, bool userGesture, const QRect &)
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineNewViewRequest request;
    // This increases the ref-count of newWebContents and will tell Chromium
    // to start loading it and possibly return it to its parent page window.open().
    request.m_adapter = newWebContents;
    request.m_isUserInitiated = userGesture;

    switch (disposition) {
    case WebContentsAdapterClient::NewForegroundTabDisposition:
        request.m_destination = QQuickWebEngineView::NewViewInTab;
        break;
    case WebContentsAdapterClient::NewBackgroundTabDisposition:
        request.m_destination = QQuickWebEngineView::NewViewInBackgroundTab;
        break;
    case WebContentsAdapterClient::NewPopupDisposition:
        request.m_destination = QQuickWebEngineView::NewViewInDialog;
        break;
    case WebContentsAdapterClient::NewWindowDisposition:
        request.m_destination = QQuickWebEngineView::NewViewInWindow;
        break;
    default:
        Q_UNREACHABLE();
    }

    Q_EMIT q->newViewRequested(&request);
}

bool QQuickWebEngineViewPrivate::isBeingAdopted()
{
    return false;
}

void QQuickWebEngineViewPrivate::close()
{
    Q_Q(QQuickWebEngineView);
    emit q->windowCloseRequested();
}

void QQuickWebEngineViewPrivate::windowCloseRejected()
{
#ifdef ENABLE_QML_TESTSUPPORT_API
    if (m_testSupport)
        Q_EMIT m_testSupport->windowCloseRejected();
#endif
}

void QQuickWebEngineViewPrivate::requestFullScreenMode(const QUrl &origin, bool fullscreen)
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineFullScreenRequest request(this, origin, fullscreen);
    Q_EMIT q->fullScreenRequested(request);
}

bool QQuickWebEngineViewPrivate::isFullScreenMode() const
{
    return m_fullscreenMode;
}

void QQuickWebEngineViewPrivate::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID)
{
    Q_Q(QQuickWebEngineView);
    if (q->receivers(SIGNAL(javaScriptConsoleMessage(JavaScriptConsoleMessageLevel,QString,int,QString))) > 0) {
        Q_EMIT q->javaScriptConsoleMessage(static_cast<QQuickWebEngineView::JavaScriptConsoleMessageLevel>(level), message, lineNumber, sourceID);
        return;
    }

    static QLoggingCategory loggingCategory("js", QtWarningMsg);
    const QByteArray file = sourceID.toUtf8();
    QMessageLogger logger(file.constData(), lineNumber, nullptr, loggingCategory.categoryName());

    switch (level) {
    case JavaScriptConsoleMessageLevel::Info:
        if (loggingCategory.isInfoEnabled())
            logger.info().noquote() << message;
        break;
    case JavaScriptConsoleMessageLevel::Warning:
        if (loggingCategory.isWarningEnabled())
            logger.warning().noquote() << message;
        break;
    case JavaScriptConsoleMessageLevel::Error:
        if (loggingCategory.isCriticalEnabled())
            logger.critical().noquote() << message;
        break;
    }
}

void QQuickWebEngineViewPrivate::authenticationRequired(QSharedPointer<AuthenticationDialogController> controller)
{
    ui()->showDialog(controller);
}

void QQuickWebEngineViewPrivate::runMediaAccessPermissionRequest(const QUrl &securityOrigin, WebContentsAdapterClient::MediaRequestFlags requestFlags)
{
    Q_Q(QQuickWebEngineView);
    if (!requestFlags)
        return;
    QQuickWebEngineView::Feature feature;
    if (requestFlags.testFlag(WebContentsAdapterClient::MediaAudioCapture) && requestFlags.testFlag(WebContentsAdapterClient::MediaVideoCapture))
        feature = QQuickWebEngineView::MediaAudioVideoCapture;
    else if (requestFlags.testFlag(WebContentsAdapterClient::MediaAudioCapture))
        feature = QQuickWebEngineView::MediaAudioCapture;
    else // WebContentsAdapterClient::MediaVideoCapture
        feature = QQuickWebEngineView::MediaVideoCapture;
    Q_EMIT q->featurePermissionRequested(securityOrigin, feature);
}

void QQuickWebEngineViewPrivate::runMouseLockPermissionRequest(const QUrl &securityOrigin)
{

    Q_UNUSED(securityOrigin);

    // TODO: Add mouse lock support
    adapter->grantMouseLockPermission(false);
}

#ifndef QT_NO_ACCESSIBILITY
QObject *QQuickWebEngineViewPrivate::accessibilityParentObject()
{
    Q_Q(QQuickWebEngineView);
    return q;
}
#endif // QT_NO_ACCESSIBILITY

QSharedPointer<BrowserContextAdapter> QQuickWebEngineViewPrivate::browserContextAdapter()
{
    return m_profile->d_ptr->browserContext();
}

WebEngineSettings *QQuickWebEngineViewPrivate::webEngineSettings() const
{
    return m_settings->d_ptr.data();
}

void QQuickWebEngineViewPrivate::setDevicePixelRatio(qreal devicePixelRatio)
{
    Q_Q(QQuickWebEngineView);
    this->devicePixelRatio = devicePixelRatio;
    QScreen *screen = q->window() ? q->window()->screen() : QGuiApplication::primaryScreen();
    m_dpiScale = devicePixelRatio / screen->devicePixelRatio();
}

#ifndef QT_NO_ACCESSIBILITY
QQuickWebEngineViewAccessible::QQuickWebEngineViewAccessible(QQuickWebEngineView *o)
    : QAccessibleObject(o)
{}

QAccessibleInterface *QQuickWebEngineViewAccessible::parent() const
{
    QQuickItem *parent = engineView()->parentItem();
    return QAccessible::queryAccessibleInterface(parent);
}

int QQuickWebEngineViewAccessible::childCount() const
{
    if (engineView() && child(0))
        return 1;
    return 0;
}

QAccessibleInterface *QQuickWebEngineViewAccessible::child(int index) const
{
    if (index == 0)
        return engineView()->d_func()->adapter->browserAccessible();
    return 0;
}

int QQuickWebEngineViewAccessible::indexOfChild(const QAccessibleInterface *c) const
{
    if (c == child(0))
        return 0;
    return -1;
}

QString QQuickWebEngineViewAccessible::text(QAccessible::Text) const
{
    return QString();
}

QAccessible::Role QQuickWebEngineViewAccessible::role() const
{
    return QAccessible::Document;
}

QAccessible::State QQuickWebEngineViewAccessible::state() const
{
    QAccessible::State s;
    return s;
}
#endif // QT_NO_ACCESSIBILITY

class WebContentsAdapterOwner : public QObject
{
public:
    typedef QExplicitlySharedDataPointer<QtWebEngineCore::WebContentsAdapter> AdapterPtr;
    WebContentsAdapterOwner(const AdapterPtr &ptr)
        : adapter(ptr)
    {}

private:
    AdapterPtr adapter;
};

void QQuickWebEngineViewPrivate::adoptWebContents(WebContentsAdapter *webContents)
{
    if (!webContents) {
        qWarning("Trying to open an empty request, it was either already used or was invalidated."
            "\nYou must complete the request synchronously within the newViewRequested signal handler."
            " If a view hasn't been adopted before returning, the request will be invalidated.");
        return;
    }

    if (webContents->browserContextAdapter() && browserContextAdapter() != webContents->browserContextAdapter()) {
        qWarning("Can not adopt content from a different WebEngineProfile.");
        return;
    }

    Q_Q(QQuickWebEngineView);

    // This throws away the WebContentsAdapter that has been used until now.
    // All its states, particularly the loading URL, are replaced by the adopted WebContentsAdapter.
    WebContentsAdapterOwner *adapterOwner = new WebContentsAdapterOwner(adapter);
    adapterOwner->deleteLater();
    adapter = webContents;
    adapter->initialize(this);

    // associate the webChannel with the new adapter
    if (m_webChannel)
        adapter->setWebChannel(m_webChannel);

    // set initial background color if non-default
    if (m_backgroundColor != Qt::white)
        adapter->backgroundColorChanged();

    // re-bind the userscrips to the new adapter
    Q_FOREACH (QQuickWebEngineScript *script, m_userScripts)
        script->d_func()->bind(browserContextAdapter()->userScriptController(), adapter.data());

    // set the zoomFactor if it had been changed on the old adapter.
    if (!qFuzzyCompare(adapter->currentZoomFactor(), m_defaultZoomFactor))
        q->setZoomFactor(m_defaultZoomFactor);

    // Emit signals for values that might be different from the previous WebContentsAdapter.
    emit q->titleChanged();
    emit q->urlChanged();
    emit q->iconChanged();
    // FIXME: The current loading state should be stored in the WebContentAdapter
    // and it should be checked here if the signal emission is really necessary.
    QQuickWebEngineLoadRequest loadRequest(adapter->activeUrl(), QQuickWebEngineView::LoadSucceededStatus);
    emit q->loadingChanged(&loadRequest);
    emit q->loadProgressChanged();
}

QQuickWebEngineView::QQuickWebEngineView(QQuickItem *parent)
    : QQuickItem(parent)
    , d_ptr(new QQuickWebEngineViewPrivate)
{
    Q_D(QQuickWebEngineView);
    d->e->q_ptr = d->q_ptr = this;
    this->setActiveFocusOnTab(true);
    this->setFlags(QQuickItem::ItemIsFocusScope | QQuickItem::ItemAcceptsInputMethod);

#ifndef QT_NO_ACCESSIBILITY
    QQuickAccessibleAttached *accessible = QQuickAccessibleAttached::qmlAttachedProperties(this);
    accessible->setRole(QAccessible::Grouping);
#endif // QT_NO_ACCESSIBILITY
}

QQuickWebEngineView::~QQuickWebEngineView()
{
}

void QQuickWebEngineViewPrivate::ensureContentsAdapter()
{
    Q_Q(QQuickWebEngineView);
    if (!adapter) {
        adapter = new WebContentsAdapter();
        adapter->initialize(this);
        if (m_backgroundColor != Qt::white)
            adapter->backgroundColorChanged();
        if (m_webChannel)
            adapter->setWebChannel(m_webChannel);
        if (explicitUrl.isValid())
            adapter->load(explicitUrl);
        // push down the page's user scripts
        Q_FOREACH (QQuickWebEngineScript *script, m_userScripts)
            script->d_func()->bind(browserContextAdapter()->userScriptController(), adapter.data());
        // set the zoomFactor if it had been changed on the old adapter.
        if (!qFuzzyCompare(adapter->currentZoomFactor(), m_defaultZoomFactor))
            q->setZoomFactor(m_defaultZoomFactor);

    }
}

void QQuickWebEngineViewPrivate::setFullScreenMode(bool fullscreen)
{
    Q_Q(QQuickWebEngineView);
    if (m_fullscreenMode != fullscreen) {
        m_fullscreenMode = fullscreen;
        adapter->changedFullScreen();
        Q_EMIT q->isFullScreenChanged();
    }
}

QUrl QQuickWebEngineView::url() const
{
    Q_D(const QQuickWebEngineView);
    return d->explicitUrl.isValid() ? d->explicitUrl : (d->adapter ? d->adapter->activeUrl() : QUrl());
}

void QQuickWebEngineView::setUrl(const QUrl& url)
{
    if (url.isEmpty())
        return;

    Q_D(QQuickWebEngineView);
    d->explicitUrl = url;
    if (d->adapter)
        d->adapter->load(url);
    if (!qmlEngine(this) || isComponentComplete())
        d->ensureContentsAdapter();
}

QUrl QQuickWebEngineView::icon() const
{
    Q_D(const QQuickWebEngineView);
    return d->icon;
}

void QQuickWebEngineView::loadHtml(const QString &html, const QUrl &baseUrl)
{
    Q_D(QQuickWebEngineView);
    d->explicitUrl = QUrl();
    if (!qmlEngine(this) || isComponentComplete())
        d->ensureContentsAdapter();
    if (d->adapter)
        d->adapter->setContent(html.toUtf8(), QStringLiteral("text/html;charset=UTF-8"), baseUrl);
}

void QQuickWebEngineView::goBack()
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    d->adapter->navigateToOffset(-1);
}

void QQuickWebEngineView::goForward()
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    d->adapter->navigateToOffset(1);
}

void QQuickWebEngineView::reload()
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    d->adapter->reload();
}

void QQuickWebEngineView::reloadAndBypassCache()
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    d->adapter->reloadAndBypassCache();
}

void QQuickWebEngineView::stop()
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    d->adapter->stop();
}

void QQuickWebEngineView::setZoomFactor(qreal arg)
{
    Q_D(QQuickWebEngineView);
    d->m_defaultZoomFactor = arg;

    if (!d->adapter)
        return;

    qreal oldFactor = d->adapter->currentZoomFactor();
    d->adapter->setZoomFactor(arg);
    if (qFuzzyCompare(oldFactor, d->adapter->currentZoomFactor()))
        return;

    emit zoomFactorChanged(arg);
}

QQuickWebEngineProfile *QQuickWebEngineView::profile() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_profile;
}

void QQuickWebEngineView::setProfile(QQuickWebEngineProfile *profile)
{
    Q_D(QQuickWebEngineView);
    d->setProfile(profile);
}

QQuickWebEngineSettings *QQuickWebEngineView::settings() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_settings.data();
}

QQmlListProperty<QQuickWebEngineScript> QQuickWebEngineView::userScripts()
{
    Q_D(QQuickWebEngineView);
    return QQmlListProperty<QQuickWebEngineScript>(this, d,
                                                   d->userScripts_append,
                                                   d->userScripts_count,
                                                   d->userScripts_at,
                                                   d->userScripts_clear);
}

void QQuickWebEngineViewPrivate::setProfile(QQuickWebEngineProfile *profile)
{
    Q_Q(QQuickWebEngineView);

    if (profile == m_profile)
        return;
    m_profile = profile;
    Q_EMIT q->profileChanged();
    m_settings->setParentSettings(profile->settings());

    if (adapter && adapter->browserContext() != browserContextAdapter()->browserContext()) {
        // When the profile changes we need to create a new WebContentAdapter and reload the active URL.
        QUrl activeUrl = adapter->activeUrl();
        adapter = 0;
        ensureContentsAdapter();

        if (!explicitUrl.isValid() && activeUrl.isValid())
            adapter->load(activeUrl);
    }
}

#ifdef ENABLE_QML_TESTSUPPORT_API
QQuickWebEngineTestSupport *QQuickWebEngineView::testSupport() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_testSupport;
}

void QQuickWebEngineView::setTestSupport(QQuickWebEngineTestSupport *testSupport)
{
    Q_D(QQuickWebEngineView);
    d->m_testSupport = testSupport;
}

#endif

/*!
 * \qmlproperty bool WebEngineView::activeFocusOnPress
 * \since QtWebEngine 1.2
 *
 * This property specifies whether the view should gain active focus when pressed.
 * The default value is true.
 *
 */
bool QQuickWebEngineView::activeFocusOnPress() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_activeFocusOnPress;
}

void QQuickWebEngineViewPrivate::didRunJavaScript(quint64 requestId, const QVariant &result)
{
    Q_Q(QQuickWebEngineView);
    QJSValue callback = m_callbacks.take(requestId);
    QJSValueList args;
    args.append(qmlEngine(q)->toScriptValue(result));
    callback.call(args);
}

void QQuickWebEngineViewPrivate::didFindText(quint64 requestId, int matchCount)
{
    QJSValue callback = m_callbacks.take(requestId);
    QJSValueList args;
    args.append(QJSValue(matchCount));
    callback.call(args);
}
void QQuickWebEngineViewPrivate::showValidationMessage(const QRect &anchor, const QString &mainText, const QString &subText)
{
#ifdef ENABLE_QML_TESTSUPPORT_API
    if (m_testSupport)
        Q_EMIT m_testSupport->validationMessageShown(mainText, subText);
#endif

    ui()->showMessageBubble(anchor, mainText, subText);
}

void QQuickWebEngineViewPrivate::hideValidationMessage()
{
    ui()->hideMessageBubble();
}

void QQuickWebEngineViewPrivate::moveValidationMessage(const QRect &anchor)
{
    ui()->moveMessageBubble(anchor);
}

void QQuickWebEngineViewPrivate::renderProcessTerminated(
        RenderProcessTerminationStatus terminationStatus, int exitCode)
{
    Q_Q(QQuickWebEngineView);
    Q_EMIT q->renderProcessTerminated(static_cast<QQuickWebEngineView::RenderProcessTerminationStatus>(
                                      renderProcessExitStatus(terminationStatus)), exitCode);
}

bool QQuickWebEngineView::isLoading() const
{
    Q_D(const QQuickWebEngineView);
    return d->isLoading;
}

int QQuickWebEngineView::loadProgress() const
{
    Q_D(const QQuickWebEngineView);
    return d->loadProgress;
}

QString QQuickWebEngineView::title() const
{
    Q_D(const QQuickWebEngineView);
    if (!d->adapter)
        return QString();
    return d->adapter->pageTitle();
}

bool QQuickWebEngineView::canGoBack() const
{
    Q_D(const QQuickWebEngineView);
    if (!d->adapter)
        return false;
    return d->adapter->canGoBack();
}

bool QQuickWebEngineView::canGoForward() const
{
    Q_D(const QQuickWebEngineView);
    if (!d->adapter)
        return false;
    return d->adapter->canGoForward();
}

void QQuickWebEngineView::runJavaScript(const QString &script, const QJSValue &callback)
{
    Q_D(QQuickWebEngineView);
    d->ensureContentsAdapter();
    if (!callback.isUndefined()) {
        quint64 requestId = d_ptr->adapter->runJavaScriptCallbackResult(script);
        d->m_callbacks.insert(requestId, callback);
    } else
        d->adapter->runJavaScript(script);
}

QQuickWebEngineViewExperimental *QQuickWebEngineView::experimental() const
{
    Q_D(const QQuickWebEngineView);
    return d->e.data();
}

qreal QQuickWebEngineView::zoomFactor() const
{
    Q_D(const QQuickWebEngineView);
    if (!d->adapter)
        return d->m_defaultZoomFactor;
    return d->adapter->currentZoomFactor();
}

/*!
    \qmlproperty bool WebEngineView::backgroundColor
    \since QtWebEngine 1.2

    Sets this property to change the color of the WebEngineView's background,
    behing the document's body. You can set it to "transparent" or to a translucent
    color to see through the document, or you can set this color to match your
    web content in an hybrid app to prevent the white flashes that may appear
    during loading.

    The default value is white.
*/
QColor QQuickWebEngineView::backgroundColor() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_backgroundColor;
}

void QQuickWebEngineView::setBackgroundColor(const QColor &color)
{
    Q_D(QQuickWebEngineView);
    if (color == d->m_backgroundColor)
        return;
    d->m_backgroundColor = color;
    if (d->adapter)
        d->adapter->backgroundColorChanged();
    emit backgroundColorChanged();
}

bool QQuickWebEngineView::isFullScreen() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_fullscreenMode;
}

void QQuickWebEngineViewExperimental::setExtraContextMenuEntriesComponent(QQmlComponent *contextMenuExtras)
{
    if (d_ptr->contextMenuExtraItems == contextMenuExtras)
        return;
    d_ptr->contextMenuExtraItems = contextMenuExtras;
    emit extraContextMenuEntriesComponentChanged();
}

QQmlComponent *QQuickWebEngineViewExperimental::extraContextMenuEntriesComponent() const
{
    return d_ptr->contextMenuExtraItems;
}

void QQuickWebEngineView::findText(const QString &subString, FindFlags options, const QJSValue &callback)
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    if (subString.isEmpty()) {
        d->adapter->stopFinding();
        if (!callback.isUndefined()) {
            QJSValueList args;
            args.append(QJSValue(0));
            const_cast<QJSValue&>(callback).call(args);
        }
    } else {
        quint64 requestId = d->adapter->findText(subString, options & FindCaseSensitively, options & FindBackward);
        if (!callback.isUndefined())
            d->m_callbacks.insert(requestId, callback);
    }
}

QQuickWebEngineHistory *QQuickWebEngineView::navigationHistory() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_history.data();
}

QQmlWebChannel *QQuickWebEngineView::webChannel()
{
    Q_D(QQuickWebEngineView);
    if (!d->m_webChannel) {
        d->m_webChannel = new QQmlWebChannel(this);
        if (d->adapter)
            d->adapter->setWebChannel(d->m_webChannel);
    }

    return d->m_webChannel;
}

void QQuickWebEngineView::setWebChannel(QQmlWebChannel *webChannel)
{
    Q_D(QQuickWebEngineView);
    if (d->m_webChannel == webChannel)
        return;
    d->m_webChannel = webChannel;
    if (d->adapter)
        d->adapter->setWebChannel(webChannel);
    Q_EMIT webChannelChanged();
}

void QQuickWebEngineView::grantFeaturePermission(const QUrl &securityOrigin, QQuickWebEngineView::Feature feature, bool granted)
{
    if (!d_ptr->adapter)
        return;
    if (!granted && feature >= MediaAudioCapture && feature <= MediaAudioVideoCapture) {
         d_ptr->adapter->grantMediaAccessPermission(securityOrigin, WebContentsAdapterClient::MediaNone);
         return;
    }

    switch (feature) {
    case MediaAudioCapture:
        d_ptr->adapter->grantMediaAccessPermission(securityOrigin, WebContentsAdapterClient::MediaAudioCapture);
        break;
    case MediaVideoCapture:
        d_ptr->adapter->grantMediaAccessPermission(securityOrigin, WebContentsAdapterClient::MediaVideoCapture);
        break;
    case MediaAudioVideoCapture:
        d_ptr->adapter->grantMediaAccessPermission(securityOrigin, WebContentsAdapterClient::MediaRequestFlags(WebContentsAdapterClient::MediaAudioCapture                                                                                                               | WebContentsAdapterClient::MediaVideoCapture));
        break;
    case Geolocation:
        d_ptr->adapter->runGeolocationRequestCallback(securityOrigin, granted);
        break;
    default:
        Q_UNREACHABLE();
    }
}

void QQuickWebEngineView::setActiveFocusOnPress(bool arg)
{
    Q_D(QQuickWebEngineView);
    if (d->m_activeFocusOnPress == arg)
        return;

    d->m_activeFocusOnPress = arg;
    emit activeFocusOnPressChanged(arg);
}

void QQuickWebEngineView::goBackOrForward(int offset)
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    const int current = d->adapter->currentNavigationEntryIndex();
    const int count = d->adapter->navigationEntryCount();
    const int index = current + offset;

    if (index < 0 || index >= count)
        return;

    d->adapter->navigateToIndex(index);
}

void QQuickWebEngineView::fullScreenCancelled()
{
    Q_D(QQuickWebEngineView);
    d->adapter->exitFullScreen();
}

void QQuickWebEngineView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    Q_FOREACH(QQuickItem *child, childItems()) {
        if (qobject_cast<RenderWidgetHostViewQtDelegateQuick *>(child))
            child->setSize(newGeometry.size());
    }
}

void QQuickWebEngineView::itemChange(ItemChange change, const ItemChangeData &value)
{
    Q_D(QQuickWebEngineView);
    if (d->adapter && (change == ItemSceneChange || change == ItemVisibleHasChanged)) {
        if (window() && isVisible())
            d->adapter->wasShown();
        else
            d->adapter->wasHidden();
    }
    QQuickItem::itemChange(change, value);
}

void QQuickWebEngineView::triggerWebAction(WebAction action)
{
    Q_D(QQuickWebEngineView);
    switch (action) {
    case Back:
        d->adapter->navigateToOffset(-1);
        break;
    case Forward:
        d->adapter->navigateToOffset(1);
        break;
    case Stop:
        d->adapter->stop();
        break;
    case Reload:
        d->adapter->reload();
        break;
    case ReloadAndBypassCache:
        d->adapter->reloadAndBypassCache();
        break;
    case Cut:
        d->adapter->cut();
        break;
    case Copy:
        d->adapter->copy();
        break;
    case Paste:
        d->adapter->paste();
        break;
    case Undo:
        d->adapter->undo();
        break;
    case Redo:
        d->adapter->redo();
        break;
    case SelectAll:
        d->adapter->selectAll();
        break;
    case PasteAndMatchStyle:
        d->adapter->pasteAndMatchStyle();
        break;
    case OpenLinkInThisWindow:
        if (d->contextMenuData.linkUrl.isValid())
            setUrl(d->contextMenuData.linkUrl);
        break;
    case OpenLinkInNewWindow:
        if (d->contextMenuData.linkUrl.isValid()) {
            QQuickWebEngineNewViewRequest request;
            request.m_requestedUrl = d->contextMenuData.linkUrl;
            request.m_isUserInitiated = true;
            request.m_destination = NewViewInWindow;
            Q_EMIT newViewRequested(&request);
        }
        break;
    case OpenLinkInNewTab:
        if (d->contextMenuData.linkUrl.isValid()) {
            QQuickWebEngineNewViewRequest request;
            request.m_requestedUrl = d->contextMenuData.linkUrl;
            request.m_isUserInitiated = true;
            request.m_destination = NewViewInBackgroundTab;
            Q_EMIT newViewRequested(&request);
        }
        break;
    case CopyLinkToClipboard:
        if (d->contextMenuData.linkUrl.isValid()) {
            QString urlString = d->contextMenuData.linkUrl.toString(QUrl::FullyEncoded);
            QString title = d->contextMenuData.linkText.toHtmlEscaped();
            QMimeData *data = new QMimeData();
            data->setText(urlString);
            QString html = QStringLiteral("<a href=\"") + urlString + QStringLiteral("\">") + title + QStringLiteral("</a>");
            data->setHtml(html);
            data->setUrls(QList<QUrl>() << d->contextMenuData.linkUrl);
            qApp->clipboard()->setMimeData(data);
        }
        break;
    case DownloadLinkToDisk:
        if (d->contextMenuData.linkUrl.isValid())
            d->adapter->download(d->contextMenuData.linkUrl, d->contextMenuData.suggestedFileName);
        break;
    case CopyImageToClipboard:
        if (d->contextMenuData.hasImageContent &&
                (d->contextMenuData.mediaType == WebEngineContextMenuData::MediaTypeImage ||
                 d->contextMenuData.mediaType == WebEngineContextMenuData::MediaTypeCanvas))
        {
            d->adapter->copyImageAt(d->contextMenuData.pos);
        }
        break;
    case CopyImageUrlToClipboard:
        if (d->contextMenuData.mediaUrl.isValid() && d->contextMenuData.mediaType == WebEngineContextMenuData::MediaTypeImage) {
            QString urlString = d->contextMenuData.mediaUrl.toString(QUrl::FullyEncoded);
            QString title = d->contextMenuData.linkText;
            if (!title.isEmpty())
                title = QStringLiteral(" alt=\"%1\"").arg(title.toHtmlEscaped());
            QMimeData *data = new QMimeData();
            data->setText(urlString);
            QString html = QStringLiteral("<img src=\"") + urlString + QStringLiteral("\"") + title + QStringLiteral("></img>");
            data->setHtml(html);
            data->setUrls(QList<QUrl>() << d->contextMenuData.mediaUrl);
            qApp->clipboard()->setMimeData(data);
        }
        break;
    case DownloadImageToDisk:
    case DownloadMediaToDisk:
        if (d->contextMenuData.mediaUrl.isValid())
            d->adapter->download(d->contextMenuData.mediaUrl, d->contextMenuData.suggestedFileName);
        break;
    case CopyMediaUrlToClipboard:
        if (d->contextMenuData.mediaUrl.isValid() &&
                (d->contextMenuData.mediaType == WebEngineContextMenuData::MediaTypeAudio ||
                 d->contextMenuData.mediaType == WebEngineContextMenuData::MediaTypeVideo))
        {
            QString urlString = d->contextMenuData.mediaUrl.toString(QUrl::FullyEncoded);
            QMimeData *data = new QMimeData();
            data->setText(urlString);
            if (d->contextMenuData.mediaType == WebEngineContextMenuData::MediaTypeAudio)
                data->setHtml(QStringLiteral("<audio src=\"") + urlString + QStringLiteral("\"></audio>"));
            else
                data->setHtml(QStringLiteral("<video src=\"") + urlString + QStringLiteral("\"></video>"));
            data->setUrls(QList<QUrl>() << d->contextMenuData.mediaUrl);
            qApp->clipboard()->setMimeData(data);
        }
        break;
    case ToggleMediaControls:
        if (d->contextMenuData.mediaUrl.isValid() && d->contextMenuData.mediaFlags & WebEngineContextMenuData::MediaCanToggleControls) {
            bool enable = !(d->contextMenuData.mediaFlags & WebEngineContextMenuData::MediaControls);
            d->adapter->executeMediaPlayerActionAt(d->contextMenuData.pos, WebContentsAdapter::MediaPlayerControls, enable);
        }
        break;
    case ToggleMediaLoop:
        if (d->contextMenuData.mediaUrl.isValid() &&
                (d->contextMenuData.mediaType == WebEngineContextMenuData::MediaTypeAudio ||
                 d->contextMenuData.mediaType == WebEngineContextMenuData::MediaTypeVideo))
        {
            bool enable = !(d->contextMenuData.mediaFlags & WebEngineContextMenuData::MediaLoop);
            d->adapter->executeMediaPlayerActionAt(d->contextMenuData.pos, WebContentsAdapter::MediaPlayerLoop, enable);
        }
        break;
    case ToggleMediaPlayPause:
        if (d->contextMenuData.mediaUrl.isValid() &&
                (d->contextMenuData.mediaType == WebEngineContextMenuData::MediaTypeAudio ||
                 d->contextMenuData.mediaType == WebEngineContextMenuData::MediaTypeVideo))
        {
            bool enable = (d->contextMenuData.mediaFlags & WebEngineContextMenuData::MediaPaused);
            d->adapter->executeMediaPlayerActionAt(d->contextMenuData.pos, WebContentsAdapter::MediaPlayerPlay, enable);
        }
        break;
    case ToggleMediaMute:
        if (d->contextMenuData.mediaUrl.isValid() && d->contextMenuData.mediaFlags & WebEngineContextMenuData::MediaHasAudio) {
            bool enable = !(d->contextMenuData.mediaFlags & WebEngineContextMenuData::MediaMuted);
            d->adapter->executeMediaPlayerActionAt(d->contextMenuData.pos, WebContentsAdapter::MediaPlayerMute, enable);
        }
        break;
    case InspectElement:
        d->adapter->inspectElementAt(d->contextMenuData.pos);
        break;
    case ExitFullScreen:
        d->adapter->exitFullScreen();
        break;
    case RequestClose:
        d->adapter->requestClose();
        break;
    default:
        Q_UNREACHABLE();
    }
}

void QQuickWebEngineViewPrivate::userScripts_append(QQmlListProperty<QQuickWebEngineScript> *p, QQuickWebEngineScript *script)
{
    Q_ASSERT(p && p->data);
    QQuickWebEngineViewPrivate *d = static_cast<QQuickWebEngineViewPrivate*>(p->data);
    UserScriptControllerHost *scriptController = d->browserContextAdapter()->userScriptController();
    d->m_userScripts.append(script);
    // If the adapter hasn't been instantiated, we'll bind the scripts in ensureContentsAdapter()
    if (!d->adapter)
        return;
    script->d_func()->bind(scriptController, d->adapter.data());
}

int QQuickWebEngineViewPrivate::userScripts_count(QQmlListProperty<QQuickWebEngineScript> *p)
{
    Q_ASSERT(p && p->data);
    QQuickWebEngineViewPrivate *d = static_cast<QQuickWebEngineViewPrivate*>(p->data);
    return d->m_userScripts.count();
}

QQuickWebEngineScript *QQuickWebEngineViewPrivate::userScripts_at(QQmlListProperty<QQuickWebEngineScript> *p, int idx)
{
    Q_ASSERT(p && p->data);
    QQuickWebEngineViewPrivate *d = static_cast<QQuickWebEngineViewPrivate*>(p->data);
    return d->m_userScripts.at(idx);
}

void QQuickWebEngineViewPrivate::userScripts_clear(QQmlListProperty<QQuickWebEngineScript> *p)
{
    Q_ASSERT(p && p->data);
    QQuickWebEngineViewPrivate *d = static_cast<QQuickWebEngineViewPrivate*>(p->data);
    UserScriptControllerHost *scriptController = d->browserContextAdapter()->userScriptController();
    scriptController->clearAllScripts(d->adapter.data());
    d->m_userScripts.clear();
}

void QQuickWebEngineView::componentComplete()
{
    QQuickItem::componentComplete();
    QTimer::singleShot(0, this, &QQuickWebEngineView::lazyInitialize);
}

void QQuickWebEngineView::lazyInitialize()
{
    Q_D(QQuickWebEngineView);
    d->ensureContentsAdapter();
}

QQuickWebEngineFullScreenRequest::QQuickWebEngineFullScreenRequest()
    : m_viewPrivate(0)
    , m_toggleOn(false)
{
}

QQuickWebEngineFullScreenRequest::QQuickWebEngineFullScreenRequest(QQuickWebEngineViewPrivate *viewPrivate, const QUrl &origin, bool toggleOn)
    : m_viewPrivate(viewPrivate)
    , m_origin(origin)
    , m_toggleOn(toggleOn)
{
}

void QQuickWebEngineFullScreenRequest::accept()
{
    if (m_viewPrivate)
        m_viewPrivate->setFullScreenMode(m_toggleOn);
}

void QQuickWebEngineFullScreenRequest::reject()
{
    if (m_viewPrivate)
        m_viewPrivate->setFullScreenMode(!m_toggleOn);
}

QQuickWebEngineViewExperimental::QQuickWebEngineViewExperimental(QQuickWebEngineViewPrivate *viewPrivate)
    : q_ptr(0)
    , d_ptr(viewPrivate)
{
}

QQuickWebEngineViewport *QQuickWebEngineViewExperimental::viewport() const
{
    Q_D(const QQuickWebEngineView);
    return d->viewport();
}

QQuickWebEngineViewport::QQuickWebEngineViewport(QQuickWebEngineViewPrivate *viewPrivate)
    : d_ptr(viewPrivate)
{
}

qreal QQuickWebEngineViewport::devicePixelRatio() const
{
    Q_D(const QQuickWebEngineView);
    return d->devicePixelRatio;
}

void QQuickWebEngineViewport::setDevicePixelRatio(qreal devicePixelRatio)
{
    Q_D(QQuickWebEngineView);
    // Valid range is [1, inf)
    devicePixelRatio = qMax(qreal(1.0), devicePixelRatio);
    if (d->devicePixelRatio == devicePixelRatio)
        return;
    d->setDevicePixelRatio(devicePixelRatio);
    if (!d->adapter)
        return;
    d->adapter->dpiScaleChanged();
    Q_EMIT devicePixelRatioChanged();
}

QT_END_NAMESPACE

