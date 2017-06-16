// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsexception_enabler.h"
#include "base/mac/scoped_nsobject.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace ui {

namespace {

// Would be nice if this were in UTCoreTypes.h, but it isn't
NSString* const kUTTypeURLName = @"public.url-name";

// Tells us if WebKit was the last to write to the pasteboard. There's no
// actual data associated with this type.
NSString* const kWebSmartPastePboardType = @"NeXT smart paste pasteboard type";

// Pepper custom data format type.
NSString* const kPepperCustomDataPboardType =
    @"org.chromium.pepper-custom-data";

NSPasteboard* GetPasteboard() {
  // The pasteboard should not be nil in a UI session, but this handy DCHECK
  // can help track down problems if someone tries using clipboard code outside
  // of a UI session.
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  DCHECK(pasteboard);
  return pasteboard;
}

}  // namespace

// Clipboard::FormatType implementation.
Clipboard::FormatType::FormatType() : data_(nil) {
}

Clipboard::FormatType::FormatType(NSString* native_format)
    : data_([native_format retain]) {
}

Clipboard::FormatType::FormatType(const FormatType& other)
    : data_([other.data_ retain]) {
}

Clipboard::FormatType& Clipboard::FormatType::operator=(
    const FormatType& other) {
  if (this != &other) {
    [data_ release];
    data_ = [other.data_ retain];
  }
  return *this;
}

bool Clipboard::FormatType::Equals(const FormatType& other) const {
  return [data_ isEqualToString:other.data_];
}

Clipboard::FormatType::~FormatType() {
  [data_ release];
}

std::string Clipboard::FormatType::Serialize() const {
  return base::SysNSStringToUTF8(data_);
}

// static
Clipboard::FormatType Clipboard::FormatType::Deserialize(
    const std::string& serialization) {
  return FormatType(base::SysUTF8ToNSString(serialization));
}

bool Clipboard::FormatType::operator<(const FormatType& other) const {
  return [data_ compare:other.data_] == NSOrderedAscending;
}

// Various predefined FormatTypes.
// static
Clipboard::FormatType Clipboard::GetFormatType(
    const std::string& format_string) {
  return FormatType::Deserialize(format_string);
}

// static
const Clipboard::FormatType& Clipboard::GetUrlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (NSURLPboardType));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetUrlWFormatType() {
  return GetUrlFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (NSStringPboardType));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType() {
  return GetPlainTextFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetFilenameFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (NSFilenamesPboardType));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetFilenameWFormatType() {
  return GetFilenameFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetHtmlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (NSHTMLPboardType));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetRtfFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (NSRTFPboardType));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetBitmapFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (NSTIFFPboardType));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kWebSmartPastePboardType));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kWebCustomDataPboardType));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPepperCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kPepperCustomDataPboardType));
  return type;
}

// Clipboard factory method.
// static
Clipboard* Clipboard::Create() {
  return new ClipboardMac;
}

// ClipboardMac implementation.
ClipboardMac::ClipboardMac() {
  DCHECK(CalledOnValidThread());
}

ClipboardMac::~ClipboardMac() {
  DCHECK(CalledOnValidThread());
}

uint64 ClipboardMac::GetSequenceNumber(ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);

  NSPasteboard* pb = GetPasteboard();
  return [pb changeCount];
}

bool ClipboardMac::IsFormatAvailable(const FormatType& format,
                                     ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);

  NSPasteboard* pb = GetPasteboard();
  NSArray* types = [pb types];

  // Safari only places RTF on the pasteboard, never HTML. We can convert RTF
  // to HTML, so the presence of either indicates success when looking for HTML.
  if ([format.ToNSString() isEqualToString:NSHTMLPboardType]) {
    return [types containsObject:NSHTMLPboardType] ||
           [types containsObject:NSRTFPboardType];
  }
  return [types containsObject:format.ToNSString()];
}

void ClipboardMac::Clear(ClipboardType type) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);

  NSPasteboard* pb = GetPasteboard();
  [pb declareTypes:[NSArray array] owner:nil];
}

