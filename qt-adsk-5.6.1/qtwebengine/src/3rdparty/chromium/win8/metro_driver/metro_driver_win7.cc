// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include <corewindow.h>
#include <shobjidl.h>

#include "base/logging.h"
#include "base/profiler/scoped_tracker.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/win/msg_util.h"

#pragma comment(lib, "shell32.lib")

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
// Even though we only create a single window, we need to keep this
// count because of the hidden window used by the UI message loop of
// the metro viewer.
int g_window_count = 0;

const wchar_t kAshWin7AppId[] = L"Google.Chrome.AshWin7.1";
const wchar_t kAshWin7CoreWindowHandler[] = L"CoreWindowHandler";
extern float GetModernUIScale();
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                         LPARAM lparam);

HWND CreateMetroTopLevelWindow(const RECT& work_area) {
  HINSTANCE hInst = reinterpret_cast<HINSTANCE>(&__ImageBase);
  WNDCLASSEXW wcex;
  wcex.cbSize = sizeof(wcex);
  wcex.style              = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc        = WndProc;
  wcex.cbClsExtra         = 0;
  wcex.cbWndExtra         = 0;
  wcex.hInstance          = hInst;
  wcex.hIcon              = LoadIcon(::GetModuleHandle(NULL), L"IDR_MAINFRAME");
  wcex.hCursor            = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground      = (HBRUSH)(COLOR_INACTIVECAPTION+1);
  wcex.lpszMenuName       = 0;
  wcex.lpszClassName      = L"Windows.UI.Core.CoreWindow";
  wcex.hIconSm            = LoadIcon(::GetModuleHandle(NULL), L"IDR_MAINFRAME");

  HWND hwnd = ::CreateWindowExW(0,
                                MAKEINTATOM(::RegisterClassExW(&wcex)),
                                L"metro_win7",
                                WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX,
                                work_area.top, work_area.left,
                                work_area.right, work_area.bottom,
                                NULL, NULL, hInst, NULL);
  return hwnd;
}

typedef winfoundtn::ITypedEventHandler<
    winapp::Core::CoreApplicationView*,
    winapp::Activation::IActivatedEventArgs*> ActivatedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::WindowActivatedEventArgs*> WindowActivatedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::AutomationProviderRequestedEventArgs*>
        AutomationProviderHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::CharacterReceivedEventArgs*> CharEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::CoreWindowEventArgs*> CoreWindowEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::InputEnabledEventArgs*> InputEnabledEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::KeyEventArgs*> KeyEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::PointerEventArgs*> PointerEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::WindowSizeChangedEventArgs*> SizeChangedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::TouchHitTestingEventArgs*> TouchHitTestHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::VisibilityChangedEventArgs*> VisibilityChangedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreDispatcher*,
    winui::Core::AcceleratorKeyEventArgs*> AcceleratorKeyEventHandler;

// This interface is implemented by classes which handle mouse and keyboard
// input.
class InputHandler {
 public:
  InputHandler() {}
  virtual ~InputHandler() {}

