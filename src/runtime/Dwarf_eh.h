/*
 MIT License

 Copyright (c) 2018 Andy Best

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#ifndef ELECTRUM_DWARF_EH_H
#define ELECTRUM_DWARF_EH_H

#include "Runtime.h"
#include <unwind.h>
#include "stdint.h"
#include <string>
#include <memory>

extern "C" {
struct ElectrumException {
  EObjectHeader header;
  _Unwind_Exception unwind_exception;
  const char* exception_type;
  void* metadata;
  char message[];
};

}

enum DwarfEHEncoding : uint8_t {

  DW_EH_PE_absptr = 0x0,  /// Absolute pointer (uintptr_t)
  DW_EH_PE_uleb128 = 0x01, /// Unsigned LEB128 encoded
  DW_EH_PE_udata2 = 0x02, /// uint16
  DW_EH_PE_udata4 = 0x03, /// uint32
  DW_EH_PE_udata8 = 0x04, /// uint64
  DW_EH_PE_sleb128 = 0x09, /// Signed LEB128 encoded
  DW_EH_PE_sdata2 = 0x0A, /// int16
  DW_EH_PE_sdata4 = 0x0B, /// int32
  DW_EH_PE_sdata8 = 0x0C, /// int64

  DW_EH_PE_pcrel = 0x10, /// Value is relative to program counter
  DW_EH_PE_datarel = 0x30, /// Value is relative to the beginning of the section

  DW_EH_PE_indirect = 0x80, /// Value is an indirect pointer

  DW_EH_PE_omit = 0xff  /// No value present
};

struct DwarfEHCallsite {
  uintptr_t offset;
  uintptr_t instruction_length;
  uintptr_t landingpad_offset;
  uintptr_t action;
};

struct DwarfLSDATable {
  uintptr_t landingpad_base_ptr;
  DwarfEHEncoding type_table_encoding;
  uint8_t* typetable_ptr;
  uint8_t* action_table_ptr;
  std::vector<DwarfEHCallsite> callsites;
};

size_t _el_rt_eh_decode_ULEB128(const uint8_t* data, uintptr_t* result);
size_t _el_rt_eh_decode_SLEB128(const uint8_t* data, intptr_t* result);
size_t _el_rt_eh_read_encoded_ptr(const uint8_t* data, DwarfEHEncoding encoding, uintptr_t* result);
size_t _el_rt_eh_encoding_size(DwarfEHEncoding enc);

#endif //ELECTRUM_DWARF_EH_H
