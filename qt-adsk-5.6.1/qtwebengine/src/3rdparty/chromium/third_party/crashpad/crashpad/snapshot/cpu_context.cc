// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "snapshot/cpu_context.h"

#include "base/basictypes.h"
#include "base/logging.h"

namespace crashpad {

// static
uint16_t CPUContextX86::FxsaveToFsaveTagWord(
    uint16_t fsw,
    uint8_t fxsave_tag,
    const CPUContextX86::X87OrMMXRegister st_mm[8]) {
  enum {
    kX87TagValid = 0,
    kX87TagZero,
    kX87TagSpecial,
    kX87TagEmpty,
  };

  // The x87 tag word (in both abridged and full form) identifies physical
  // registers, but |st_mm| is arranged in logical stack order. In order to map
  // physical tag word bits to the logical stack registers they correspond to,
  // the “stack top” value from the x87 status word is necessary.
  int stack_top = (fsw >> 11) & 0x7;

  uint16_t fsave_tag = 0;
  for (int physical_index = 0; physical_index < 8; ++physical_index) {
    bool fxsave_bit = fxsave_tag & (1 << physical_index);
    uint8_t fsave_bits;

    if (fxsave_bit) {
      int st_index = (physical_index + 8 - stack_top) % 8;
      const CPUContextX86::X87Register& st = st_mm[st_index].st;

      uint32_t exponent = ((st[9] & 0x7f) << 8) | st[8];
      if (exponent == 0x7fff) {
        // Infinity, NaN, pseudo-infinity, or pseudo-NaN. If it was important to
        // distinguish between these, the J bit and the M bit (the most
        // significant bit of |fraction|) could be consulted.
        fsave_bits = kX87TagSpecial;
      } else {
        // The integer bit the “J bit”.
        bool integer_bit = st[7] & 0x80;
        if (exponent == 0) {
          uint64_t fraction = ((implicit_cast<uint64_t>(st[7]) & 0x7f) << 56) |
                              (implicit_cast<uint64_t>(st[6]) << 48) |
                              (implicit_cast<uint64_t>(st[5]) << 40) |
                              (implicit_cast<uint64_t>(st[4]) << 32) |
                              (implicit_cast<uint32_t>(st[3]) << 24) |
                              (st[2] << 16) | (st[1] << 8) | st[0];
          if (!integer_bit && fraction == 0) {
            fsave_bits = kX87TagZero;
          } else {
            // Denormal (if the J bit is clear) or pseudo-denormal.
            fsave_bits = kX87TagSpecial;
          }
        } else if (integer_bit) {
          fsave_bits = kX87TagValid;
        } else {
          // Unnormal.
          fsave_bits = kX87TagSpecial;
        }
      }
    } else {
      fsave_bits = kX87TagEmpty;
    }

    fsave_tag |= (fsave_bits << (physical_index * 2));
  }

  return fsave_tag;
}

uint64_t CPUContext::InstructionPointer() const {
  switch (architecture) {
    case kCPUArchitectureX86:
      return x86->eip;
    case kCPUArchitectureX86_64:
      return x86_64->rip;
    default:
      NOTREACHED();
      return ~0ull;
  }
}

}  // namespace crashpad
