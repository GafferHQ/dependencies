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

#include "render_widget_host_view_qt.h"

#include "common/qt_messages.h"
#include "browser_accessibility_manager_qt.h"
#include "browser_accessibility_qt.h"
#include "chromium_overrides.h"
#include "delegated_frame_node.h"
#include "qtwebenginecoreglobal_p.h"
#include "render_widget_host_view_qt_delegate.h"
#include "type_conversion.h"
#include "web_contents_adapter.h"
#include "web_contents_adapter_client.h"
#include "web_event_factory.h"

#include "base/command_line.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/cursors/webcursor.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/geometry/size_conversions.h"

#include <QEvent>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QTextFormat>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QScreen>
#include <QStyleHints>
#include <QVariant>
#include <QWheelEvent>
#include <QWindow>
#include <QtGui/qaccessible.h>

namespace QtWebEngineCore {

static inline ui::LatencyInfo CreateLatencyInfo(const blink::WebInputEvent& event) {
  ui::LatencyInfo latency_info;
  // The latency number should only be added if the timestamp is valid.
  if (event.timeStampSeconds) {
    const int64 time_micros = static_cast<int64>(
        event.timeStampSeconds * base::Time::kMicrosecondsPerSecond);
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
        0,
        0,
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(time_micros),
        1);
  }
  return latency_info;
}

static inline Qt::InputMethodHints toQtInputMethodHints(ui::TextInputType inputType)
{
    switch (inputType) {
    case ui::TEXT_INPUT_TYPE_TEXT:
        return Qt::ImhPreferLowercase;
    case ui::TEXT_INPUT_TYPE_SEARCH:
        return Qt::ImhPreferLowercase | Qt::ImhNoAutoUppercase;
    case ui::TEXT_INPUT_TYPE_PASSWORD:
        return Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase;
    case ui::TEXT_INPUT_TYPE_EMAIL:
        return Qt::ImhEmailCharactersOnly;
    case ui::TEXT_INPUT_TYPE_NUMBER:
        return Qt::ImhFormattedNumbersOnly;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
        return Qt::ImhDialableCharactersOnly;
    case ui::TEXT_INPUT_TYPE_URL:
        return Qt::ImhUrlCharactersOnly | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase;
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
        return Qt::ImhDate | Qt::ImhTime;
    case ui::TEXT_INPUT_TYPE_DATE:
    case ui::TEXT_INPUT_TYPE_MONTH:
    case ui::TEXT_INPUT_TYPE_WEEK:
        return Qt::ImhDate;
    case ui::TEXT_INPUT_TYPE_TIME:
        return Qt::ImhTime;
    case ui::TEXT_INPUT_TYPE_TEXT_AREA:
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
        return Qt::ImhMultiLine | Qt::ImhPreferLowercase;
    default:
        return Qt::ImhNone;
    }
}

static inline int firstAvailableId(const QMap<int, int> &map)
{
    ui::BitSet32 usedIds;
    QMap<int, int>::const_iterator end = map.end();
    for (QMap<int, int>::const_iterator it = map.begin(); it != end; ++it)
        usedIds.mark_bit(it.value());
    return usedIds.first_unmarked_bit();
}

static inline ui::GestureProvider::Config QtGestureProviderConfig() {
    ui::GestureProvider::Config config = ui::GetGestureProviderConfig(ui::GestureProviderConfigType::CURRENT_PLATFORM);
    // Causes an assert in CreateWebGestureEventFromGestureEventData and we don't need them in Qt.
    config.gesture_begin_end_types_enabled = false;
    config.gesture_detector_config.swipe_enabled = false;
    config.gesture_detector_config.two_finger_tap_enabled = false;
    return config;
}

static inline bool compareTouchPoints(const QTouchEvent::TouchPoint &lhs, const QTouchEvent::TouchPoint &rhs)
{
    // TouchPointPressed < TouchPointMoved < TouchPointReleased
    return lhs.state() < rhs.state();
}

static inline int flagsFromModifiers(Qt::KeyboardModifiers modifiers)
{
    int modifierFlags = ui::EF_NONE;
#if defined(Q_OS_OSX)
    if (!qApp->testAttribute(Qt::AA_MacDontSwapCtrlAndMeta)) {
        if ((modifiers & Qt::ControlModifier) != 0)
            modifierFlags |= ui::EF_COMMAND_DOWN;
        if ((modifiers & Qt::MetaModifier) != 0)
            modifierFlags |= ui::EF_CONTROL_DOWN;
    } else
#endif
    {
        if ((modifiers & Qt::ControlModifier) != 0)
            modifierFlags |= ui::EF_CONTROL_DOWN;
        if ((modifiers & Qt::MetaModifier) != 0)
            modifierFlags |= ui::EF_COMMAND_DOWN;
    }
    if ((modifiers & Qt::ShiftModifier) != 0)
        modifierFlags |= ui::EF_SHIFT_DOWN;
    if ((modifiers & Qt::AltModifier) != 0)
        modifierFlags |= ui::EF_ALT_DOWN;
    return modifierFlags;
}

