// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/demo/gl_renderer.h"
#include "ui/ozone/demo/software_renderer.h"
#include "ui/ozone/demo/surfaceless_gl_renderer.h"
#include "ui/ozone/gpu/gpu_memory_buffer_factory_ozone_native_buffer.h"
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

const char kDisableGpu[] = "disable-gpu";

const char kWindowSize[] = "window-size";

class DemoWindow;

class RendererFactory {
 public:
  enum RendererType {
    GL,
    SURFACELESS_GL,
    SOFTWARE,
  };

  RendererFactory();
  ~RendererFactory();

  bool Initialize();
  scoped_ptr<ui::Renderer> CreateRenderer(gfx::AcceleratedWidget widget,
                                          const gfx::Size& size);

 private:
  RendererType type_ = SOFTWARE;

  // Helper for applications that do GL on main thread.
  ui::OzoneGpuTestHelper gpu_helper_;

  // Used by the surfaceless renderers to allocate buffers.
  ui::GpuMemoryBufferFactoryOzoneNativeBuffer buffer_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererFactory);
};

class WindowManager : public ui::NativeDisplayObserver {
 public:
  WindowManager(const base::Closure& quit_closure);
  ~WindowManager() override;

  void Quit();

  void AddWindow(DemoWindow* window);
  void RemoveWindow(DemoWindow* window);

 private:
  void OnDisplaysAquired(const std::vector<ui::DisplaySnapshot*>& displays);
  void OnDisplayConfigured(const gfx::Rect& bounds, bool success);

  // ui::NativeDisplayDelegate:
  void OnConfigurationChanged() override;

  scoped_ptr<ui::NativeDisplayDelegate> delegate_;
  base::Closure quit_closure_;
  RendererFactory renderer_factory_;
  ScopedVector<DemoWindow> windows_;

  // Flags used to keep track of the current state of display configuration.
  //
  // True if configuring the displays. In this case a new display configuration
  // isn't started.
  bool is_configuring_ = false;

  // If |is_configuring_| is true and another display configuration event
  // happens, the event is deferred. This is set to true and a display
  // configuration will be scheduled after the current one finishes.
  bool should_configure_ = false;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

class DemoWindow : public ui::PlatformWindowDelegate {
 public:
  DemoWindow(WindowManager* window_manager,
             RendererFactory* renderer_factory,
             const gfx::Rect& bounds)
      : window_manager_(window_manager),
        renderer_factory_(renderer_factory),
        weak_ptr_factory_(this) {
    platform_window_ =
        ui::OzonePlatform::GetInstance()->CreatePlatformWindow(this, bounds);
  }
  ~DemoWindow() override {}

  gfx::AcceleratedWidget GetAcceleratedWidget() {
    // TODO(spang): We should start rendering asynchronously.
    DCHECK_NE(widget_, gfx::kNullAcceleratedWidget)
        << "Widget not available synchronously";
    return widget_;
  }

  gfx::Size GetSize() { return platform_window_->GetBounds().size(); }

  void Start() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&DemoWindow::StartOnGpu, weak_ptr_factory_.GetWeakPtr()));
  }

  void Quit() {
    window_manager_->Quit();
  }

  // PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {
    if (event->IsKeyEvent() &&
        static_cast<ui::KeyEvent*>(event)->code() == ui::DomCode::KEY_Q)
      Quit();
  }
  void OnCloseRequest() override { Quit(); }
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                    float device_pixel_ratio) override {
    DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
    widget_ = widget;
  }
  void OnActivationChanged(bool active) override {}

 private:
  // Since we pretend to have a GPU process, we should also pretend to
  // initialize the GPU resources via a posted task.
  void StartOnGpu() {
    renderer_ =
        renderer_factory_->CreateRenderer(GetAcceleratedWidget(), GetSize());
    renderer_->Initialize();
  }

  WindowManager* window_manager_;      // Not owned.
  RendererFactory* renderer_factory_;  // Not owned.

  scoped_ptr<ui::Renderer> renderer_;

  // Window-related state.
  scoped_ptr<ui::PlatformWindow> platform_window_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;

  base::WeakPtrFactory<DemoWindow> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindow);
};

///////////////////////////////////////////////////////////////////////////////
// RendererFactory implementation:

