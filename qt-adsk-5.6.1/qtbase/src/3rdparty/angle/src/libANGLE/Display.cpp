//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Display.cpp: Implements the egl::Display class, representing the abstract
// display on which graphics are drawn. Implements EGLDisplay.
// [EGL 1.4] section 2.1.2 page 3.

#include "libANGLE/Display.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <sstream>
#include <vector>

#include <platform/Platform.h>
#include <EGL/eglext.h>

#include "common/debug.h"
#include "common/mathutil.h"
#include "common/platform.h"
#include "libANGLE/Context.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/DisplayImpl.h"

#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
#   include "libANGLE/renderer/d3d/DisplayD3D.h"
#endif

#if defined(ANGLE_ENABLE_OPENGL)
#   if defined(ANGLE_PLATFORM_WINDOWS)
#       include "libANGLE/renderer/gl/wgl/DisplayWGL.h"
#   else
#       error Unsupported OpenGL platform.
#   endif
#endif

namespace egl
{

namespace
{

class DefaultPlatform : public angle::Platform
{
public:
    DefaultPlatform() {}
    ~DefaultPlatform() override {}
};

DefaultPlatform *defaultPlatform = nullptr;

void InitDefaultPlatformImpl()
{
    if (ANGLEPlatformCurrent() == nullptr)
    {
        if (defaultPlatform == nullptr)
        {
            defaultPlatform = new DefaultPlatform();
        }

        ANGLEPlatformInitialize(defaultPlatform);
    }
}

void DeinitDefaultPlatformImpl()
{
    if (defaultPlatform != nullptr)
    {
        if (ANGLEPlatformCurrent() == defaultPlatform)
        {
            ANGLEPlatformShutdown();
        }

        SafeDelete(defaultPlatform);
    }
}

typedef std::map<EGLNativeWindowType, Surface*> WindowSurfaceMap;
// Get a map of all EGL window surfaces to validate that no window has more than one EGL surface
// associated with it.
static WindowSurfaceMap *GetWindowSurfaces()
{
    static WindowSurfaceMap windowSurfaces;
    return &windowSurfaces;
}

typedef std::map<EGLNativeDisplayType, Display*> DisplayMap;
static DisplayMap *GetDisplayMap()
{
    static DisplayMap displays;
    return &displays;
}

rx::DisplayImpl *CreateDisplayImpl(const AttributeMap &attribMap)
{
    rx::DisplayImpl *impl = nullptr;
    EGLint displayType = attribMap.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE);
    switch (displayType)
    {
      case EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE:
#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
        // Default to D3D displays
        impl = new rx::DisplayD3D();
#else
        // No display available
        UNREACHABLE();
#endif
        break;

      case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
      case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
        impl = new rx::DisplayD3D();
#else
        // A D3D display was requested on a platform that doesn't support it
        UNREACHABLE();
#endif
        break;

      case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
#if defined(ANGLE_ENABLE_OPENGL)
#if defined(ANGLE_PLATFORM_WINDOWS)
        impl = new rx::DisplayWGL();
#else
#error Unsupported OpenGL platform.
#endif
#else
        UNREACHABLE();
#endif
        break;

      default:
        UNREACHABLE();
        break;
    }

    ASSERT(impl != nullptr);
    return impl;
}

}

Display *Display::getDisplay(EGLNativeDisplayType displayId, const AttributeMap &attribMap)
{
    // Initialize the global platform if not already
    InitDefaultPlatformImpl();

    Display *display = NULL;

    DisplayMap *displays = GetDisplayMap();
    DisplayMap::const_iterator iter = displays->find(displayId);
    if (iter != displays->end())
    {
        display = iter->second;
    }

    if (display == nullptr)
    {
        // Validate the native display
        if (!Display::isValidNativeDisplay(displayId))
        {
            return NULL;
        }

        display = new Display(displayId);
        displays->insert(std::make_pair(displayId, display));
    }

    // Apply new attributes if the display is not initialized yet.
    if (!display->isInitialized())
    {
        rx::DisplayImpl* impl = CreateDisplayImpl(attribMap);
        display->setAttributes(impl, attribMap);
    }

    return display;
}

Display::Display(EGLNativeDisplayType displayId)
    : mImplementation(nullptr),
      mDisplayId(displayId),
      mAttributeMap(),
      mConfigSet(),
      mContextSet(),
      mInitialized(false),
      mCaps(),
      mDisplayExtensions(),
      mDisplayExtensionString(),
      mVendorString()
{
}

Display::~Display()
{
    terminate();

    DisplayMap *displays = GetDisplayMap();
    DisplayMap::iterator iter = displays->find(mDisplayId);
    if (iter != displays->end())
    {
        displays->erase(iter);
    }

    SafeDelete(mImplementation);
}