static uint32 s_eventId = 0;
class MotionEventQt : public ui::MotionEvent {
public:
    MotionEventQt(const QList<QTouchEvent::TouchPoint> &touchPoints, const base::TimeTicks &eventTime, Action action, const Qt::KeyboardModifiers modifiers, float dpiScale, int index = -1)
        : touchPoints(touchPoints)
        , eventTime(eventTime)
        , action(action)
        , eventId(++s_eventId)
        , flags(flagsFromModifiers(modifiers))
        , index(index)
        , dpiScale(dpiScale)
    {
        // ACTION_DOWN and ACTION_UP must be accesssed through pointer_index 0
        Q_ASSERT((action != ACTION_DOWN && action != ACTION_UP) || index == 0);
    }

    virtual uint32 GetUniqueEventId() const Q_DECL_OVERRIDE { return eventId; }
    virtual Action GetAction() const Q_DECL_OVERRIDE { return action; }
    virtual int GetActionIndex() const Q_DECL_OVERRIDE { return index; }
    virtual size_t GetPointerCount() const Q_DECL_OVERRIDE { return touchPoints.size(); }
    virtual int GetPointerId(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).id(); }
    virtual float GetX(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).pos().x() / dpiScale; }
    virtual float GetY(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).pos().y() / dpiScale; }
    virtual float GetRawX(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).screenPos().x(); }
    virtual float GetRawY(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).screenPos().y(); }
    virtual float GetTouchMajor(size_t pointer_index) const Q_DECL_OVERRIDE
    {
        QRectF touchRect = touchPoints.at(pointer_index).rect();
        return std::max(touchRect.height(), touchRect.width());
    }
    virtual float GetTouchMinor(size_t pointer_index) const Q_DECL_OVERRIDE
    {
        QRectF touchRect = touchPoints.at(pointer_index).rect();
        return std::min(touchRect.height(), touchRect.width());
    }
    virtual float GetOrientation(size_t pointer_index) const Q_DECL_OVERRIDE
    {
        return 0;
    }
    virtual int GetFlags() const Q_DECL_OVERRIDE { return flags; }
    virtual float GetPressure(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).pressure(); }
    virtual base::TimeTicks GetEventTime() const Q_DECL_OVERRIDE { return eventTime; }

    virtual size_t GetHistorySize() const Q_DECL_OVERRIDE { return 0; }
    virtual base::TimeTicks GetHistoricalEventTime(size_t historical_index) const Q_DECL_OVERRIDE { return base::TimeTicks(); }
    virtual float GetHistoricalTouchMajor(size_t pointer_index, size_t historical_index) const Q_DECL_OVERRIDE { return 0; }
    virtual float GetHistoricalX(size_t pointer_index, size_t historical_index) const Q_DECL_OVERRIDE { return 0; }
    virtual float GetHistoricalY(size_t pointer_index, size_t historical_index) const Q_DECL_OVERRIDE { return 0; }
    virtual ToolType GetToolType(size_t pointer_index) const Q_DECL_OVERRIDE { return ui::MotionEvent::TOOL_TYPE_UNKNOWN; }
    virtual int GetButtonState() const Q_DECL_OVERRIDE { return 0; }

private:
    QList<QTouchEvent::TouchPoint> touchPoints;
    base::TimeTicks eventTime;
    Action action;
    const uint32 eventId;
    int flags;
    int index;
    float dpiScale;
};

RenderWidgetHostViewQt::RenderWidgetHostViewQt(content::RenderWidgetHost* widget)
    : m_host(content::RenderWidgetHostImpl::From(widget))
    , m_gestureProvider(QtGestureProviderConfig(), this)
    , m_sendMotionActionDown(false)
    , m_touchMotionStarted(false)
    , m_chromiumCompositorData(new ChromiumCompositorData)
    , m_needsDelegatedFrameAck(false)
    , m_didFirstVisuallyNonEmptyLayout(false)
    , m_adapterClient(0)
    , m_imeInProgress(false)
    , m_anchorPositionWithinSelection(0)
    , m_cursorPositionWithinSelection(0)
    , m_initPending(false)
{
    m_host->SetView(this);
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::installActivationObserver(this);
    if (QAccessible::isActive())
        content::BrowserAccessibilityStateImpl::GetInstance()->EnableAccessibility();
#endif // QT_NO_ACCESSIBILITY
}

RenderWidgetHostViewQt::~RenderWidgetHostViewQt()
{
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::removeActivationObserver(this);
#endif // QT_NO_ACCESSIBILITY
}

void RenderWidgetHostViewQt::setDelegate(RenderWidgetHostViewQtDelegate* delegate)
{
    m_delegate.reset(delegate);
}

void RenderWidgetHostViewQt::setAdapterClient(WebContentsAdapterClient *adapterClient)
{
    Q_ASSERT(!m_adapterClient);

    m_adapterClient = adapterClient;
    if (m_initPending)
        InitAsChild(0);
}

void RenderWidgetHostViewQt::InitAsChild(gfx::NativeView)
{
    if (!m_adapterClient) {
        m_initPending = true;
        return;
    }
    m_initPending = false;
    m_delegate->initAsChild(m_adapterClient);
}

void RenderWidgetHostViewQt::InitAsPopup(content::RenderWidgetHostView*, const gfx::Rect& rect)
{
    m_delegate->initAsPopup(toQt(rect));
}

void RenderWidgetHostViewQt::InitAsFullscreen(content::RenderWidgetHostView*)
{
}