  virtual bool HandleKeyboardMessage(const MSG& msg) = 0;
  virtual bool HandleMouseMessage(const MSG& msg) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

// This class implements the winrt interfaces corresponding to mouse input.
class MouseEvent : public mswr::RuntimeClass<
    winui::Core::IPointerEventArgs,
    winui::Input::IPointerPoint,
    winui::Input::IPointerPointProperties,
    windevs::Input::IPointerDevice> {
 public:
  MouseEvent(const MSG& msg)
      : msg_(msg) {
  }

  // IPointerEventArgs implementation.
  HRESULT STDMETHODCALLTYPE
  get_CurrentPoint(winui::Input::IPointerPoint** point) override {
    return QueryInterface(winui::Input::IID_IPointerPoint,
                          reinterpret_cast<void**>(point));
  }

  HRESULT STDMETHODCALLTYPE
  get_KeyModifiers(winsys::VirtualKeyModifiers* modifiers) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetIntermediatePoints(
      winfoundtn::Collections::IVector<winui::Input::PointerPoint*>** points)
      override {
    return E_NOTIMPL;
  }

  // IPointerPoint implementation.
  HRESULT STDMETHODCALLTYPE
  get_PointerDevice(windevs::Input::IPointerDevice** pointer_device) override {
    return QueryInterface(windevs::Input::IID_IPointerDevice,
                          reinterpret_cast<void**>(pointer_device));
  }

  HRESULT STDMETHODCALLTYPE get_Position(winfoundtn::Point* position) override {
    static float scale = GetModernUIScale();
    // Scale down the points here as they are scaled up on the other side.
    position->X = gfx::ToRoundedInt(CR_GET_X_LPARAM(msg_.lParam) / scale);
    position->Y = gfx::ToRoundedInt(CR_GET_Y_LPARAM(msg_.lParam) / scale);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_PointerId(uint32* pointer_id) override {
    // TODO(ananta)
    // Implement this properly.
    *pointer_id = 1;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_Timestamp(uint64* timestamp) override {
    *timestamp = msg_.time;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_Properties(winui::Input::IPointerPointProperties** properties) override {
    return QueryInterface(winui::Input::IID_IPointerPointProperties,
                          reinterpret_cast<void**>(properties));
  }

  HRESULT STDMETHODCALLTYPE
  get_RawPosition(winfoundtn::Point* position) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_FrameId(uint32* frame_id) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_IsInContact(boolean* in_contact) override {
    return E_NOTIMPL;
  }

  // IPointerPointProperties implementation.
  HRESULT STDMETHODCALLTYPE
  get_PointerUpdateKind(winui::Input::PointerUpdateKind* update_kind) override {
    // TODO(ananta)
    // There is no WM_POINTERUPDATE equivalent on Windows 7. Look into
    // equivalents.
    if (msg_.message == WM_LBUTTONDOWN) {
      *update_kind = winui::Input::PointerUpdateKind_LeftButtonPressed;
    } else if (msg_.message == WM_RBUTTONDOWN) {
      *update_kind = winui::Input::PointerUpdateKind_RightButtonPressed;
    } else if (msg_.message == WM_MBUTTONDOWN) {
      *update_kind = winui::Input::PointerUpdateKind_MiddleButtonPressed;
    } else if (msg_.message == WM_LBUTTONUP) {
      *update_kind = winui::Input::PointerUpdateKind_LeftButtonReleased;
    } else if (msg_.message == WM_RBUTTONUP) {
      *update_kind = winui::Input::PointerUpdateKind_RightButtonReleased;
    } else if (msg_.message == WM_MBUTTONUP) {
      *update_kind = winui::Input::PointerUpdateKind_MiddleButtonReleased;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_IsLeftButtonPressed(boolean* left_button_pressed) override {
    *left_button_pressed = msg_.wParam & MK_LBUTTON ? true : false;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_IsRightButtonPressed(boolean* right_button_pressed) override {
    *right_button_pressed = msg_.wParam & MK_RBUTTON ? true : false;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_IsMiddleButtonPressed(boolean* middle_button_pressed) override {
    *middle_button_pressed = msg_.wParam & MK_MBUTTON ? true : false;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_IsHorizontalMouseWheel(boolean* is_horizontal_mouse_wheel) override {
    *is_horizontal_mouse_wheel =
        (msg_.message == WM_MOUSEHWHEEL) ? true : false;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_MouseWheelDelta(int* delta) override {
    if (msg_.message == WM_MOUSEWHEEL || msg_.message == WM_MOUSEHWHEEL) {
      *delta = GET_WHEEL_DELTA_WPARAM(msg_.wParam);
      return S_OK;
    } else {
      return S_FALSE;
    }
  }

  HRESULT STDMETHODCALLTYPE get_Pressure(float* pressure) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_IsInverted(boolean* inverted) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_IsEraser(boolean* is_eraser) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_Orientation(float* orientation) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_XTilt(float* x_tilt) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_YTilt(float* y_tilt) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_Twist(float* twist) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_ContactRect(winfoundtn::Rect* rect) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_ContactRectRaw(winfoundtn::Rect* rect) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_TouchConfidence(boolean* confidence) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_IsPrimary(boolean* is_primary) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_IsInRange(boolean* is_in_range) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_IsCanceled(boolean* is_canceled) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_IsBarrelButtonPressed(boolean* is_barrel_button_pressed) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_IsXButton1Pressed(boolean* is_xbutton1_pressed) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_IsXButton2Pressed(boolean* is_xbutton2_pressed) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  HasUsage(uint32 usage_page, uint32 usage_id, boolean* has_usage) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetUsageValue(uint32 usage_page,
                                          uint32 usage_id,
                                          int32* usage_value) override {
    return E_NOTIMPL;
  }

  // IPointerDevice implementation.
  HRESULT STDMETHODCALLTYPE get_PointerDeviceType(
      windevs::Input::PointerDeviceType* device_type) override {
    if (msg_.message == WM_TOUCH) {
      *device_type = windevs::Input::PointerDeviceType_Touch;
    } else {
      *device_type = windevs::Input::PointerDeviceType_Mouse;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_IsIntegrated(boolean* is_integrated) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_MaxContacts(uint32* contacts) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_PhysicalDeviceRect(winfoundtn::Rect* rect) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_ScreenRect(winfoundtn::Rect* rect) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_SupportedUsages(
      winfoundtn::Collections::IVectorView<windevs::Input::PointerDeviceUsage>**
          usages) override {
    return E_NOTIMPL;
  }

 private:
  MSG msg_;

  DISALLOW_COPY_AND_ASSIGN(MouseEvent);
};

// This class implements the winrt interfaces needed to support keyboard
// character and system character messages.
class KeyEvent : public mswr::RuntimeClass<
    winui::Core::IKeyEventArgs,
    winui::Core::ICharacterReceivedEventArgs,
    winui::Core::IAcceleratorKeyEventArgs> {
 public:
  KeyEvent(const MSG& msg)
      : msg_(msg) {}

  // IKeyEventArgs implementation.
  HRESULT STDMETHODCALLTYPE
  get_VirtualKey(winsys::VirtualKey* virtual_key) override {
    *virtual_key = static_cast<winsys::VirtualKey>(msg_.wParam);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_KeyStatus(winui::Core::CorePhysicalKeyStatus* key_status) override {
    // As per msdn documentation for the keyboard messages.
    key_status->RepeatCount = msg_.lParam & 0x0000FFFF;
    key_status->ScanCode = (msg_.lParam >> 16) & 0x00FF;
    key_status->IsExtendedKey = (msg_.lParam & (1 << 24));
    key_status->IsMenuKeyDown = (msg_.lParam & (1 << 29));
    key_status->WasKeyDown = (msg_.lParam & (1 << 30));
    key_status->IsKeyReleased = (msg_.lParam & (1 << 31));
    return S_OK;
  }

  // ICharacterReceivedEventArgs implementation.
  HRESULT STDMETHODCALLTYPE get_KeyCode(uint32* key_code) override {
    *key_code = msg_.wParam;
    return S_OK;
  }

  // IAcceleratorKeyEventArgs implementation.
  HRESULT STDMETHODCALLTYPE
  get_EventType(winui::Core::CoreAcceleratorKeyEventType* event_type) override {
    if (msg_.message == WM_SYSKEYDOWN) {
      *event_type = winui::Core::CoreAcceleratorKeyEventType_SystemKeyDown;
    } else if (msg_.message == WM_SYSKEYUP) {
      *event_type = winui::Core::CoreAcceleratorKeyEventType_SystemKeyUp;
    } else if (msg_.message == WM_SYSCHAR) {
      *event_type = winui::Core::CoreAcceleratorKeyEventType_SystemCharacter;
    }
    return S_OK;
  }

 private:
  MSG msg_;
};

// The following classes are the emulation of the WinRT system as exposed
// to metro applications. There is one application (ICoreApplication) which
// contains a series of Views (ICoreApplicationView) each one of them
// containing a CoreWindow which represents a surface that can drawn to
// and that receives events.
//
// Here is the general dependency hierachy in terms of interfaces:
//
//  IFrameworkViewSource --> IFrameworkView
//          ^                     |
//          |                     |                          metro app
//  ---------------------------------------------------------------------
//          |                     |                         winRT system
//          |                     v
//  ICoreApplication     ICoreApplicationView
//                                |
//                                v
//                          ICoreWindow -----> ICoreWindowInterop
//                                |                  |
//                                |                  |
//                                v                  V
//                         ICoreDispatcher  <==>  real HWND
//
class CoreDispatcherEmulation :
    public mswr::RuntimeClass<
        winui::Core::ICoreDispatcher,
        winui::Core::ICoreAcceleratorKeys> {
 public:
  CoreDispatcherEmulation(InputHandler* input_handler)
      : input_handler_(input_handler),
        accelerator_key_event_handler_(NULL) {}

  // ICoreDispatcher implementation:
  HRESULT STDMETHODCALLTYPE get_HasThreadAccess(boolean* value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  ProcessEvents(winui::Core::CoreProcessEventsOption options) override {
    // We don't support the other message pump modes. So we basically enter a
    // traditional message loop that we only exit a teardown.
    if (options != winui::Core::CoreProcessEventsOption_ProcessUntilQuit)
      return E_FAIL;

    MSG msg = {0};
    while((::GetMessage(&msg, NULL, 0, 0) != 0) && g_window_count > 0) {
      ProcessInputMessage(msg);
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
    // TODO(cpu): figure what to do with msg.WParam which we would normally
    // return here.
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  RunAsync(winui::Core::CoreDispatcherPriority priority,
           winui::Core::IDispatchedHandler* agileCallback,
           ABI::Windows::Foundation::IAsyncAction** asyncAction) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  RunIdleAsync(winui::Core::IIdleDispatchedHandler* agileCallback,
               winfoundtn::IAsyncAction** asyncAction) override {
    return S_OK;
  }

  // ICoreAcceleratorKeys implementation:
  HRESULT STDMETHODCALLTYPE
  add_AcceleratorKeyActivated(AcceleratorKeyEventHandler* handler,
                              EventRegistrationToken* pCookie) override {
    accelerator_key_event_handler_ = handler;
    accelerator_key_event_handler_->AddRef();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  remove_AcceleratorKeyActivated(EventRegistrationToken cookie) override {
    accelerator_key_event_handler_->Release();
    accelerator_key_event_handler_ = NULL;
    return S_OK;
  }

 private:
  bool ProcessInputMessage(const MSG& msg) {
    // Poor man's way of dispatching input events.
    bool ret = false;
    if (input_handler_) {
      if ((msg.message >= WM_KEYFIRST) && (msg.message <= WM_KEYLAST)) {
        if ((msg.message == WM_SYSKEYDOWN) || (msg.message == WM_SYSKEYUP) ||
            msg.message == WM_SYSCHAR) {
          ret = HandleSystemKeys(msg);
        } else {
          ret = input_handler_->HandleKeyboardMessage(msg);
        }
      } else if ((msg.message >= WM_MOUSEFIRST) &&
                  (msg.message <= WM_MOUSELAST)) {
        ret = input_handler_->HandleMouseMessage(msg);
      }
    }
    return ret;
  }

  bool HandleSystemKeys(const MSG& msg) {
    mswr::ComPtr<winui::Core::IAcceleratorKeyEventArgs> event_args;
    event_args = mswr::Make<KeyEvent>(msg);
    accelerator_key_event_handler_->Invoke(this, event_args.Get());
    return true;
  }

  InputHandler* input_handler_;
  AcceleratorKeyEventHandler* accelerator_key_event_handler_;
};

class CoreWindowEmulation
    : public mswr::RuntimeClass<
        mswr::RuntimeClassFlags<mswr::WinRtClassicComMix>,
        winui::Core::ICoreWindow, ICoreWindowInterop>,
      public InputHandler {
 public:
  CoreWindowEmulation(winapp::Core::IFrameworkView* app_view)
      : mouse_moved_handler_(NULL),
        mouse_capture_lost_handler_(NULL),
        mouse_pressed_handler_(NULL),
        mouse_released_handler_(NULL),
        mouse_entered_handler_(NULL),
        mouse_exited_handler_(NULL),
        mouse_wheel_changed_handler_(NULL),
        key_down_handler_(NULL),
        key_up_handler_(NULL),
        character_received_handler_(NULL),
        core_hwnd_(NULL),
        app_view_(app_view),
        window_activated_handler_(NULL) {
    dispatcher_ = mswr::Make<CoreDispatcherEmulation>(this);

    // Unless we select our own AppUserModelID the shell might confuse us
    // with the app launcher one and we get the wrong taskbar button and icon.
    ::SetCurrentProcessExplicitAppUserModelID(kAshWin7AppId);

    RECT work_area = {0};
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0);
    if (::IsDebuggerPresent()) {
      work_area.top = 0;
      work_area.left = 0;
      work_area.right = 1600;
      work_area.bottom = 900;
    }

    core_hwnd_ = CreateMetroTopLevelWindow(work_area);
    ::SetProp(core_hwnd_, kAshWin7CoreWindowHandler, this);
  }

  ~CoreWindowEmulation() override {
    if (core_hwnd_) {
      ::RemoveProp(core_hwnd_, kAshWin7CoreWindowHandler);
      ::DestroyWindow(core_hwnd_);
    }
  }

  // ICoreWindow implementation:
  HRESULT STDMETHODCALLTYPE
  get_AutomationHostProvider(IInspectable** value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_Bounds(winfoundtn::Rect* value) override {
    RECT rect;
    if (!::GetClientRect(core_hwnd_, &rect))
      return E_FAIL;
    value->Width = rect.right;
    value->Height = rect.bottom;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_CustomProperties(winfoundtn::Collections::IPropertySet** value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_Dispatcher(winui::Core::ICoreDispatcher** value) override {
    return dispatcher_.CopyTo(value);
  }

  HRESULT STDMETHODCALLTYPE
  get_FlowDirection(winui::Core::CoreWindowFlowDirection* value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  put_FlowDirection(winui::Core::CoreWindowFlowDirection value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_IsInputEnabled(boolean* value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE put_IsInputEnabled(boolean value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_PointerCursor(winui::Core::ICoreCursor** value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  put_PointerCursor(winui::Core::ICoreCursor* value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_PointerPosition(winfoundtn::Point* value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_Visible(boolean* value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE Activate(void) override {
    // After we fire OnActivate on the View, Chrome calls us back here.
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE Close(void) override {
    ::PostMessage(core_hwnd_, WM_CLOSE, 0, 0);
    core_hwnd_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetAsyncKeyState(
      ABI::Windows::System::VirtualKey virtualKey,
      winui::Core::CoreVirtualKeyStates* KeyState) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetKeyState(
      ABI::Windows::System::VirtualKey virtualKey,
      winui::Core::CoreVirtualKeyStates* KeyState) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE ReleasePointerCapture(void) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE SetPointerCapture(void) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_Activated(
      WindowActivatedHandler* handler,
      EventRegistrationToken* pCookie) override {
    window_activated_handler_ = handler;
    handler->AddRef();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_Activated(
      EventRegistrationToken cookie) override {
    window_activated_handler_->Release();
    window_activated_handler_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_AutomationProviderRequested(
      AutomationProviderHandler* handler,
      EventRegistrationToken* cookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_AutomationProviderRequested(
      EventRegistrationToken cookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_CharacterReceived(
      CharEventHandler* handler,
      EventRegistrationToken* pCookie) override {
    character_received_handler_ = handler;
    character_received_handler_->AddRef();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_CharacterReceived(
      EventRegistrationToken cookie) override {
    character_received_handler_->Release();
    character_received_handler_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_Closed(
      CoreWindowEventHandler* handler,
      EventRegistrationToken* pCookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_Closed(
      EventRegistrationToken cookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_InputEnabled(
      InputEnabledEventHandler* handler,
      EventRegistrationToken* pCookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_InputEnabled(
      EventRegistrationToken cookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_KeyDown(
      KeyEventHandler* handler,
      EventRegistrationToken* pCookie) override {
    key_down_handler_ = handler;
    key_down_handler_->AddRef();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_KeyDown(
      EventRegistrationToken cookie) override {
    key_down_handler_->Release();
    key_down_handler_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_KeyUp(
      KeyEventHandler* handler,
      EventRegistrationToken* pCookie) override {
    key_up_handler_ = handler;
    key_up_handler_->AddRef();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_KeyUp(
      EventRegistrationToken cookie) override {
    key_up_handler_->Release();
    key_up_handler_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_PointerCaptureLost(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) override {
    mouse_capture_lost_handler_ = handler;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_PointerCaptureLost(
      EventRegistrationToken cookie) override {
    mouse_capture_lost_handler_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_PointerEntered(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) override {
    mouse_entered_handler_ = handler;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_PointerEntered(
      EventRegistrationToken cookie) override {
    mouse_entered_handler_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_PointerExited(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) override {
    mouse_exited_handler_ = handler;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_PointerExited(
      EventRegistrationToken cookie) override {
    mouse_exited_handler_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_PointerMoved(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) override {
    mouse_moved_handler_ = handler;
    mouse_moved_handler_->AddRef();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_PointerMoved(
      EventRegistrationToken cookie) override {
    mouse_moved_handler_->Release();
    mouse_moved_handler_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_PointerPressed(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) override {
    mouse_pressed_handler_ = handler;
    mouse_pressed_handler_->AddRef();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_PointerPressed(
      EventRegistrationToken cookie) override {
    mouse_pressed_handler_->Release();
    mouse_pressed_handler_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_PointerReleased(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) override {
    mouse_released_handler_ = handler;
    mouse_released_handler_->AddRef();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_PointerReleased(
      EventRegistrationToken cookie) override {
    mouse_released_handler_->Release();
    mouse_released_handler_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_TouchHitTesting(
      TouchHitTestHandler* handler,
      EventRegistrationToken* pCookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_TouchHitTesting(
      EventRegistrationToken cookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_PointerWheelChanged(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) override {
    mouse_wheel_changed_handler_ = handler;
    mouse_wheel_changed_handler_->AddRef();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_PointerWheelChanged(
      EventRegistrationToken cookie) override {
    mouse_wheel_changed_handler_->Release();
    mouse_wheel_changed_handler_ = NULL;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_SizeChanged(
      SizeChangedHandler* handler,
      EventRegistrationToken* pCookie) override {
    // TODO(cpu): implement this.
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_SizeChanged(
      EventRegistrationToken cookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE add_VisibilityChanged(
      VisibilityChangedHandler* handler,
      EventRegistrationToken* pCookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE remove_VisibilityChanged(
      EventRegistrationToken cookie) override {
    return S_OK;
  }

  // ICoreWindowInterop implementation:
  HRESULT STDMETHODCALLTYPE get_WindowHandle(HWND* hwnd) override {
    if (!core_hwnd_)
      return E_FAIL;
    *hwnd = core_hwnd_;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE put_MessageHandled(boolean value) override {
    return S_OK;
  }

  // InputHandler
  bool HandleKeyboardMessage(const MSG& msg) override {
    switch (msg.message) {
      case WM_KEYDOWN:
      case WM_KEYUP: {
        mswr::ComPtr<winui::Core::IKeyEventArgs> event_args;
        event_args = mswr::Make<KeyEvent>(msg);
        KeyEventHandler* handler = NULL;
        if (msg.message == WM_KEYDOWN) {
          handler = key_down_handler_;
        } else {
          handler = key_up_handler_;
        }
        handler->Invoke(this, event_args.Get());
        break;
      }

      case WM_CHAR:
      case WM_DEADCHAR:
      case WM_UNICHAR: {
        mswr::ComPtr<winui::Core::ICharacterReceivedEventArgs> event_args;
        event_args = mswr::Make<KeyEvent>(msg);
        character_received_handler_->Invoke(this, event_args.Get());
        break;
      }

      default:
        return false;
    }
    return true;
  }

  bool HandleMouseMessage(const MSG& msg) override {
    PointerEventHandler* handler = NULL;
    mswr::ComPtr<winui::Core::IPointerEventArgs> event_args;
    event_args = mswr::Make<MouseEvent>(msg);
    switch (msg.message) {
      case WM_MOUSEMOVE: {
        handler = mouse_moved_handler_;
        break;
      }
      case WM_LBUTTONDOWN: {
      case WM_RBUTTONDOWN:
      case WM_MBUTTONDOWN:
        handler = mouse_pressed_handler_;
        break;
      }

      case WM_LBUTTONUP: {
      case WM_RBUTTONUP:
      case WM_MBUTTONUP:
        handler = mouse_released_handler_;
        break;
      }

      case WM_MOUSEWHEEL: {
      case WM_MOUSEHWHEEL:
        handler = mouse_wheel_changed_handler_;
        break;
      }

      default:
        return false;
    }
    DCHECK(handler);
    handler->Invoke(this, event_args.Get());
    return true;
  }

  void OnWindowActivated() {
    if (window_activated_handler_)
      window_activated_handler_->Invoke(this, NULL);
  }

 private:
   PointerEventHandler* mouse_moved_handler_;
   PointerEventHandler* mouse_capture_lost_handler_;
   PointerEventHandler* mouse_pressed_handler_;
   PointerEventHandler* mouse_released_handler_;
   PointerEventHandler* mouse_entered_handler_;
   PointerEventHandler* mouse_exited_handler_;
   PointerEventHandler* mouse_wheel_changed_handler_;
   KeyEventHandler* key_down_handler_;
   KeyEventHandler* key_up_handler_;
   CharEventHandler* character_received_handler_;
   HWND core_hwnd_;
   mswr::ComPtr<winui::Core::ICoreDispatcher> dispatcher_;
   mswr::ComPtr<winapp::Core::IFrameworkView> app_view_;
   WindowActivatedHandler* window_activated_handler_;
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
                         WPARAM wparam, LPARAM lparam) {
  PAINTSTRUCT ps;
  HDC hdc;
  switch (message) {
    case WM_ACTIVATE: {
      // HIWORD(wparam) is 1 if the window is minimized.
      bool active = (LOWORD(wparam) != WA_INACTIVE) && !HIWORD(wparam);
      if (active) {
        CoreWindowEmulation* core_window_handler =
            reinterpret_cast<CoreWindowEmulation*>(
                ::GetProp(hwnd, kAshWin7CoreWindowHandler));
        if (core_window_handler)
          core_window_handler->OnWindowActivated();
      }
      return ::DefWindowProc(hwnd, message, wparam, lparam);
    }
    case WM_CREATE:
      ++g_window_count;
      break;
    case WM_PAINT:
      hdc = ::BeginPaint(hwnd, &ps);
      ::EndPaint(hwnd, &ps);
      break;
    case WM_CLOSE:
      ::DestroyWindow(hwnd);
      break;
    case WM_DESTROY:
      --g_window_count;
      if (!g_window_count)
        ::PostQuitMessage(0);
      break;
    // Always allow Chrome to set the cursor.
    case WM_SETCURSOR:
      return 1;
    default:
      return ::DefWindowProc(hwnd, message, wparam, lparam);
  }
  return 0;
}

class ActivatedEvent
    : public mswr::RuntimeClass<winapp::Activation::IActivatedEventArgs> {
 public:
  ActivatedEvent(winapp::Activation::ActivationKind activation_kind)
    : activation_kind_(activation_kind) {
  }

  HRESULT STDMETHODCALLTYPE
  get_Kind(winapp::Activation::ActivationKind* value) override {
    *value = activation_kind_;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_PreviousExecutionState(
      winapp::Activation::ApplicationExecutionState* value) override {
    *value = winapp::Activation::ApplicationExecutionState_ClosedByUser;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_SplashScreen(winapp::Activation::ISplashScreen** value) override {
    return E_FAIL;
  }

 private:
  winapp::Activation::ActivationKind activation_kind_;
};

class CoreApplicationViewEmulation
    : public mswr::RuntimeClass<winapp::Core::ICoreApplicationView> {
 public:
   CoreApplicationViewEmulation(winapp::Core::IFrameworkView* app_view) {
      core_window_ = mswr::Make<CoreWindowEmulation>(app_view);
   }

  HRESULT Activate() {
    if (activated_handler_) {
      auto ae = mswr::Make<ActivatedEvent>(
        winapp::Activation::ActivationKind_File);
      return activated_handler_->Invoke(this, ae.Get());
    } else {
      return S_OK;
    }
  }

  HRESULT Close() {
    return core_window_->Close();
  }

  // ICoreApplicationView implementation:
  HRESULT STDMETHODCALLTYPE
  get_CoreWindow(winui::Core::ICoreWindow** value) override {
    if (!core_window_)
      return E_FAIL;
    return core_window_.CopyTo(value);
  }

  HRESULT STDMETHODCALLTYPE
  add_Activated(ActivatedHandler* handler,
                EventRegistrationToken* token) override {
    // The real component supports multiple handles but we don't yet.
    if (activated_handler_)
      return E_FAIL;
    activated_handler_ = handler;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  remove_Activated(EventRegistrationToken token) override {
    // Chrome never unregisters handlers, so we don't care about it.
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_IsMain(boolean* value) override { return S_OK; }

  HRESULT STDMETHODCALLTYPE get_IsHosted(boolean* value) override {
    return S_OK;
  }

 private:
  mswr::ComPtr<CoreWindowEmulation> core_window_;
  mswr::ComPtr<ActivatedHandler> activated_handler_;
};

class CoreApplicationWin7Emulation
    : public mswr::RuntimeClass<winapp::Core::ICoreApplication,
                                winapp::Core::ICoreApplicationExit> {
 public:
   // ICoreApplication implementation:

  HRESULT STDMETHODCALLTYPE get_Id(HSTRING* value) override { return S_OK; }

  HRESULT STDMETHODCALLTYPE add_Suspending(
      winfoundtn::IEventHandler<winapp::SuspendingEventArgs*>* handler,
      EventRegistrationToken* token) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  remove_Suspending(EventRegistrationToken token) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  add_Resuming(winfoundtn::IEventHandler<IInspectable*>* handler,
               EventRegistrationToken* token) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  remove_Resuming(EventRegistrationToken token) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_Properties(winfoundtn::Collections::IPropertySet** value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  GetCurrentView(winapp::Core::ICoreApplicationView** value) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  Run(winapp::Core::IFrameworkViewSource* viewSource) override {
    HRESULT hr = viewSource->CreateView(app_view_.GetAddressOf());
    if (FAILED(hr))
      return hr;
    view_emulation_ = mswr::Make<CoreApplicationViewEmulation>(
        app_view_.Get());
    hr = app_view_->Initialize(view_emulation_.Get());
    if (FAILED(hr))
      return hr;
    mswr::ComPtr<winui::Core::ICoreWindow> core_window;
    hr = view_emulation_->get_CoreWindow(core_window.GetAddressOf());
    if (FAILED(hr))
      return hr;
    hr = app_view_->SetWindow(core_window.Get());
    if (FAILED(hr))
      return hr;
    hr = app_view_->Load(NULL);
    if (FAILED(hr))
      return hr;
    hr = view_emulation_->Activate();
    if (FAILED(hr))
      return hr;
    return app_view_->Run();
  }

  HRESULT STDMETHODCALLTYPE RunWithActivationFactories(
      winfoundtn::IGetActivationFactory* activationFactoryCallback) override {
    return S_OK;
  }

  // ICoreApplicationExit implementation:

  HRESULT STDMETHODCALLTYPE Exit(void) override {
    return view_emulation_->Close();
  }

  HRESULT STDMETHODCALLTYPE
  add_Exiting(winfoundtn::IEventHandler<IInspectable*>* handler,
              EventRegistrationToken* token) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  remove_Exiting(EventRegistrationToken token) override {
    return S_OK;
  }

 private:
  mswr::ComPtr<winapp::Core::IFrameworkView> app_view_;
  mswr::ComPtr<CoreApplicationViewEmulation> view_emulation_;
};


mswr::ComPtr<winapp::Core::ICoreApplication> InitWindows7() {
  HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr))
    CHECK(false);
  return mswr::Make<CoreApplicationWin7Emulation>();
}