void Display::setAttributes(rx::DisplayImpl *impl, const AttributeMap &attribMap)
{
    ASSERT(!mInitialized);

    ASSERT(impl != nullptr);
    SafeDelete(mImplementation);
    mImplementation = impl;

    mAttributeMap = attribMap;
}

Error Display::initialize()
{
    ASSERT(mImplementation != nullptr);

    if (isInitialized())
    {
        return Error(EGL_SUCCESS);
    }

    Error error = mImplementation->initialize(this);
    if (error.isError())
    {
        return error;
    }

    mCaps = mImplementation->getCaps();

    mConfigSet = mImplementation->generateConfigs();
    if (mConfigSet.size() == 0)
    {
        mImplementation->terminate();
        return Error(EGL_NOT_INITIALIZED);
    }

    initDisplayExtensions();
    initVendorString();

    mInitialized = true;
    return Error(EGL_SUCCESS);
}

void Display::terminate()
{
    makeCurrent(nullptr, nullptr, nullptr);

    while (!mContextSet.empty())
    {
        destroyContext(*mContextSet.begin());
    }

    mConfigSet.clear();

    mImplementation->terminate();
    mInitialized = false;

    // De-init default platform
    DeinitDefaultPlatformImpl();
}

std::vector<const Config*> Display::getConfigs(const egl::AttributeMap &attribs) const
{
    return mConfigSet.filter(attribs);
}

bool Display::getConfigAttrib(const Config *configuration, EGLint attribute, EGLint *value)
{
    switch (attribute)
    {
      case EGL_BUFFER_SIZE:               *value = configuration->bufferSize;             break;
      case EGL_ALPHA_SIZE:                *value = configuration->alphaSize;              break;
      case EGL_BLUE_SIZE:                 *value = configuration->blueSize;               break;
      case EGL_GREEN_SIZE:                *value = configuration->greenSize;              break;
      case EGL_RED_SIZE:                  *value = configuration->redSize;                break;
      case EGL_DEPTH_SIZE:                *value = configuration->depthSize;              break;
      case EGL_STENCIL_SIZE:              *value = configuration->stencilSize;            break;
      case EGL_CONFIG_CAVEAT:             *value = configuration->configCaveat;           break;
      case EGL_CONFIG_ID:                 *value = configuration->configID;               break;
      case EGL_LEVEL:                     *value = configuration->level;                  break;
      case EGL_NATIVE_RENDERABLE:         *value = configuration->nativeRenderable;       break;
      case EGL_NATIVE_VISUAL_TYPE:        *value = configuration->nativeVisualType;       break;
      case EGL_SAMPLES:                   *value = configuration->samples;                break;
      case EGL_SAMPLE_BUFFERS:            *value = configuration->sampleBuffers;          break;
      case EGL_SURFACE_TYPE:              *value = configuration->surfaceType;            break;
      case EGL_TRANSPARENT_TYPE:          *value = configuration->transparentType;        break;
      case EGL_TRANSPARENT_BLUE_VALUE:    *value = configuration->transparentBlueValue;   break;
      case EGL_TRANSPARENT_GREEN_VALUE:   *value = configuration->transparentGreenValue;  break;
      case EGL_TRANSPARENT_RED_VALUE:     *value = configuration->transparentRedValue;    break;
      case EGL_BIND_TO_TEXTURE_RGB:       *value = configuration->bindToTextureRGB;       break;
      case EGL_BIND_TO_TEXTURE_RGBA:      *value = configuration->bindToTextureRGBA;      break;
      case EGL_MIN_SWAP_INTERVAL:         *value = configuration->minSwapInterval;        break;
      case EGL_MAX_SWAP_INTERVAL:         *value = configuration->maxSwapInterval;        break;
      case EGL_LUMINANCE_SIZE:            *value = configuration->luminanceSize;          break;
      case EGL_ALPHA_MASK_SIZE:           *value = configuration->alphaMaskSize;          break;
      case EGL_COLOR_BUFFER_TYPE:         *value = configuration->colorBufferType;        break;
      case EGL_RENDERABLE_TYPE:           *value = configuration->renderableType;         break;
      case EGL_MATCH_NATIVE_PIXMAP:       *value = false; UNIMPLEMENTED();                break;
      case EGL_CONFORMANT:                *value = configuration->conformant;             break;
      case EGL_MAX_PBUFFER_WIDTH:         *value = configuration->maxPBufferWidth;        break;
      case EGL_MAX_PBUFFER_HEIGHT:        *value = configuration->maxPBufferHeight;       break;
      case EGL_MAX_PBUFFER_PIXELS:        *value = configuration->maxPBufferPixels;       break;
      default:
        return false;
    }

    return true;
}