content::RenderWidgetHost* RenderWidgetHostViewQt::GetRenderWidgetHost() const
{
    return m_host;
}

void RenderWidgetHostViewQt::SetSize(const gfx::Size& size)
{
    int width = size.width();
    int height = size.height();

    m_delegate->resize(width,height);
}

void RenderWidgetHostViewQt::SetBounds(const gfx::Rect& screenRect)
{
    // This is called when webkit has sent us a Move message.
    if (IsPopup())
        m_delegate->move(toQt(screenRect.origin()));
    SetSize(screenRect.size());
}

gfx::Vector2dF RenderWidgetHostViewQt::GetLastScrollOffset() const {
    return m_lastScrollOffset;
}

gfx::Size RenderWidgetHostViewQt::GetPhysicalBackingSize() const
{
    if (!m_delegate || !m_delegate->window() || !m_delegate->window()->screen())
        return gfx::Size();

    const QScreen* screen = m_delegate->window()->screen();
    gfx::SizeF size = toGfx(m_delegate->screenRect().size());
    return gfx::ToCeiledSize(gfx::ScaleSize(size, screen->devicePixelRatio()));
}

gfx::NativeView RenderWidgetHostViewQt::GetNativeView() const
{
    // gfx::NativeView is a typedef to a platform specific view
    // pointer (HWND, NSView*, GtkWidget*) and other ports use
    // this function in the renderer_host layer when setting up
    // the view hierarchy and for generating snapshots in tests.
    // Since we manage the view hierarchy in Qt its value hasn't
    // been meaningful.
    return gfx::NativeView();
}

gfx::NativeViewId RenderWidgetHostViewQt::GetNativeViewId() const
{
    return 0;
}

gfx::NativeViewAccessible RenderWidgetHostViewQt::GetNativeViewAccessible()
{
    return 0;
}

content::BrowserAccessibilityManager* RenderWidgetHostViewQt::CreateBrowserAccessibilityManager(content::BrowserAccessibilityDelegate* delegate)
{
#ifndef QT_NO_ACCESSIBILITY
    return new content::BrowserAccessibilityManagerQt(
        m_adapterClient->accessibilityParentObject(),
        content::BrowserAccessibilityManagerQt::GetEmptyDocument(),
        delegate);
#else
    return 0;
#endif // QT_NO_ACCESSIBILITY
}

// Set focus to the associated View component.
void RenderWidgetHostViewQt::Focus()
{
    m_host->SetInputMethodActive(true);
    if (!IsPopup())
        m_delegate->setKeyboardFocus();
    m_host->Focus();
}

bool RenderWidgetHostViewQt::HasFocus() const
{
    return m_delegate->hasKeyboardFocus();
}

bool RenderWidgetHostViewQt::IsSurfaceAvailableForCopy() const
{
    return true;
}

void RenderWidgetHostViewQt::Show()
{
    m_delegate->show();
}

void RenderWidgetHostViewQt::Hide()
{
    m_delegate->hide();
}

bool RenderWidgetHostViewQt::IsShowing()
{
    return m_delegate->isVisible();
}

// Retrieve the bounds of the View, in screen coordinates.
gfx::Rect RenderWidgetHostViewQt::GetViewBounds() const
{
    QRectF p = m_delegate->contentsRect();
    float s = dpiScale();
    gfx::Point p1(floor(p.x() / s), floor(p.y() / s));
    gfx::Point p2(ceil(p.right() /s), ceil(p.bottom() / s));
    return gfx::BoundingRect(p1, p2);
}

void RenderWidgetHostViewQt::SetBackgroundColor(SkColor color)
{
    RenderWidgetHostViewBase::SetBackgroundColor(color);
    // Set the background of the compositor if necessary
    m_delegate->setClearColor(toQt(color));
    // Set the background of the blink::FrameView
    m_host->Send(new RenderViewObserverQt_SetBackgroundColor(m_host->GetRoutingID(), color));
}

// Return value indicates whether the mouse is locked successfully or not.
bool RenderWidgetHostViewQt::LockMouse()
{
    mouse_locked_ = true;
    m_lockedMousePosition = QCursor::pos();
    m_delegate->lockMouse();
    qApp->setOverrideCursor(Qt::BlankCursor);
    return true;
}

void RenderWidgetHostViewQt::UnlockMouse()
{
    mouse_locked_ = false;
    m_delegate->unlockMouse();
    qApp->restoreOverrideCursor();
    m_host->LostMouseLock();
}

void RenderWidgetHostViewQt::MovePluginWindows(const std::vector<content::WebPluginGeometry>&)
{
    // QT_NOT_YET_IMPLEMENTED
}

