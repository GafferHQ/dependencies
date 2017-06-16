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

#ifndef WEB_CONTENTS_ADAPTER_CLIENT_H
#define WEB_CONTENTS_ADAPTER_CLIENT_H

#include "qtwebenginecoreglobal.h"

#include <QFlags>
#include <QRect>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QUrl>

QT_FORWARD_DECLARE_CLASS(QKeyEvent)
QT_FORWARD_DECLARE_CLASS(QVariant)
QT_FORWARD_DECLARE_CLASS(CertificateErrorController)

namespace QtWebEngineCore {

class AuthenticationDialogController;
class BrowserContextAdapter;
class FilePickerController;
class JavaScriptDialogController;
class RenderWidgetHostViewQt;
class RenderWidgetHostViewQtDelegate;
class RenderWidgetHostViewQtDelegateClient;
class WebContentsAdapter;
class WebContentsDelegateQt;
class WebEngineSettings;

// FIXME: make this ref-counted and implicitely shared and expose as public API maybe ?
class WebEngineContextMenuData {

public:
    WebEngineContextMenuData()
        : mediaType(MediaTypeNone)
        , hasImageContent(false)
        , mediaFlags(0)
    {
    }

    // Must match blink::WebContextMenuData::MediaType:
    enum MediaType {
        // No special node is in context.
        MediaTypeNone,
        // An image node is selected.
        MediaTypeImage,
        // A video node is selected.
        MediaTypeVideo,
        // An audio node is selected.
        MediaTypeAudio,
        // A canvas node is selected.
        MediaTypeCanvas,
        // A file node is selected.
        MediaTypeFile,
        // A plugin node is selected.
        MediaTypePlugin,
        MediaTypeLast = MediaTypePlugin
    };
    // Must match blink::WebContextMenuData::MediaFlags:
    enum MediaFlags {
        MediaNone = 0x0,
        MediaInError = 0x1,
        MediaPaused = 0x2,
        MediaMuted = 0x4,
        MediaLoop = 0x8,
        MediaCanSave = 0x10,
        MediaHasAudio = 0x20,
        MediaCanToggleControls = 0x40,
        MediaControls = 0x80,
        MediaCanPrint = 0x100,
        MediaCanRotate = 0x200,
    };

    QPoint pos;
    QUrl linkUrl;
    QString linkText;
    QString selectedText;
    QUrl mediaUrl;
    MediaType mediaType;
    bool hasImageContent;
    uint mediaFlags;
    QString suggestedFileName;
// Some likely candidates for future additions as we add support for the related actions:
//    bool isImageBlocked;
//    bool isEditable;
//    bool isSpellCheckingEnabled;
//    QStringList spellCheckingSuggestions;
//    <enum tbd> mediaType;
//    ...
};

class QWEBENGINE_EXPORT WebContentsAdapterClient {
public:
    // This must match window_open_disposition_list.h.
    enum WindowOpenDisposition {
        UnknownDisposition = 0,
        SuppressOpenDisposition = 1,
        CurrentTabDisposition = 2,
        SingletonTabDisposition = 3,
        NewForegroundTabDisposition = 4,
        NewBackgroundTabDisposition = 5,
        NewPopupDisposition = 6,
        NewWindowDisposition = 7,
        SaveToDiskDisposition = 8,
        OffTheRecordDisposition = 9,
        IgnoreActionDisposition = 10,
    };

    // Must match the values in javascript_message_type.h.
    enum JavascriptDialogType {
        AlertDialog,
        ConfirmDialog,
        PromptDialog,
        UnloadDialog,
        // Leave room for potential new specs
        InternalAuthorizationDialog = 0x10,
    };

    enum NavigationRequestAction {
        AcceptRequest,
        // Make room in the valid range of the enum for extra actions exposed in Experimental.
        IgnoreRequest = 0xFF
    };

    enum NavigationType {
        LinkNavigation,
        TypedNavigation,
        FormSubmittedNavigation,
        BackForwardNavigation,
        ReloadNavigation,
        OtherNavigation
    };

    enum JavaScriptConsoleMessageLevel {
        Info = 0,
        Warning,
        Error
    };

    enum RenderProcessTerminationStatus {
        NormalTerminationStatus = 0,
        AbnormalTerminationStatus,
        CrashedTerminationStatus,
        KilledTerminationStatus
    };

