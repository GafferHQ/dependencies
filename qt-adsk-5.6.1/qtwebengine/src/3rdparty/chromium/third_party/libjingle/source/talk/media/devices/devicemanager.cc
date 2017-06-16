/*
 * libjingle
 * Copyright 2004 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/media/devices/devicemanager.h"

#include "talk/media/base/mediacommon.h"
#include "talk/media/base/videocapturerfactory.h"
#include "talk/media/devices/deviceinfo.h"
#include "talk/media/devices/filevideocapturer.h"
#include "talk/media/devices/yuvframescapturer.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/windowpicker.h"
#include "webrtc/base/windowpickerfactory.h"

#ifdef HAVE_WEBRTC_VIDEO
#include "talk/media/webrtc/webrtcvideocapturerfactory.h"
#endif  // HAVE_WEBRTC_VIDEO

namespace {

bool StringMatchWithWildcard(
    const std::pair<const std::basic_string<char>, cricket::VideoFormat> key,
    const std::string& val) {
  return rtc::string_match(val.c_str(), key.first.c_str());
}

}  // namespace

namespace cricket {

// Initialize to empty string.
const char DeviceManagerInterface::kDefaultDeviceName[] = "";

DeviceManager::DeviceManager()
    : initialized_(false),
      window_picker_(rtc::WindowPickerFactory::CreateWindowPicker()) {
#ifdef HAVE_WEBRTC_VIDEO
  SetVideoDeviceCapturerFactory(new WebRtcVideoDeviceCapturerFactory());
#endif  // HAVE_WEBRTC_VIDEO
}

DeviceManager::~DeviceManager() {
  if (initialized()) {
    Terminate();
  }
}

bool DeviceManager::Init() {
  if (!initialized()) {
    if (!watcher()->Start()) {
      return false;
    }
    set_initialized(true);
  }
  return true;
}

void DeviceManager::Terminate() {
  if (initialized()) {
    watcher()->Stop();
    set_initialized(false);
  }
}

int DeviceManager::GetCapabilities() {
  std::vector<Device> devices;
  int caps = VIDEO_RECV;
  if (GetAudioInputDevices(&devices) && !devices.empty()) {
    caps |= AUDIO_SEND;
  }
  if (GetAudioOutputDevices(&devices) && !devices.empty()) {
    caps |= AUDIO_RECV;
  }
  if (GetVideoCaptureDevices(&devices) && !devices.empty()) {
    caps |= VIDEO_SEND;
  }
  return caps;
}

bool DeviceManager::GetAudioInputDevices(std::vector<Device>* devices) {
  return GetAudioDevices(true, devices);
}

bool DeviceManager::GetAudioOutputDevices(std::vector<Device>* devices) {
  return GetAudioDevices(false, devices);
}

bool DeviceManager::GetAudioInputDevice(const std::string& name, Device* out) {
  return GetAudioDevice(true, name, out);
}

bool DeviceManager::GetAudioOutputDevice(const std::string& name, Device* out) {
  return GetAudioDevice(false, name, out);
}

bool DeviceManager::GetVideoCaptureDevices(std::vector<Device>* devices) {
  devices->clear();
#if defined(ANDROID) || defined(IOS)
  // On Android and iOS, we treat the camera(s) as a single device. Even if
  // there are multiple cameras, that's abstracted away at a higher level.
  Device dev("camera", "1");    // name and ID
  devices->push_back(dev);
  return true;
#else
  return false;
#endif
}

bool DeviceManager::GetVideoCaptureDevice(const std::string& name,
                                          Device* out) {
  // If the name is empty, return the default device.
  if (name.empty() || name == kDefaultDeviceName) {
    return GetDefaultVideoCaptureDevice(out);
  }

  std::vector<Device> devices;
  if (!GetVideoCaptureDevices(&devices)) {
    return false;
  }

  for (std::vector<Device>::const_iterator it = devices.begin();
      it != devices.end(); ++it) {
    if (name == it->name) {
      *out = *it;
      return true;
    }
  }

  // If |name| is a valid name for a file or yuvframedevice,
  // return a fake video capturer device.
  if (GetFakeVideoCaptureDevice(name, out)) {
    return true;
  }

  return false;
}

bool DeviceManager::GetFakeVideoCaptureDevice(const std::string& name,
                                              Device* out) const {
  if (rtc::Filesystem::IsFile(name)) {
    *out = FileVideoCapturer::CreateFileVideoCapturerDevice(name);
    return true;
  }

  if (name == YuvFramesCapturer::kYuvFrameDeviceName) {
    *out = YuvFramesCapturer::CreateYuvFramesCapturerDevice();
    return true;
  }

  return false;
}

void DeviceManager::SetVideoCaptureDeviceMaxFormat(
    const std::string& usb_id,
    const VideoFormat& max_format) {
  max_formats_[usb_id] = max_format;
}

void DeviceManager::ClearVideoCaptureDeviceMaxFormat(
    const std::string& usb_id) {
  max_formats_.erase(usb_id);
}

VideoCapturer* DeviceManager::CreateVideoCapturer(const Device& device) const {
  VideoCapturer* capturer = MaybeConstructFakeVideoCapturer(device);
  if (capturer) {
    return capturer;
  }

  if (!video_device_capturer_factory_) {
    LOG(LS_ERROR) << "No video capturer factory for devices.";
    return NULL;
  }
  capturer = video_device_capturer_factory_->Create(device);
  if (!capturer) {
    return NULL;
  }
  LOG(LS_INFO) << "Created VideoCapturer for " << device.name;
  VideoFormat video_format;
  bool has_max = GetMaxFormat(device, &video_format);
  capturer->set_enable_camera_list(has_max);
  if (has_max) {
    capturer->ConstrainSupportedFormats(video_format);
  }
  return capturer;
}

VideoCapturer* DeviceManager::MaybeConstructFakeVideoCapturer(
    const Device& device) const {
  // TODO(hellner): Throw out the creation of a file video capturer once the
  // refactoring is completed.
  if (FileVideoCapturer::IsFileVideoCapturerDevice(device)) {
    FileVideoCapturer* capturer = new FileVideoCapturer;
    if (!capturer->Init(device)) {
      delete capturer;
      return NULL;
    }
    LOG(LS_INFO) << "Created file video capturer " << device.name;
    capturer->set_repeat(FileVideoCapturer::kForever);
    return capturer;
  }

  if (YuvFramesCapturer::IsYuvFramesCapturerDevice(device)) {
    YuvFramesCapturer* capturer = new YuvFramesCapturer();
    capturer->Init();
    return capturer;
  }
  return NULL;
}

bool DeviceManager::GetWindows(
    std::vector<rtc::WindowDescription>* descriptions) {
  if (!window_picker_) {
    return false;
  }
  return window_picker_->GetWindowList(descriptions);
}

bool DeviceManager::GetDesktops(
    std::vector<rtc::DesktopDescription>* descriptions) {
  if (!window_picker_) {
    return false;
  }
  return window_picker_->GetDesktopList(descriptions);
}

VideoCapturer* DeviceManager::CreateScreenCapturer(
    const ScreencastId& screenid) const {
  if (!screen_capturer_factory_) {
    LOG(LS_ERROR) << "No video capturer factory for screens.";
    return NULL;
  }
  return screen_capturer_factory_->Create(screenid);
}

bool DeviceManager::GetAudioDevices(bool input,
                                    std::vector<Device>* devs) {
  devs->clear();
#if defined(ANDROID)
  // Under Android, 0 is always required for the playout device and 0 is the
  // default for the recording device.
  devs->push_back(Device("default-device", 0));
  return true;
#else
  // Other platforms either have their own derived class implementation
  // (desktop) or don't use device manager for audio devices (iOS).
  return false;
#endif
}

bool DeviceManager::GetAudioDevice(bool is_input, const std::string& name,
                                   Device* out) {
  // If the name is empty, return the default device id.
  if (name.empty() || name == kDefaultDeviceName) {
    *out = Device(name, -1);
    return true;
  }

  std::vector<Device> devices;
  bool ret = is_input ? GetAudioInputDevices(&devices) :
                        GetAudioOutputDevices(&devices);
  if (ret) {
    ret = false;
    for (size_t i = 0; i < devices.size(); ++i) {
      if (devices[i].name == name) {
        *out = devices[i];
        ret = true;
        break;
      }
    }
  }
  return ret;
}

bool DeviceManager::GetDefaultVideoCaptureDevice(Device* device) {
  bool ret = false;
  // We just return the first device.
  std::vector<Device> devices;
  ret = (GetVideoCaptureDevices(&devices) && !devices.empty());
  if (ret) {
    *device = devices[0];
  }
  return ret;
}

bool DeviceManager::IsInWhitelist(const std::string& key,
                                  VideoFormat* video_format) const {
  std::map<std::string, VideoFormat>::const_iterator found =
      std::search_n(max_formats_.begin(), max_formats_.end(), 1, key,
                    StringMatchWithWildcard);
  if (found == max_formats_.end()) {
    return false;
  }
  *video_format = found->second;
  return true;
}

bool DeviceManager::GetMaxFormat(const Device& device,
                                 VideoFormat* video_format) const {
  // Match USB ID if available. Failing that, match device name.
  std::string usb_id;
  if (GetUsbId(device, &usb_id) && IsInWhitelist(usb_id, video_format)) {
      return true;
  }
  return IsInWhitelist(device.name, video_format);
}

bool DeviceManager::ShouldDeviceBeIgnored(const std::string& device_name,
    const char* const exclusion_list[]) {
  // If exclusion_list is empty return directly.
  if (!exclusion_list)
    return false;

  int i = 0;
  while (exclusion_list[i]) {
    if (strnicmp(device_name.c_str(), exclusion_list[i],
        strlen(exclusion_list[i])) == 0) {
      LOG(LS_INFO) << "Ignoring device " << device_name;
      return true;
    }
    ++i;
  }
  return false;
}

bool DeviceManager::FilterDevices(std::vector<Device>* devices,
    const char* const exclusion_list[]) {
  if (!devices) {
    return false;
  }

  for (std::vector<Device>::iterator it = devices->begin();
       it != devices->end(); ) {
    if (ShouldDeviceBeIgnored(it->name, exclusion_list)) {
      it = devices->erase(it);
    } else {
      ++it;
    }
  }
  return true;
}

}  // namespace cricket