void RenderWidgetHostViewQt::UpdateCursor(const content::WebCursor &webCursor)
{
    content::WebCursor::CursorInfo cursorInfo;
    webCursor.GetCursorInfo(&cursorInfo);
    Qt::CursorShape shape;
    switch (cursorInfo.type) {
    case blink::WebCursorInfo::TypePointer:
        shape = Qt::ArrowCursor;
        break;
    case blink::WebCursorInfo::TypeCross:
        shape = Qt::CrossCursor;
        break;
    case blink::WebCursorInfo::TypeHand:
        shape = Qt::PointingHandCursor;
        break;
    case blink::WebCursorInfo::TypeIBeam:
        shape = Qt::IBeamCursor;
        break;
    case blink::WebCursorInfo::TypeWait:
        shape = Qt::WaitCursor;
        break;
    case blink::WebCursorInfo::TypeHelp:
        shape = Qt::WhatsThisCursor;
        break;
    case blink::WebCursorInfo::TypeEastResize:
    case blink::WebCursorInfo::TypeWestResize:
    case blink::WebCursorInfo::TypeEastWestResize:
    case blink::WebCursorInfo::TypeEastPanning:
    case blink::WebCursorInfo::TypeWestPanning:
        shape = Qt::SizeHorCursor;
        break;
    case blink::WebCursorInfo::TypeNorthResize:
    case blink::WebCursorInfo::TypeSouthResize:
    case blink::WebCursorInfo::TypeNorthSouthResize:
    case blink::WebCursorInfo::TypeNorthPanning:
    case blink::WebCursorInfo::TypeSouthPanning:
        shape = Qt::SizeVerCursor;
        break;
    case blink::WebCursorInfo::TypeNorthEastResize:
    case blink::WebCursorInfo::TypeSouthWestResize:
    case blink::WebCursorInfo::TypeNorthEastSouthWestResize:
    case blink::WebCursorInfo::TypeNorthEastPanning:
    case blink::WebCursorInfo::TypeSouthWestPanning:
        shape = Qt::SizeBDiagCursor;
        break;
    case blink::WebCursorInfo::TypeNorthWestResize:
    case blink::WebCursorInfo::TypeSouthEastResize:
    case blink::WebCursorInfo::TypeNorthWestSouthEastResize:
    case blink::WebCursorInfo::TypeNorthWestPanning:
    case blink::WebCursorInfo::TypeSouthEastPanning:
        shape = Qt::SizeFDiagCursor;
        break;
    case blink::WebCursorInfo::TypeColumnResize:
        shape = Qt::SplitHCursor;
        break;
    case blink::WebCursorInfo::TypeRowResize:
        shape = Qt::SplitVCursor;
        break;
    case blink::WebCursorInfo::TypeMiddlePanning:
    case blink::WebCursorInfo::TypeMove:
        shape = Qt::SizeAllCursor;
        break;
    case blink::WebCursorInfo::TypeVerticalText:
    case blink::WebCursorInfo::TypeCell:
    case blink::WebCursorInfo::TypeContextMenu:
    case blink::WebCursorInfo::TypeAlias:
    case blink::WebCursorInfo::TypeProgress:
    case blink::WebCursorInfo::TypeCopy:
    case blink::WebCursorInfo::TypeZoomIn:
    case blink::WebCursorInfo::TypeZoomOut:
        // FIXME: Load from the resource bundle.
        shape = Qt::ArrowCursor;
        break;
    case blink::WebCursorInfo::TypeNoDrop:
    case blink::WebCursorInfo::TypeNotAllowed:
        shape = Qt::ForbiddenCursor;
        break;
    case blink::WebCursorInfo::TypeNone:
        shape = Qt::BlankCursor;
        break;
    case blink::WebCursorInfo::TypeGrab:
        shape = Qt::OpenHandCursor;
        break;
    case blink::WebCursorInfo::TypeGrabbing:
        shape = Qt::ClosedHandCursor;
        break;
    case blink::WebCursorInfo::TypeCustom:
        if (cursorInfo.custom_image.colorType() == SkColorType::kN32_SkColorType) {
            QImage cursor = toQImage(cursorInfo.custom_image, QImage::Format_ARGB32);
            m_delegate->updateCursor(QCursor(QPixmap::fromImage(cursor)));
            return;
        }
        // Use arrow cursor as fallback in case the Chromium implementation changes.
        shape = Qt::ArrowCursor;
        break;
    default:
        Q_UNREACHABLE();
        shape = Qt::ArrowCursor;
    }
    m_delegate->updateCursor(QCursor(shape));
}

void RenderWidgetHostViewQt::SetIsLoading(bool)
{
    // We use WebContentsDelegateQt::LoadingStateChanged to notify about loading state.
}

void RenderWidgetHostViewQt::TextInputTypeChanged(ui::TextInputType type, ui::TextInputMode mode, bool can_compose_inline, int flags)
{
    Q_UNUSED(mode);
    Q_UNUSED(can_compose_inline);
    Q_UNUSED(flags);
    m_currentInputType = type;
    m_delegate->inputMethodStateChanged(static_cast<bool>(type));
}

void RenderWidgetHostViewQt::ImeCancelComposition()
{
    qApp->inputMethod()->reset();
}

void RenderWidgetHostViewQt::ImeCompositionRangeChanged(const gfx::Range&, const std::vector<gfx::Rect>&)
{
    // FIXME: not implemented?
    QT_NOT_YET_IMPLEMENTED
}

void RenderWidgetHostViewQt::RenderProcessGone(base::TerminationStatus terminationStatus,
                                               int exitCode)
{
    m_adapterClient->renderProcessTerminated(
                m_adapterClient->renderProcessExitStatus(terminationStatus), exitCode);
    Destroy();
}

void RenderWidgetHostViewQt::Destroy()
{
    delete this;
}

void RenderWidgetHostViewQt::SetTooltipText(const base::string16 &tooltip_text)
{
    m_delegate->setTooltip(toQt(tooltip_text));
}