Error Display::createWindowSurface(const Config *configuration, EGLNativeWindowType window, const AttributeMap &attribs,
                                   Surface **outSurface)
{
    if (mImplementation->testDeviceLost())
    {
        Error error = restoreLostDevice();
        if (error.isError())
        {
            return error;
        }
    }

    rx::SurfaceImpl *surfaceImpl = nullptr;
    Error error = mImplementation->createWindowSurface(configuration, window, attribs, &surfaceImpl);
    if (error.isError())
    {
        return error;
    }

    ASSERT(surfaceImpl != nullptr);
    Surface *surface = new Surface(surfaceImpl, EGL_WINDOW_BIT, configuration, attribs);
    mImplementation->getSurfaceSet().insert(surface);

    WindowSurfaceMap *windowSurfaces = GetWindowSurfaces();
    ASSERT(windowSurfaces && windowSurfaces->find(window) == windowSurfaces->end());
    windowSurfaces->insert(std::make_pair(window, surface));

    ASSERT(outSurface != nullptr);
    *outSurface = surface;
    return Error(EGL_SUCCESS);
}

Error Display::createPbufferSurface(const Config *configuration, const AttributeMap &attribs, Surface **outSurface)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        Error error = restoreLostDevice();
        if (error.isError())
        {
            return error;
        }
    }

    rx::SurfaceImpl *surfaceImpl = nullptr;
    Error error = mImplementation->createPbufferSurface(configuration, attribs, &surfaceImpl);
    if (error.isError())
    {
        return error;
    }

    ASSERT(surfaceImpl != nullptr);
    Surface *surface = new Surface(surfaceImpl, EGL_PBUFFER_BIT, configuration, attribs);
    mImplementation->getSurfaceSet().insert(surface);

    ASSERT(outSurface != nullptr);
    *outSurface = surface;
    return Error(EGL_SUCCESS);
}

Error Display::createPbufferFromClientBuffer(const Config *configuration, EGLClientBuffer shareHandle,
                                             const AttributeMap &attribs, Surface **outSurface)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        Error error = restoreLostDevice();
        if (error.isError())
        {
            return error;
        }
    }

    rx::SurfaceImpl *surfaceImpl = nullptr;
    Error error = mImplementation->createPbufferFromClientBuffer(configuration, shareHandle, attribs, &surfaceImpl);
    if (error.isError())
    {
        return error;
    }

    ASSERT(surfaceImpl != nullptr);
    Surface *surface = new Surface(surfaceImpl, EGL_PBUFFER_BIT, configuration, attribs);
    mImplementation->getSurfaceSet().insert(surface);

    ASSERT(outSurface != nullptr);
    *outSurface = surface;
    return Error(EGL_SUCCESS);
}

Error Display::createPixmapSurface(const Config *configuration, NativePixmapType nativePixmap, const AttributeMap &attribs,
                                   Surface **outSurface)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        Error error = restoreLostDevice();
        if (error.isError())
        {
            return error;
        }
    }

    rx::SurfaceImpl *surfaceImpl = nullptr;
    Error error = mImplementation->createPixmapSurface(configuration, nativePixmap, attribs, &surfaceImpl);
    if (error.isError())
    {
        return error;
    }

    ASSERT(surfaceImpl != nullptr);
    Surface *surface = new Surface(surfaceImpl, EGL_PIXMAP_BIT, configuration, attribs);
    mImplementation->getSurfaceSet().insert(surface);

    ASSERT(outSurface != nullptr);
    *outSurface = surface;
    return Error(EGL_SUCCESS);
}

Error Display::createContext(const Config *configuration, gl::Context *shareContext, const AttributeMap &attribs,
                             gl::Context **outContext)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        Error error = restoreLostDevice();
        if (error.isError())
        {
            return error;
        }
    }

    gl::Context *context = nullptr;
    Error error = mImplementation->createContext(configuration, shareContext, attribs, &context);
    if (error.isError())
    {
        return error;
    }

    ASSERT(context != nullptr);
    mContextSet.insert(context);

    ASSERT(outContext != nullptr);
    *outContext = context;
    return Error(EGL_SUCCESS);
}

Error Display::makeCurrent(egl::Surface *drawSurface, egl::Surface *readSurface, gl::Context *context)
{
    Error error = mImplementation->makeCurrent(drawSurface, readSurface, context);
    if (error.isError())
    {
        return error;
    }

    if (context && drawSurface)
    {
        context->makeCurrent(drawSurface);
    }

    return egl::Error(EGL_SUCCESS);
}

