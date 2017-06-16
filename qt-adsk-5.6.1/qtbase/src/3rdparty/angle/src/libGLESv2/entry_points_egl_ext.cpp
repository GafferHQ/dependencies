//
// Copyright(c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// entry_points_ext.cpp : Implements the EGL extension entry points.

#include "libGLESv2/entry_points_egl_ext.h"
#include "libGLESv2/global_state.h"

#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/validationEGL.h"

#include "common/debug.h"

namespace egl
{

// EGL_ANGLE_query_surface_pointer
EGLBoolean EGLAPIENTRY QuerySurfacePointerANGLE(EGLDisplay dpy, EGLSurface surface, EGLint attribute, void **value)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint attribute = %d, void **value = 0x%0.8p)",
          dpy, surface, attribute, value);

    Display *display = static_cast<Display*>(dpy);
    Surface *eglSurface = static_cast<Surface*>(surface);

    Error error = ValidateSurface(display, eglSurface);
    if (error.isError())
    {
        SetGlobalError(error);
        return EGL_FALSE;
    }

    if (!display->getExtensions().querySurfacePointer)
    {
        SetGlobalError(Error(EGL_SUCCESS));
        return EGL_FALSE;
    }

    if (surface == EGL_NO_SURFACE)
    {
        SetGlobalError(Error(EGL_BAD_SURFACE));
        return EGL_FALSE;
    }

    // validate the attribute parameter
    switch (attribute)
    {
      case EGL_DEVICE_EXT:
        break;
      case EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE:
        if (!display->getExtensions().surfaceD3DTexture2DShareHandle)
        {
            SetGlobalError(Error(EGL_BAD_ATTRIBUTE));
            return EGL_FALSE;
        }
        break;

      default:
        SetGlobalError(Error(EGL_BAD_ATTRIBUTE));
        return EGL_FALSE;
    }

    error = eglSurface->querySurfacePointerANGLE(attribute, value);
    SetGlobalError(error);
    return (error.isError() ? EGL_FALSE : EGL_TRUE);
}


// EGL_NV_post_sub_buffer
EGLBoolean EGLAPIENTRY PostSubBufferNV(EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint x = %d, EGLint y = %d, EGLint width = %d, EGLint height = %d)", dpy, surface, x, y, width, height);

    if (x < 0 || y < 0 || width < 0 || height < 0)
    {
        SetGlobalError(Error(EGL_BAD_PARAMETER));
        return EGL_FALSE;
    }

    Display *display = static_cast<Display*>(dpy);
    Surface *eglSurface = static_cast<Surface*>(surface);

    Error error = ValidateSurface(display, eglSurface);
    if (error.isError())
    {
        SetGlobalError(error);
        return EGL_FALSE;
    }

    if (display->isDeviceLost())
    {
        SetGlobalError(Error(EGL_CONTEXT_LOST));
        return EGL_FALSE;
    }

    if (surface == EGL_NO_SURFACE)
    {
        SetGlobalError(Error(EGL_BAD_SURFACE));
        return EGL_FALSE;
    }

#if !defined(ANGLE_ENABLE_WINDOWS_STORE) || (defined(ANGLE_ENABLE_WINDOWS_STORE) && WINAPI_FAMILY == WINAPI_FAMILY_PC_APP) // Qt WP: Allow this entry point as a workaround
    if (!display->getExtensions().postSubBuffer)
    {
        // Spec is not clear about how this should be handled.
        SetGlobalError(Error(EGL_SUCCESS));
        return EGL_TRUE;
    }
#endif

    error = eglSurface->postSubBuffer(x, y, width, height);
    if (error.isError())
    {
        SetGlobalError(error);
        return EGL_FALSE;
    }

    SetGlobalError(Error(EGL_SUCCESS));
    return EGL_TRUE;
}