void RenderWidgetHostViewQt::SelectionBoundsChanged(const ViewHostMsg_SelectionBounds_Params &params)
{
    if (selection_range_.IsValid()) {
        if (params.is_anchor_first) {
            m_anchorPositionWithinSelection = selection_range_.GetMin() - selection_text_offset_;
            m_cursorPositionWithinSelection = selection_range_.GetMax() - selection_text_offset_;
        } else {
            m_anchorPositionWithinSelection = selection_range_.GetMax() - selection_text_offset_;
            m_cursorPositionWithinSelection = selection_range_.GetMin() - selection_text_offset_;
        }
    }

    gfx::Rect caretRect = gfx::UnionRects(params.anchor_rect, params.focus_rect);
    m_cursorRect = QRect(caretRect.x(), caretRect.y(), caretRect.width(), caretRect.height());
}

void RenderWidgetHostViewQt::CopyFromCompositingSurface(const gfx::Rect& src_subrect, const gfx::Size& dst_size, content::ReadbackRequestCallback& callback, const SkColorType color_type)
{
    NOTIMPLEMENTED();
    Q_UNUSED(src_subrect);
    Q_UNUSED(dst_size);
    Q_UNUSED(color_type);
    callback.Run(SkBitmap(), content::READBACK_FAILED);
}

void RenderWidgetHostViewQt::CopyFromCompositingSurfaceToVideoFrame(const gfx::Rect& src_subrect, const scoped_refptr<media::VideoFrame>& target, const base::Callback<void(bool)>& callback)
{
    NOTIMPLEMENTED();
    callback.Run(false);
}

bool RenderWidgetHostViewQt::CanCopyToVideoFrame() const
{
    return false;
}

bool RenderWidgetHostViewQt::HasAcceleratedSurface(const gfx::Size&)
{
    return false;
}

void RenderWidgetHostViewQt::OnSwapCompositorFrame(uint32 output_surface_id, scoped_ptr<cc::CompositorFrame> frame)
{
    m_lastScrollOffset = frame->metadata.root_scroll_offset;
    Q_ASSERT(!m_needsDelegatedFrameAck);
    m_needsDelegatedFrameAck = true;
    m_pendingOutputSurfaceId = output_surface_id;
    Q_ASSERT(frame->delegated_frame_data);
    Q_ASSERT(!m_chromiumCompositorData->frameData || m_chromiumCompositorData->frameData->resource_list.empty());
    m_chromiumCompositorData->frameData = frame->delegated_frame_data.Pass();
    m_chromiumCompositorData->frameDevicePixelRatio = frame->metadata.device_scale_factor;

    // Support experimental.viewport.devicePixelRatio, see GetScreenInfo implementation below.
    float dpiScale = this->dpiScale();
    if (dpiScale != 0 && dpiScale != 1)
        m_chromiumCompositorData->frameDevicePixelRatio /= dpiScale;

    m_delegate->update();

    if (m_didFirstVisuallyNonEmptyLayout) {
        m_adapterClient->loadVisuallyCommitted();
        m_didFirstVisuallyNonEmptyLayout = false;
    }
}

void RenderWidgetHostViewQt::GetScreenInfo(blink::WebScreenInfo* results)
{
    QWindow* window = m_delegate->window();
    if (!window)
        return;
    GetScreenInfoFromNativeWindow(window, results);

    // Support experimental.viewport.devicePixelRatio
    results->deviceScaleFactor *= dpiScale();
}

gfx::Rect RenderWidgetHostViewQt::GetBoundsInRootWindow()
{
    if (!m_delegate->window())
        return gfx::Rect();

    QRect r = m_delegate->window()->frameGeometry();
    return gfx::Rect(r.x(), r.y(), r.width(), r.height());
}

gfx::GLSurfaceHandle RenderWidgetHostViewQt::GetCompositingSurface()
{
    return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NULL_TRANSPORT);
}

void RenderWidgetHostViewQt::SelectionChanged(const base::string16 &text, size_t offset, const gfx::Range &range)
{
    content::RenderWidgetHostViewBase::SelectionChanged(text, offset, range);
    m_adapterClient->selectionChanged();

#if defined(USE_X11)
    // Set the CLIPBOARD_TYPE_SELECTION to the ui::Clipboard.
    ui::ScopedClipboardWriter clipboard_writer(ui::CLIPBOARD_TYPE_SELECTION);
    clipboard_writer.WriteText(text);
#endif
}

void RenderWidgetHostViewQt::OnGestureEvent(const ui::GestureEventData& gesture)
{
    m_host->ForwardGestureEvent(ui::CreateWebGestureEventFromGestureEventData(gesture));
}

QSGNode *RenderWidgetHostViewQt::updatePaintNode(QSGNode *oldNode)
{
    DelegatedFrameNode *frameNode = static_cast<DelegatedFrameNode *>(oldNode);
    if (!frameNode)
        frameNode = new DelegatedFrameNode;

    frameNode->commit(m_chromiumCompositorData.data(), &m_resourcesToRelease, m_delegate.get());

    // This is possibly called from the Qt render thread, post the ack back to the UI
    // to tell the child compositors to release resources and trigger a new frame.
    if (m_needsDelegatedFrameAck) {
        m_needsDelegatedFrameAck = false;
        content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
            base::Bind(&RenderWidgetHostViewQt::sendDelegatedFrameAck, AsWeakPtr()));
    }

    return frameNode;
}