void ClipboardMac::ReadAvailableTypes(ClipboardType type,
                                      std::vector<base::string16>* types,
                                      bool* contains_filenames) const {
  DCHECK(CalledOnValidThread());
  types->clear();
  if (IsFormatAvailable(Clipboard::GetPlainTextFormatType(), type))
    types->push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (IsFormatAvailable(Clipboard::GetHtmlFormatType(), type))
    types->push_back(base::UTF8ToUTF16(kMimeTypeHTML));
  if (IsFormatAvailable(Clipboard::GetRtfFormatType(), type))
    types->push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  if ([NSImage canInitWithPasteboard:GetPasteboard()])
    types->push_back(base::UTF8ToUTF16(kMimeTypePNG));
  *contains_filenames = false;

  NSPasteboard* pb = GetPasteboard();
  if ([[pb types] containsObject:kWebCustomDataPboardType]) {
    NSData* data = [pb dataForType:kWebCustomDataPboardType];
    if ([data length])
      ReadCustomDataTypes([data bytes], [data length], types);
  }
}

void ClipboardMac::ReadText(ClipboardType type, base::string16* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  NSPasteboard* pb = GetPasteboard();
  NSString* contents = [pb stringForType:NSStringPboardType];

  *result = base::SysNSStringToUTF16(contents);
}

void ClipboardMac::ReadAsciiText(ClipboardType type,
                                 std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  NSPasteboard* pb = GetPasteboard();
  NSString* contents = [pb stringForType:NSStringPboardType];

  if (!contents)
    result->clear();
  else
    result->assign([contents UTF8String]);
}

void ClipboardMac::ReadHTML(ClipboardType type,
                            base::string16* markup,
                            std::string* src_url,
                            uint32* fragment_start,
                            uint32* fragment_end) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);

  // TODO(avi): src_url?
  markup->clear();
  if (src_url)
    src_url->clear();

  NSPasteboard* pb = GetPasteboard();
  NSArray* supportedTypes = [NSArray arrayWithObjects:NSHTMLPboardType,
                                                      NSRTFPboardType,
                                                      NSStringPboardType,
                                                      nil];
  NSString* bestType = [pb availableTypeFromArray:supportedTypes];
  if (bestType) {
    NSString* contents = [pb stringForType:bestType];
    if ([bestType isEqualToString:NSRTFPboardType])
      contents = [pb htmlFromRtf];
    *markup = base::SysNSStringToUTF16(contents);
  }

  *fragment_start = 0;
  DCHECK(markup->length() <= kuint32max);
  *fragment_end = static_cast<uint32>(markup->length());
}

void ClipboardMac::ReadRTF(ClipboardType type, std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);

  return ReadData(GetRtfFormatType(), result);
}

SkBitmap ClipboardMac::ReadImage(ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);

  // If the pasteboard's image data is not to its liking, the guts of NSImage
  // may throw, and that exception will leak. Prevent a crash in that case;
  // a blank image is better.
  base::scoped_nsobject<NSImage> image(base::mac::RunBlockIgnoringExceptions(^{
      return [[NSImage alloc] initWithPasteboard:GetPasteboard()];
  }));
  SkBitmap bitmap;
  if (image.get()) {
    bitmap = gfx::NSImageToSkBitmapWithColorSpace(
        image.get(), /*is_opaque=*/ false, base::mac::GetSystemColorSpace());
  }
  return bitmap;
}

void ClipboardMac::ReadCustomData(ClipboardType clipboard_type,
                                  const base::string16& type,
                                  base::string16* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(clipboard_type, CLIPBOARD_TYPE_COPY_PASTE);

  NSPasteboard* pb = GetPasteboard();
  if ([[pb types] containsObject:kWebCustomDataPboardType]) {
    NSData* data = [pb dataForType:kWebCustomDataPboardType];
    if ([data length])
      ReadCustomDataForType([data bytes], [data length], type, result);
  }
}

void ClipboardMac::ReadBookmark(base::string16* title, std::string* url) const {
  DCHECK(CalledOnValidThread());
  NSPasteboard* pb = GetPasteboard();

  if (title) {
    NSString* contents = [pb stringForType:kUTTypeURLName];
    *title = base::SysNSStringToUTF16(contents);
  }

  if (url) {
    NSString* url_string = [[NSURL URLFromPasteboard:pb] absoluteString];
    if (!url_string)
      url->clear();
    else
      url->assign([url_string UTF8String]);
  }
}