Error Display::restoreLostDevice()
{
    for (ContextSet::iterator ctx = mContextSet.begin(); ctx != mContextSet.end(); ctx++)
    {
        if ((*ctx)->isResetNotificationEnabled())
        {
            // If reset notifications have been requested, application must delete all contexts first
            return Error(EGL_CONTEXT_LOST);
        }
    }

    return mImplementation->restoreLostDevice();
}

void Display::destroySurface(Surface *surface)
{
    if (surface->getType() == EGL_WINDOW_BIT)
    {
        WindowSurfaceMap *windowSurfaces = GetWindowSurfaces();
        ASSERT(windowSurfaces);

        bool surfaceRemoved = false;
        for (WindowSurfaceMap::iterator iter = windowSurfaces->begin(); iter != windowSurfaces->end(); iter++)
        {
            if (iter->second == surface)
            {
                windowSurfaces->erase(iter);
                surfaceRemoved = true;
                break;
            }
        }

        ASSERT(surfaceRemoved);
    }

    mImplementation->destroySurface(surface);
}

void Display::destroyContext(gl::Context *context)
{
    mContextSet.erase(context);
    SafeDelete(context);
}

bool Display::isDeviceLost() const
{
    ASSERT(isInitialized());
    return mImplementation->isDeviceLost();
}

bool Display::testDeviceLost()
{
    ASSERT(isInitialized());
    return mImplementation->testDeviceLost();
}

void Display::notifyDeviceLost()
{
    for (ContextSet::iterator context = mContextSet.begin(); context != mContextSet.end(); context++)
    {
        (*context)->markContextLost();
    }
}

const Caps &Display::getCaps() const
{
    return mCaps;
}

bool Display::isInitialized() const
{
    return mInitialized;
}

bool Display::isValidConfig(const Config *config) const
{
    return mConfigSet.contains(config);
}

bool Display::isValidContext(gl::Context *context) const
{
    return mContextSet.find(context) != mContextSet.end();
}

bool Display::isValidSurface(Surface *surface) const
{
    return mImplementation->getSurfaceSet().find(surface) != mImplementation->getSurfaceSet().end();
}

bool Display::hasExistingWindowSurface(EGLNativeWindowType window)
{
    WindowSurfaceMap *windowSurfaces = GetWindowSurfaces();
    ASSERT(windowSurfaces);

    return windowSurfaces->find(window) != windowSurfaces->end();
}

static ClientExtensions GenerateClientExtensions()
{
    ClientExtensions extensions;

    extensions.clientExtensions = true;
    extensions.platformBase = true;
    extensions.platformANGLE = true;

#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
    extensions.platformANGLED3D = true;
#endif

#if defined(ANGLE_ENABLE_OPENGL)
    extensions.platformANGLEOpenGL = true;
#endif

    return extensions;
}

template <typename T>
static std::string GenerateExtensionsString(const T &extensions)
{
    std::vector<std::string> extensionsVector = extensions.getStrings();

    std::ostringstream stream;
    std::copy(extensionsVector.begin(), extensionsVector.end(), std::ostream_iterator<std::string>(stream, " "));
    return stream.str();
}

const ClientExtensions &Display::getClientExtensions()
{
    static const ClientExtensions clientExtensions = GenerateClientExtensions();
    return clientExtensions;
}

const std::string &Display::getClientExtensionString()
{
    static const std::string clientExtensionsString = GenerateExtensionsString(getClientExtensions());
    return clientExtensionsString;
}

void Display::initDisplayExtensions()
{
    mDisplayExtensions = mImplementation->getExtensions();
    mDisplayExtensionString = GenerateExtensionsString(mDisplayExtensions);
}

bool Display::isValidNativeWindow(EGLNativeWindowType window) const
{
    return mImplementation->isValidNativeWindow(window);
}

bool Display::isValidNativeDisplay(EGLNativeDisplayType display)
{
    // TODO(jmadill): handle this properly
    if (display == EGL_DEFAULT_DISPLAY)
    {
        return true;
    }

#if defined(ANGLE_PLATFORM_WINDOWS) && !defined(ANGLE_ENABLE_WINDOWS_STORE)
    if (display == EGL_SOFTWARE_DISPLAY_ANGLE ||
        display == EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE ||
        display == EGL_D3D11_ONLY_DISPLAY_ANGLE)
    {
        return true;
    }
    return (WindowFromDC(display) != NULL);
#else
    return true;
#endif
}

void Display::initVendorString()
{
    mVendorString = mImplementation->getVendorString();
}

const DisplayExtensions &Display::getExtensions() const
{
    return mDisplayExtensions;
}

const std::string &Display::getExtensionString() const
{
    return mDisplayExtensionString;
}

const std::string &Display::getVendorString() const
{
    return mVendorString;
}

}