void RenderWidgetHostViewQt::notifyResize()
{
    m_host->WasResized();
}

void RenderWidgetHostViewQt::notifyShown()
{
    m_host->WasShown(ui::LatencyInfo());
}

void RenderWidgetHostViewQt::notifyHidden()
{
    m_host->WasHidden();
}

void RenderWidgetHostViewQt::windowBoundsChanged()
{
    m_host->SendScreenRects();
    if (m_delegate->window())
        m_host->NotifyScreenInfoChanged();
}

void RenderWidgetHostViewQt::windowChanged()
{
    if (m_delegate->window())
        m_host->NotifyScreenInfoChanged();
}

bool RenderWidgetHostViewQt::forwardEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        Focus(); // Fall through.
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        // Skip second MouseMove event when a window is being adopted, so that Chromium
        // can properly handle further move events.
        if (m_adapterClient->isBeingAdopted())
            return false;
        handleMouseEvent(static_cast<QMouseEvent*>(event));
        break;
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        handleKeyEvent(static_cast<QKeyEvent*>(event));
        break;
    case QEvent::Wheel:
        handleWheelEvent(static_cast<QWheelEvent*>(event));
        break;
    case QEvent::TouchBegin:
        Focus(); // Fall through.
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        handleTouchEvent(static_cast<QTouchEvent*>(event));
        break;
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::HoverMove:
        handleHoverEvent(static_cast<QHoverEvent*>(event));
        break;
    case QEvent::FocusIn:
    case QEvent::FocusOut:
        handleFocusEvent(static_cast<QFocusEvent*>(event));
        break;
    case QEvent::InputMethod:
        handleInputMethodEvent(static_cast<QInputMethodEvent*>(event));
        break;
    default:
        return false;
    }
    return true;
}

QVariant RenderWidgetHostViewQt::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return QVariant(m_currentInputType != ui::TEXT_INPUT_TYPE_NONE);
    case Qt::ImCursorRectangle:
        return m_cursorRect;
    case Qt::ImFont:
        return QVariant();
    case Qt::ImCursorPosition:
        return static_cast<uint>(m_cursorPositionWithinSelection);
    case Qt::ImAnchorPosition:
        return static_cast<uint>(m_anchorPositionWithinSelection);
    case Qt::ImSurroundingText:
        return toQt(selection_text_);
    case Qt::ImCurrentSelection:
        return toQt(GetSelectedText());
    case Qt::ImMaximumTextLength:
        return QVariant(); // No limit.
    case Qt::ImHints:
        return int(toQtInputMethodHints(m_currentInputType));
    default:
        return QVariant();
    }
}

void RenderWidgetHostViewQt::ProcessAckedTouchEvent(const content::TouchEventWithLatencyInfo &touch, content::InputEventAckState ack_result) {
    Q_UNUSED(touch);
    const bool eventConsumed = ack_result == content::INPUT_EVENT_ACK_STATE_CONSUMED;
    m_gestureProvider.OnTouchEventAck(touch.event.uniqueTouchEventId, eventConsumed);
}

void RenderWidgetHostViewQt::sendDelegatedFrameAck()
{
    cc::CompositorFrameAck ack;
    m_resourcesToRelease.swap(ack.resources);
    content::RenderWidgetHostImpl::SendSwapCompositorFrameAck(
        m_host->GetRoutingID(), m_pendingOutputSurfaceId,
        m_host->GetProcess()->GetID(), ack);
}

void RenderWidgetHostViewQt::processMotionEvent(const ui::MotionEvent &motionEvent)
{
    if (!m_gestureProvider.OnTouchEvent(motionEvent).succeeded)
        return;

    bool causesScrollingIfUncancelled = true;
    blink::WebTouchEvent touchEvent = ui::CreateWebTouchEventFromMotionEvent(motionEvent,
                                                                             causesScrollingIfUncancelled);
    m_host->ForwardTouchEventWithLatencyInfo(touchEvent, CreateLatencyInfo(touchEvent));
}

QList<QTouchEvent::TouchPoint> RenderWidgetHostViewQt::mapTouchPointIds(const QList<QTouchEvent::TouchPoint> &inputPoints)
{
    QList<QTouchEvent::TouchPoint> outputPoints = inputPoints;
    for (int i = 0; i < outputPoints.size(); ++i) {
        QTouchEvent::TouchPoint &point = outputPoints[i];

        int qtId = point.id();
        QMap<int, int>::const_iterator it = m_touchIdMapping.find(qtId);
        if (it == m_touchIdMapping.end())
            it = m_touchIdMapping.insert(qtId, firstAvailableId(m_touchIdMapping));
        point.setId(it.value());

        if (point.state() == Qt::TouchPointReleased)
            m_touchIdMapping.remove(qtId);
    }

    return outputPoints;
}

float RenderWidgetHostViewQt::dpiScale() const
{
    return m_adapterClient ? m_adapterClient->dpiScale() : 1.0;
}

bool RenderWidgetHostViewQt::IsPopup() const
{
    return popup_type_ != blink::WebPopupTypeNone;
}