// EGL_EXT_platform_base
EGLDisplay EGLAPIENTRY GetPlatformDisplayEXT(EGLenum platform, void *native_display, const EGLint *attrib_list)
{
    EVENT("(EGLenum platform = %d, void* native_display = 0x%0.8p, const EGLint* attrib_list = 0x%0.8p)",
          platform, native_display, attrib_list);

    const ClientExtensions &clientExtensions = Display::getClientExtensions();

    switch (platform)
    {
      case EGL_PLATFORM_ANGLE_ANGLE:
        if (!clientExtensions.platformANGLE)
        {
            SetGlobalError(Error(EGL_BAD_PARAMETER));
            return EGL_NO_DISPLAY;
        }
        break;

      default:
        SetGlobalError(Error(EGL_BAD_CONFIG));
        return EGL_NO_DISPLAY;
    }

    EGLint platformType = EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;
    EGLint deviceType = EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE;
    bool majorVersionSpecified = false;
    bool minorVersionSpecified = false;
    bool enableAutoTrimSpecified = false;

    if (attrib_list)
    {
        for (const EGLint *curAttrib = attrib_list; curAttrib[0] != EGL_NONE; curAttrib += 2)
        {
            switch (curAttrib[0])
            {
              case EGL_PLATFORM_ANGLE_TYPE_ANGLE:
                switch (curAttrib[1])
                {
                  case EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE:
                    break;

                  case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
                  case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
                    if (!clientExtensions.platformANGLED3D)
                    {
                        SetGlobalError(Error(EGL_BAD_ATTRIBUTE));
                        return EGL_NO_DISPLAY;
                    }
                    break;

                  case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
                  case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
                    if (!clientExtensions.platformANGLEOpenGL)
                    {
                        SetGlobalError(Error(EGL_BAD_ATTRIBUTE));
                        return EGL_NO_DISPLAY;
                    }
                    break;

                  default:
                    SetGlobalError(Error(EGL_BAD_ATTRIBUTE));
                    return EGL_NO_DISPLAY;
                }
                platformType = curAttrib[1];
                break;

              case EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE:
                if (curAttrib[1] != EGL_DONT_CARE)
                {
                    majorVersionSpecified = true;
                }
                break;

              case EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE:
                if (curAttrib[1] != EGL_DONT_CARE)
                {
                    minorVersionSpecified = true;
                }
                break;

              case EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE:
                switch (curAttrib[1])
                {
                  case EGL_TRUE:
                  case EGL_FALSE:
                    break;
                  default:
                    SetGlobalError(Error(EGL_BAD_ATTRIBUTE));
                    return EGL_NO_DISPLAY;
                }
                enableAutoTrimSpecified = true;
                break;

              case EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE:
                if (!clientExtensions.platformANGLED3D)
                {
                    SetGlobalError(Error(EGL_BAD_ATTRIBUTE));
                    return EGL_NO_DISPLAY;
                }

                switch (curAttrib[1])
                {
                  case EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE:
                  case EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE:
                  case EGL_PLATFORM_ANGLE_DEVICE_TYPE_REFERENCE_ANGLE:
                  case EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE:
                    break;

                  default:
                    SetGlobalError(Error(EGL_BAD_ATTRIBUTE));
                    return EGL_NO_DISPLAY;
                }
                deviceType = curAttrib[1];
                break;

              default:
                break;
            }
        }
    }

    if (!majorVersionSpecified && minorVersionSpecified)
    {
        SetGlobalError(Error(EGL_BAD_ATTRIBUTE));
        return EGL_NO_DISPLAY;
    }

    if (deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE &&
        platformType != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        SetGlobalError(Error(EGL_BAD_ATTRIBUTE, "EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE requires a device type of "
                                                "EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE."));
        return EGL_NO_DISPLAY;
    }

    if (enableAutoTrimSpecified &&
        platformType != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        SetGlobalError(Error(EGL_BAD_ATTRIBUTE, "EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE requires a device type of "
                                                "EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE."));
        return EGL_NO_DISPLAY;
    }

    SetGlobalError(Error(EGL_SUCCESS));

    EGLNativeDisplayType displayId = static_cast<EGLNativeDisplayType>(native_display);
    return Display::getDisplay(displayId, AttributeMap(attrib_list));
}

}