    enum MediaRequestFlag {
        MediaNone = 0,
        MediaAudioCapture = 0x01,
        MediaVideoCapture = 0x02,
    };
    Q_DECLARE_FLAGS(MediaRequestFlags, MediaRequestFlag)

    virtual ~WebContentsAdapterClient() { }

    virtual RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegate(RenderWidgetHostViewQtDelegateClient *client) = 0;
    virtual RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegateForPopup(RenderWidgetHostViewQtDelegateClient *client) = 0;
    virtual void titleChanged(const QString&) = 0;
    virtual void urlChanged(const QUrl&) = 0;
    virtual void iconChanged(const QUrl&) = 0;
    virtual void loadProgressChanged(int progress) = 0;
    virtual void didUpdateTargetURL(const QUrl&) = 0;
    virtual void selectionChanged() = 0;
    virtual QRectF viewportRect() const = 0;
    virtual qreal dpiScale() const = 0;
    virtual QColor backgroundColor() const = 0;
    virtual void loadStarted(const QUrl &provisionalUrl, bool isErrorPage = false) = 0;
    virtual void loadCommitted() = 0;
    virtual void loadVisuallyCommitted() = 0;
    virtual void loadFinished(bool success, const QUrl &url, bool isErrorPage = false, int errorCode = 0, const QString &errorDescription = QString()) = 0;
    virtual void focusContainer() = 0;
    virtual void unhandledKeyEvent(QKeyEvent *event) = 0;
    virtual void adoptNewWindow(WebContentsAdapter *newWebContents, WindowOpenDisposition disposition, bool userGesture, const QRect & initialGeometry) = 0;
    virtual bool isBeingAdopted() = 0;
    virtual void close() = 0;
    virtual void windowCloseRejected() = 0;
    virtual bool contextMenuRequested(const WebEngineContextMenuData&) = 0;
    virtual void navigationRequested(int navigationType, const QUrl &url, int &navigationRequestAction, bool isMainFrame) = 0;
    virtual void requestFullScreenMode(const QUrl &origin, bool fullscreen) = 0;
    virtual bool isFullScreenMode() const = 0;
    virtual void javascriptDialog(QSharedPointer<JavaScriptDialogController>) = 0;
    virtual void runFileChooser(FilePickerController *controller) = 0;
    virtual void didRunJavaScript(quint64 requestId, const QVariant& result) = 0;
    virtual void didFetchDocumentMarkup(quint64 requestId, const QString& result) = 0;
    virtual void didFetchDocumentInnerText(quint64 requestId, const QString& result) = 0;
    virtual void didFindText(quint64 requestId, int matchCount) = 0;
    virtual void passOnFocus(bool reverse) = 0;
    // returns the last QObject (QWidget/QQuickItem) based object in the accessibility
    // hierarchy before going into the BrowserAccessibility tree
#ifndef QT_NO_ACCESSIBILITY
    virtual QObject *accessibilityParentObject() = 0;
#endif // QT_NO_ACCESSIBILITY
    virtual void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID) = 0;
    virtual void authenticationRequired(QSharedPointer<AuthenticationDialogController>) = 0;
    virtual void runGeolocationPermissionRequest(const QUrl &securityOrigin) = 0;
    virtual void runMediaAccessPermissionRequest(const QUrl &securityOrigin, MediaRequestFlags requestFlags) = 0;
    virtual void runMouseLockPermissionRequest(const QUrl &securityOrigin) = 0;
    virtual WebEngineSettings *webEngineSettings() const = 0;
    virtual void showValidationMessage(const QRect &anchor, const QString &mainText, const QString &subText) = 0;
    virtual void hideValidationMessage() = 0;
    virtual void moveValidationMessage(const QRect &anchor) = 0;
    RenderProcessTerminationStatus renderProcessExitStatus(int);
    virtual void renderProcessTerminated(RenderProcessTerminationStatus terminationStatus, int exitCode) = 0;
    virtual void requestGeometryChange(const QRect &geometry) = 0;
    virtual void allowCertificateError(const QSharedPointer<CertificateErrorController> &errorController) = 0;

    virtual QSharedPointer<BrowserContextAdapter> browserContextAdapter() = 0;

};

} // namespace QtWebEngineCore

#endif // WEB_CONTENTS_ADAPTER_CLIENT_H