void RenderWidgetHostViewQt::handleMouseEvent(QMouseEvent* event)
{
    blink::WebMouseEvent webEvent = WebEventFactory::toWebMouseEvent(event, dpiScale());
    if (event->type() == QMouseEvent::MouseButtonPress) {
        if (event->button() != m_clickHelper.lastPressButton
            || (event->timestamp() - m_clickHelper.lastPressTimestamp > static_cast<ulong>(qGuiApp->styleHints()->mouseDoubleClickInterval()))
            || (event->pos() - m_clickHelper.lastPressPosition).manhattanLength() > qGuiApp->styleHints()->startDragDistance())
            m_clickHelper.clickCounter = 0;

        m_clickHelper.lastPressTimestamp = event->timestamp();
        webEvent.clickCount = ++m_clickHelper.clickCounter;
        m_clickHelper.lastPressButton = event->button();
        m_clickHelper.lastPressPosition = QPointF(event->pos()).toPoint();
    }

    if (IsMouseLocked()) {
        webEvent.movementX = -(m_lockedMousePosition.x() - event->globalX());
        webEvent.movementY = -(m_lockedMousePosition.y() - event->globalY());
        QCursor::setPos(m_lockedMousePosition);
    }

    m_host->ForwardMouseEvent(webEvent);
}

void RenderWidgetHostViewQt::handleKeyEvent(QKeyEvent *ev)
{
    if (IsMouseLocked() && ev->key() == Qt::Key_Escape && ev->type() == QEvent::KeyRelease)
        UnlockMouse();

    if (m_imeInProgress) {
        // IME composition was not finished with a valid commit string.
        // We're getting the composition result in a key event.
        if (ev->key() != 0) {
            // The key event is not a result of an IME composition. Cancel IME.
            m_host->ImeCancelComposition();
            m_imeInProgress = false;
        } else {
            if (ev->type() == QEvent::KeyRelease) {
                m_host->ImeConfirmComposition(toString16(ev->text()), gfx::Range::InvalidRange(),
                                              false);
                m_imeInProgress = false;
            }
            return;
        }
    }

    content::NativeWebKeyboardEvent webEvent = WebEventFactory::toWebKeyboardEvent(ev);
    if (webEvent.type == blink::WebInputEvent::RawKeyDown && !ev->text().isEmpty()) {
        // Blink won't consume the RawKeyDown, but rather the Char event in this case.
        // Make sure to skip the former on the way back. The same os_event will be set on both of them.
        webEvent.skip_in_browser = true;
        m_host->ForwardKeyboardEvent(webEvent);

        webEvent.skip_in_browser = false;
        webEvent.type = blink::WebInputEvent::Char;
        m_host->ForwardKeyboardEvent(webEvent);
    } else {
        m_host->ForwardKeyboardEvent(webEvent);
    }
}

void RenderWidgetHostViewQt::handleInputMethodEvent(QInputMethodEvent *ev)
{
    if (!m_host)
        return;

    QString commitString = ev->commitString();
    QString preeditString = ev->preeditString();

    int replacementStart = ev->replacementStart();
    int replacementLength = ev->replacementLength();

    int cursorPositionInPreeditString = -1;
    gfx::Range selectionRange = gfx::Range::InvalidRange();

    const QList<QInputMethodEvent::Attribute> &attributes = ev->attributes();
    std::vector<blink::WebCompositionUnderline> underlines;

    Q_FOREACH (const QInputMethodEvent::Attribute &attribute, attributes) {
        switch (attribute.type) {
        case QInputMethodEvent::TextFormat: {
            if (preeditString.isEmpty())
                break;

            int start = qMin(attribute.start, (attribute.start + attribute.length));
            int end = qMax(attribute.start, (attribute.start + attribute.length));
            underlines.push_back(blink::WebCompositionUnderline(start, end, /*color*/ SK_ColorBLACK, /*thick*/ false, /*backgroundColor*/ SK_ColorTRANSPARENT));
            break;
        }
        case QInputMethodEvent::Cursor:
            if (attribute.length)
                cursorPositionInPreeditString = attribute.start;
            break;
        case QInputMethodEvent::Selection:
            selectionRange.set_start(qMin(attribute.start, (attribute.start + attribute.length)));
            selectionRange.set_end(qMax(attribute.start, (attribute.start + attribute.length)));
            break;
        default:
            break;
        }
    }

    if (!commitString.isEmpty()) {
        gfx::Range replacementRange = (replacementLength > 0) ? gfx::Range(replacementStart, replacementStart + replacementLength)
                                                              : gfx::Range::InvalidRange();
        m_host->ImeConfirmComposition(toString16(commitString), replacementRange, false);
        m_imeInProgress = false;
    } else if (!preeditString.isEmpty()) {
        if (!selectionRange.IsValid()) {
            // We did not receive a valid selection range, hence the range is going to mark the cursor position.
            int newCursorPosition = (cursorPositionInPreeditString < 0) ? preeditString.length() : cursorPositionInPreeditString;
            selectionRange.set_start(newCursorPosition);
            selectionRange.set_end(newCursorPosition);
        }
        m_host->ImeSetComposition(toString16(preeditString), underlines, selectionRange.start(), selectionRange.end());
        m_imeInProgress = true;
    }
}