RendererFactory::RendererFactory() {
}

RendererFactory::~RendererFactory() {
}

bool RendererFactory::Initialize() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kDisableGpu) &&
      gfx::GLSurface::InitializeOneOff() &&
      gpu_helper_.Initialize(base::ThreadTaskRunnerHandle::Get(),
                             base::ThreadTaskRunnerHandle::Get())) {
    if (command_line->HasSwitch(switches::kOzoneUseSurfaceless)) {
      type_ = SURFACELESS_GL;
    } else {
      type_ = GL;
    }
  } else {
    type_ = SOFTWARE;
  }

  return true;
}

scoped_ptr<ui::Renderer> RendererFactory::CreateRenderer(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size) {
  switch (type_) {
    case GL:
      return make_scoped_ptr(new ui::GlRenderer(widget, size));
    case SURFACELESS_GL:
      return make_scoped_ptr(
          new ui::SurfacelessGlRenderer(widget, size, &buffer_factory_));
    case SOFTWARE:
      return make_scoped_ptr(new ui::SoftwareRenderer(widget, size));
  }

  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// WindowManager implementation:

WindowManager::WindowManager(const base::Closure& quit_closure)
    : delegate_(
          ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate()),
      quit_closure_(quit_closure) {
  if (!renderer_factory_.Initialize())
    LOG(FATAL) << "Failed to initialize renderer factory";

  if (delegate_) {
    delegate_->AddObserver(this);
    delegate_->Initialize();
    OnConfigurationChanged();
  } else {
    LOG(WARNING) << "No display delegate; falling back to test window";
    int width = kTestWindowWidth;
    int height = kTestWindowHeight;
    sscanf(base::CommandLine::ForCurrentProcess()
               ->GetSwitchValueASCII(kWindowSize)
               .c_str(),
           "%dx%d", &width, &height);

    DemoWindow* window = new DemoWindow(this, &renderer_factory_,
                                        gfx::Rect(gfx::Size(width, height)));
    window->Start();
  }
}

WindowManager::~WindowManager() {
  if (delegate_)
    delegate_->RemoveObserver(this);
}

void WindowManager::Quit() {
  quit_closure_.Run();
}

void WindowManager::OnConfigurationChanged() {
  if (is_configuring_) {
    should_configure_ = true;
    return;
  }

  is_configuring_ = true;
  delegate_->GrabServer();
  delegate_->GetDisplays(
      base::Bind(&WindowManager::OnDisplaysAquired, base::Unretained(this)));
}

void WindowManager::OnDisplaysAquired(
    const std::vector<ui::DisplaySnapshot*>& displays) {
  windows_.clear();

  gfx::Point origin;
  for (auto display : displays) {
    if (!display->native_mode()) {
      LOG(ERROR) << "Display " << display->display_id()
                 << " doesn't have a native mode";
      continue;
    }

    delegate_->Configure(
        *display, display->native_mode(), origin,
        base::Bind(&WindowManager::OnDisplayConfigured, base::Unretained(this),
                   gfx::Rect(origin, display->native_mode()->size())));
    origin.Offset(display->native_mode()->size().width(), 0);
  }
  delegate_->UngrabServer();
  is_configuring_ = false;

  if (should_configure_) {
    should_configure_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&WindowManager::OnConfigurationChanged,
                              base::Unretained(this)));
  }
}

void WindowManager::OnDisplayConfigured(const gfx::Rect& bounds, bool success) {
  if (success) {
    scoped_ptr<DemoWindow> window(
        new DemoWindow(this, &renderer_factory_, bounds));
    window->Start();
    windows_.push_back(window.Pass());
  } else {
    LOG(ERROR) << "Failed to configure display at " << bounds.ToString();
  }
}

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;

  // Initialize logging so we can enable VLOG messages.
  logging::LoggingSettings settings;
  logging::InitLogging(settings);

  // Build UI thread message loop. This is used by platform
  // implementations for event polling & running background tasks.
  base::MessageLoopForUI message_loop;

  ui::OzonePlatform::InitializeForUI();
  ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()
      ->SetCurrentLayoutByName("us");

  base::RunLoop run_loop;

  WindowManager window_manager(run_loop.QuitClosure());

  run_loop.Run();

  return 0;
}
