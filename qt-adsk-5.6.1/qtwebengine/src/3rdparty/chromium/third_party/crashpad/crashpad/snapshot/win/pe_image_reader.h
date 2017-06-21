// Copyright 2015 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRASHPAD_SNAPSHOT_WIN_PE_IMAGE_READER_H_
#define CRASHPAD_SNAPSHOT_WIN_PE_IMAGE_READER_H_

#include <windows.h>

#include <string>

#include "base/basictypes.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/win/address_types.h"
#include "util/win/checked_win_address_range.h"

namespace crashpad {

class ProcessReaderWin;

namespace process_types {

// TODO(scottmg): Genericize and/or? move process_types out of mac/.
struct CrashpadInfo {
  uint32_t signature;
  uint32_t size;
  uint32_t version;
  uint8_t crashpad_handler_behavior;  // TriState.
  uint8_t system_crash_reporter_forwarding;  // TriState.
  uint16_t padding_0;
  uint64_t simple_annotations;  // TODO(scottmg): x86/64.
};

struct Section {
};

}  // namespace process_types

//! \brief A reader for PE images mapped into another process.
//!
//! This class is capable of reading both 32-bit and 64-bit images based on the
//! bitness of the remote process.
//!
//! \sa PEImageAnnotationsReader
class PEImageReader {
 public:
  PEImageReader();
  ~PEImageReader();

  //! \brief Initializes the reader.
  //!
  //! This method must be called only once on an object. This method must be
  //! called successfully before any other method in this class may be called.
  //!
  //! \param[in] process_reader The reader for the remote process.
  //! \param[in] address The address, in the remote process' address space,
  //!     where the `IMAGE_DOS_HEADER` is located.
  //! \param[in] size The size of the image.
  //! \param[in] name The module's name, a string to be used in logged messages.
  //!     This string is for diagnostic purposes.
  //!
  //! \return `true` if the image was read successfully, `false` otherwise, with
  //!     an appropriate message logged.
  bool Initialize(ProcessReaderWin* process_reader,
                  WinVMAddress address,
                  WinVMSize size,
                  const std::string& module_name);

  //! \brief Returns the image's load address.
  //!
  //! This is the value passed as \a address to Initialize().
  WinVMAddress Address() const { return module_range_.Base(); }

  //! \brief Returns the image's size.
  //!
  //! This is the value passed as \a size to Initialize().
  WinVMSize Size() const { return module_range_.Size(); }

  //! \brief Obtains the module's CrashpadInfo structure.
  //!
  //! \return `true` on success, `false` on failure. If the module does not have
  //!     a `CPADinfo` section, this will return `false` without logging any
  //!     messages. Other failures will result in messages being logged.
  bool GetCrashpadInfo(process_types::CrashpadInfo* crashpad_info) const;

 private:
  //! \brief Finds a given section by name in the image.
  bool GetSectionByName(const std::string& name,
                        IMAGE_SECTION_HEADER* section) const;

  //! \brief Reads memory from target process, first checking whether the range
  //!     requested falls inside module_range_.
  //!
  //! \return `true` on success, with \a into filled out, otherwise `false` and
  //!     a message will be logged.
  bool CheckedReadMemory(WinVMAddress address,
                         WinVMSize size,
                         void* into) const;

  ProcessReaderWin* process_reader_;  // weak
  CheckedWinAddressRange module_range_;
  std::string module_name_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(PEImageReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_WIN_PE_IMAGE_READER_H_