#ifndef QT_NO_ACCESSIBILITY
void RenderWidgetHostViewQt::accessibilityActiveChanged(bool active)
{
    if (active)
        content::BrowserAccessibilityStateImpl::GetInstance()->EnableAccessibility();
    else
        content::BrowserAccessibilityStateImpl::GetInstance()->DisableAccessibility();
}
#endif // QT_NO_ACCESSIBILITY

void RenderWidgetHostViewQt::handleWheelEvent(QWheelEvent *ev)
{
    m_host->ForwardWheelEvent(WebEventFactory::toWebWheelEvent(ev, dpiScale()));
}

void RenderWidgetHostViewQt::clearPreviousTouchMotionState()
{
    m_previousTouchPoints.clear();
    m_touchMotionStarted = false;
}

void RenderWidgetHostViewQt::handleTouchEvent(QTouchEvent *ev)
{
    // Chromium expects the touch event timestamps to be comparable to base::TimeTicks::Now().
    // Most importantly we also have to preserve the relative time distance between events.
    // Calculate a delta between event timestamps and Now() on the first received event, and
    // apply this delta to all successive events. This delta is most likely smaller than it
    // should by calculating it here but this will hopefully cause less than one frame of delay.
    base::TimeTicks eventTimestamp = base::TimeTicks() + base::TimeDelta::FromMilliseconds(ev->timestamp());
    if (m_eventsToNowDelta == base::TimeDelta())
        m_eventsToNowDelta = base::TimeTicks::Now() - eventTimestamp;
    eventTimestamp += m_eventsToNowDelta;

    QList<QTouchEvent::TouchPoint> touchPoints = mapTouchPointIds(ev->touchPoints());

    switch (ev->type()) {
    case QEvent::TouchBegin:
        m_sendMotionActionDown = true;
        m_touchMotionStarted = true;
        break;
    case QEvent::TouchUpdate:
        m_touchMotionStarted = true;
        break;
    case QEvent::TouchCancel:
    {
        // Don't process a TouchCancel event if no motion was started beforehand, or if there are
        // no touch points in the current event or in the previously processed event.
        if (!m_touchMotionStarted || (touchPoints.isEmpty() && m_previousTouchPoints.isEmpty())) {
            clearPreviousTouchMotionState();
            return;
        }

        // Use last saved touch points for the cancel event, to get rid of a QList assert,
        // because Chromium expects a MotionEvent::ACTION_CANCEL instance to contain at least
        // one touch point, whereas a QTouchCancel may not contain any touch points at all.
        if (touchPoints.isEmpty())
            touchPoints = m_previousTouchPoints;
        clearPreviousTouchMotionState();
        MotionEventQt cancelEvent(touchPoints, eventTimestamp, ui::MotionEvent::ACTION_CANCEL,
                                  ev->modifiers(), dpiScale());
        processMotionEvent(cancelEvent);
        return;
    }
    case QEvent::TouchEnd:
        clearPreviousTouchMotionState();
        break;
    default:
        break;
    }

    // Make sure that ACTION_POINTER_DOWN is delivered before ACTION_MOVE,
    // and ACTION_MOVE before ACTION_POINTER_UP.
    std::sort(touchPoints.begin(), touchPoints.end(), compareTouchPoints);

    m_previousTouchPoints = touchPoints;
    for (int i = 0; i < touchPoints.size(); ++i) {
        ui::MotionEvent::Action action;
        switch (touchPoints[i].state()) {
        case Qt::TouchPointPressed:
            if (m_sendMotionActionDown) {
                action = ui::MotionEvent::ACTION_DOWN;
                m_sendMotionActionDown = false;
            } else {
                action = ui::MotionEvent::ACTION_POINTER_DOWN;
            }
            break;
        case Qt::TouchPointMoved:
            action = ui::MotionEvent::ACTION_MOVE;
            break;
        case Qt::TouchPointReleased:
            action = touchPoints.size() > 1 ? ui::MotionEvent::ACTION_POINTER_UP :
                                              ui::MotionEvent::ACTION_UP;
            break;
        default:
            // Ignore Qt::TouchPointStationary
            continue;
        }

        MotionEventQt motionEvent(touchPoints, eventTimestamp, action, ev->modifiers(), dpiScale(),
                                  i);
        processMotionEvent(motionEvent);
    }
}

void RenderWidgetHostViewQt::handleHoverEvent(QHoverEvent *ev)
{
    m_host->ForwardMouseEvent(WebEventFactory::toWebMouseEvent(ev, dpiScale()));
}

void RenderWidgetHostViewQt::handleFocusEvent(QFocusEvent *ev)
{
    if (ev->gotFocus()) {
        m_host->GotFocus();
        m_host->SetActive(true);
        Q_ASSERT(m_host->IsRenderView());
        if (ev->reason() == Qt::TabFocusReason)
            static_cast<content::RenderViewHostImpl*>(m_host)->SetInitialFocus(false);
        else if (ev->reason() == Qt::BacktabFocusReason)
            static_cast<content::RenderViewHostImpl*>(m_host)->SetInitialFocus(true);
        ev->accept();
    } else if (ev->lostFocus()) {
        m_host->SetActive(false);
        m_host->Blur();
        ev->accept();
    }
}

void RenderWidgetHostViewQt::didFirstVisuallyNonEmptyLayout()
{
    m_didFirstVisuallyNonEmptyLayout = true;
}

} // namespace QtWebEngineCore