void ClipboardMac::ReadData(const FormatType& format,
                            std::string* result) const {
  DCHECK(CalledOnValidThread());
  NSPasteboard* pb = GetPasteboard();
  NSData* data = [pb dataForType:format.ToNSString()];
  if ([data length])
    result->assign(static_cast<const char*>([data bytes]), [data length]);
}

void ClipboardMac::WriteObjects(ClipboardType type, const ObjectMap& objects) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);

  NSPasteboard* pb = GetPasteboard();
  [pb declareTypes:[NSArray array] owner:nil];

  for (ObjectMap::const_iterator iter = objects.begin(); iter != objects.end();
       ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }
}

void ClipboardMac::WriteText(const char* text_data, size_t text_len) {
  std::string text_str(text_data, text_len);
  NSString* text = base::SysUTF8ToNSString(text_str);
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
  [pb setString:text forType:NSStringPboardType];
}

void ClipboardMac::WriteHTML(const char* markup_data,
                             size_t markup_len,
                             const char* url_data,
                             size_t url_len) {
  // We need to mark it as utf-8. (see crbug.com/11957)
  std::string html_fragment_str("<meta charset='utf-8'>");
  html_fragment_str.append(markup_data, markup_len);
  NSString* html_fragment = base::SysUTF8ToNSString(html_fragment_str);

  // TODO(avi): url_data?
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:[NSArray arrayWithObject:NSHTMLPboardType] owner:nil];
  [pb setString:html_fragment forType:NSHTMLPboardType];
}

void ClipboardMac::WriteRTF(const char* rtf_data, size_t data_len) {
  WriteData(GetRtfFormatType(), rtf_data, data_len);
}

void ClipboardMac::WriteBookmark(const char* title_data,
                                 size_t title_len,
                                 const char* url_data,
                                 size_t url_len) {
  std::string title_str(title_data, title_len);
  NSString* title = base::SysUTF8ToNSString(title_str);
  std::string url_str(url_data, url_len);
  NSString* url = base::SysUTF8ToNSString(url_str);

  // TODO(playmobil): In the Windows version of this function, an HTML
  // representation of the bookmark is also added to the clipboard, to support
  // drag and drop of web shortcuts.  I don't think we need to do this on the
  // Mac, but we should double check later on.
  NSURL* nsurl = [NSURL URLWithString:url];

  NSPasteboard* pb = GetPasteboard();
  // passing UTIs into the pasteboard methods is valid >= 10.5
  [pb addTypes:[NSArray arrayWithObjects:NSURLPboardType, kUTTypeURLName, nil]
         owner:nil];
  [nsurl writeToPasteboard:pb];
  [pb setString:title forType:kUTTypeURLName];
}

void ClipboardMac::WriteBitmap(const SkBitmap& bitmap) {
  NSImage* image = gfx::SkBitmapToNSImageWithColorSpace(
      bitmap, base::mac::GetSystemColorSpace());
  // An API to ask the NSImage to write itself to the clipboard comes in 10.6 :(
  // For now, spit out the image as a TIFF.
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:[NSArray arrayWithObject:NSTIFFPboardType] owner:nil];
  NSData* tiff_data = [image TIFFRepresentation];
  LOG_IF(ERROR, tiff_data == NULL) << "Failed to allocate image for clipboard";
  if (tiff_data) {
    [pb setData:tiff_data forType:NSTIFFPboardType];
  }
}

void ClipboardMac::WriteData(const FormatType& format,
                             const char* data_data,
                             size_t data_len) {
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:[NSArray arrayWithObject:format.ToNSString()] owner:nil];
  [pb setData:[NSData dataWithBytes:data_data length:data_len]
      forType:format.ToNSString()];
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardMac::WriteWebSmartPaste() {
  NSPasteboard* pb = GetPasteboard();
  NSString* format = GetWebKitSmartPasteFormatType().ToNSString();
  [pb addTypes:[NSArray arrayWithObject:format] owner:nil];
  [pb setData:nil forType:format];
}

}  // namespace ui
