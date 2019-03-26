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

#include "Dwarf_eh.h"
#include <unwind.h>
#include <cstdlib>
#include <cstring>

#if __APPLE__
// This type definition doesn't seem to exist in MacOS' unwind.h
typedef uint64_t _Unwind_Exception_Class;
#endif

constexpr _Unwind_Exception_Class make_exception_class(const char* c) {
    _Unwind_Exception_Class ec = 0;
    for (unsigned int i = 0; i<sizeof(_Unwind_Exception_Class); ++i) {
        ec += c[i] << (i*8L);
    }

    return ec;
}

const _Unwind_Exception_Class el_exception_class = make_exception_class("ELECELEC");

extern "C" void* el_rt_allocate_exception(const char* exc_type, void* meta) {
    auto exc = static_cast<ElectrumException*>(GC_MALLOC(sizeof(ElectrumException)));
    exc->header.gc_mark = 0;
    exc->header.tag = kETypeTagException;

    exc->exception_type = exc_type;
    exc->metadata = meta;

    exc->unwind_exception.exception_class = el_exception_class;
    return static_cast<void*>(exc);
}

extern "C" void el_rt_throw(void* thrown_exception) {
    auto exc = static_cast<ElectrumException*>(thrown_exception);
    _Unwind_RaiseException(&exc->unwind_exception);

    // If the exception was caught, this point won't ever be reached
    exit(1);
}

size_t _el_rt_eh_decode_ULEB128(const uint8_t* data, uintptr_t* result) {
    unsigned int shift = 0;
    size_t offset = 0;
    *result = 0;

    while (true) {
        uint8_t byte = data[offset];
        *result |= (byte & 0x7F) << shift;

        shift += 7;
        ++offset;

        if ((byte & 0x80)==0) {
            break;
        }
    }

    return offset;
}

size_t _el_rt_eh_decode_SLEB128(const uint8_t* data, intptr_t* result) {
    unsigned int shift = 0;
    size_t offset = 0;
    *result = 0;

    uint8_t byte;

    do {
        byte = data[offset];
        *result |= (byte & 0x7F) << shift;
        shift += 7;
        ++offset;
    }
    while ((byte & 0x80)!=0);

    if ((shift<((sizeof(intptr_t)*8)-1)) && ((byte & 0x40)>0)) {
        *result |= (~0 << shift);
    }

    return offset;
}

size_t _el_rt_eh_read_encoded_ptr(const uint8_t* data, DwarfEHEncoding encoding, uintptr_t* result) {
    *result = 0;

    if (encoding==DW_EH_PE_omit) {
        return 1;
    }

    size_t size;

    switch (encoding & 0xF) {
    case DW_EH_PE_uleb128: {
        size = _el_rt_eh_decode_ULEB128(data, result);
        break;
    }
    case DW_EH_PE_sleb128: {
        intptr_t v;
        size_t r = _el_rt_eh_decode_SLEB128(data, &v);
        *result = static_cast<uintptr_t>(v);
        size = r;
        break;
    }
    case DW_EH_PE_udata2: {
        uint16_t v;
        memcpy(&v, data, sizeof(uint16_t));
        *result = static_cast<uintptr_t>(v);
        size = 2;
        break;
    }
    case DW_EH_PE_udata4: {
        uint32_t v;
        memcpy(&v, data, sizeof(uint32_t));
        *result = static_cast<uintptr_t>(v);
        size = 4;
        break;
    }
    case DW_EH_PE_udata8: {
        uint64_t v;
        memcpy(&v, data, sizeof(uint64_t));
        *result = static_cast<uintptr_t>(v);
        size = 8;
        break;
    }
    case DW_EH_PE_sdata2: {
        int16_t v;
        memcpy(&v, data, sizeof(int16_t));
        *result = static_cast<uintptr_t>(v);
        size = 2;
        break;
    }
    case DW_EH_PE_sdata4: {
        int32_t v;
        memcpy(&v, data, sizeof(int32_t));
        *result = static_cast<uintptr_t>(v);
        size = 4;
        break;
    }
    case DW_EH_PE_sdata8: {
        int64_t v;
        memcpy(&v, data, sizeof(int64_t));
        *result = static_cast<uintptr_t>(v);
        size = 8;
        break;
    }
    case DW_EH_PE_absptr: {
        memcpy(result, data, sizeof(uintptr_t));
        size = sizeof(uintptr_t);
        break;
    }
    default: {
        printf("Unsupported Dwarf encoding type!");
        exit(1);
    }
    }

    switch (encoding & 0x70) {
    case DW_EH_PE_pcrel: {
        *result += (uintptr_t) data;
    }
    default: break;
    }

    // If it is an indirect pointer, follow it
    if((encoding & 0x80) == DW_EH_PE_indirect) {
        *result = *((uintptr_t*)*result);
    }
    return size;
}

size_t _el_rt_eh_encoding_size(DwarfEHEncoding enc) {
    switch (enc & 0xF) {
    case DW_EH_PE_udata2:
    case DW_EH_PE_sdata2:return 2;
    case DW_EH_PE_udata4:
    case DW_EH_PE_sdata4:return 4;
    case DW_EH_PE_udata8:
    case DW_EH_PE_sdata8:return 8;
    case DW_EH_PE_absptr:return sizeof(void*);
    default: return 0;  // All other encodings are variable size
    }
}

ElectrumException* get_exception_object_from_info(_Unwind_Exception const* exception_info) {
    auto offset = offsetof(ElectrumException, unwind_exception);
    return reinterpret_cast<ElectrumException*>(
            reinterpret_cast<uintptr_t const>( exception_info )-offset
    );
}

extern "C" int el_rt_exception_matches(char *exception_type, char *match) {
    return strcmp(exception_type, match) == 0;
}

bool _el_rt_load_lsda(_Unwind_Context* const context, DwarfLSDATable* t) {
    auto* const lsda = reinterpret_cast<uint8_t* const>(_Unwind_GetLanguageSpecificData(context));

    if (lsda==nullptr) {
        return false;
    }

    uint8_t* lsda_ptr = lsda;

    // Landing pad base
    auto lpbase_enc = (DwarfEHEncoding) *lsda_ptr++;


    uintptr_t landingpad_base;

    if (lpbase_enc==DW_EH_PE_omit) {
        landingpad_base = _Unwind_GetRegionStart(context);
    }
    else {
        lsda_ptr += _el_rt_eh_read_encoded_ptr(lsda_ptr, lpbase_enc, &landingpad_base);
    }

    // Type table
    auto tt_enc = (DwarfEHEncoding) *lsda_ptr++;
    uint8_t* typetable_ptr = nullptr;

    if (tt_enc!=DW_EH_PE_omit) {
        // Type table is represented as an offset from the start of the field
        uintptr_t res;
        lsda_ptr += _el_rt_eh_decode_ULEB128(lsda_ptr, &res);
        typetable_ptr = lsda_ptr+res;
    }

    // Call site table
    auto cst_enc = (DwarfEHEncoding) *lsda_ptr++;

    uintptr_t callsite_table_length;
    lsda_ptr += _el_rt_eh_decode_ULEB128(lsda_ptr, &callsite_table_length);

    std::vector<DwarfEHCallsite> callsites;

    uintptr_t cs_remaining = callsite_table_length;
    while (cs_remaining>0) {
        auto start = reinterpret_cast<uintptr_t>(lsda_ptr);

        DwarfEHCallsite cs;

        lsda_ptr += _el_rt_eh_read_encoded_ptr(lsda_ptr, cst_enc, &cs.offset);
        lsda_ptr += _el_rt_eh_read_encoded_ptr(lsda_ptr, cst_enc, &cs.instruction_length);
        lsda_ptr += _el_rt_eh_read_encoded_ptr(lsda_ptr, cst_enc, &cs.landingpad_offset);
        lsda_ptr += _el_rt_eh_decode_ULEB128(lsda_ptr, &cs.action);

        callsites.push_back(cs);

        cs_remaining -= reinterpret_cast<uintptr_t>(lsda_ptr)-start;
    }

    uint8_t* action_table_ptr = lsda_ptr;

    t->landingpad_base_ptr = landingpad_base;
    t->typetable_ptr = typetable_ptr;
    t->action_table_ptr = action_table_ptr;
    t->callsites = callsites;
    t->type_table_encoding = tt_enc;

    return true;
}

bool _el_rt_cs_matches(_Unwind_Context* context,
        DwarfLSDATable* table,
        DwarfEHCallsite* callsite,
        ElectrumException* exception) {
    if(callsite->action == 0) {
        return false;
    }

    // Initial offset into the action table
    auto action_ptr = table->action_table_ptr + callsite->action - 1;

    intptr_t type_info_offset = 0;
    intptr_t action_offset = 0;
    uint8_t* last_action_ptr = 0;

    action_ptr += _el_rt_eh_decode_SLEB128(action_ptr, &type_info_offset);
    last_action_ptr = action_ptr;
    action_ptr += _el_rt_eh_decode_SLEB128(action_ptr, &action_offset);


    auto encoding_size = _el_rt_eh_encoding_size(table->type_table_encoding);

    while(true) {
        if(type_info_offset != 0) {
            auto type_ptr = (((uint8_t*) table->typetable_ptr)-type_info_offset*encoding_size);
            const char* type_info_ptr;
            _el_rt_eh_read_encoded_ptr(type_ptr, table->type_table_encoding, (uintptr_t*) &type_info_ptr);

            if (strcmp(type_info_ptr, exception->exception_type)==0) {
                return true;
            }

        } else {
            break;
        }

        if (action_offset==0) {
            break;
        }

        action_ptr = last_action_ptr + action_offset;
        action_ptr += _el_rt_eh_decode_SLEB128(action_ptr, &type_info_offset);
        last_action_ptr = action_ptr;
    }

    return false;
}

/// Checks if the action records attached to this call site match the exception type
_Unwind_Reason_Code _el_rt_cs_perform_actions(_Unwind_Context* context,
        DwarfLSDATable* table,
        DwarfEHCallsite* callsite,
        ElectrumException* exception) {
    if(callsite->action == 0) {
        return _URC_CONTINUE_UNWIND;
    }

    // Initial offset into the action table
    auto action_ptr = table->action_table_ptr + callsite->action - 1;

    intptr_t type_info_offset = 0;
    intptr_t action_offset = 0;
    uint8_t* last_action_ptr = 0;

    action_ptr += _el_rt_eh_decode_SLEB128(action_ptr, &type_info_offset);
    last_action_ptr = action_ptr;
    action_ptr += _el_rt_eh_decode_SLEB128(action_ptr, &action_offset);

    auto encoding_size = _el_rt_eh_encoding_size(table->type_table_encoding);

    while(true) {
        auto type_ptr = (((uint8_t*)table->typetable_ptr) - type_info_offset * encoding_size);
        const char *type_info_ptr;
        _el_rt_eh_read_encoded_ptr(type_ptr, table->type_table_encoding, reinterpret_cast<uintptr_t*>(&type_info_ptr));

        if(strcmp(type_info_ptr, exception->exception_type) == 0) {
            return _URC_INSTALL_CONTEXT;
        }

        if(action_offset == 0) {
            break;
        }

        action_ptr = last_action_ptr + action_offset;
        action_ptr += _el_rt_eh_decode_SLEB128(action_ptr, &type_info_offset);
        last_action_ptr = action_ptr;
    }

    return _URC_CONTINUE_UNWIND;
}

extern "C" _Unwind_Reason_Code el_rt_eh_personality(
        int32_t version,
        _Unwind_Action actions,
        _Unwind_Exception_Class exception_class,
        _Unwind_Exception* exception_info,
        _Unwind_Context* context) {

    if (exception_info==nullptr || context==nullptr) {
        return _URC_FATAL_PHASE1_ERROR;
    }

    auto is_native_exception = exception_class == el_exception_class;

    if (actions & _UA_SEARCH_PHASE) {
        DwarfLSDATable t;

        if (!is_native_exception || !_el_rt_load_lsda(context, &t)) {
            return _URC_CONTINUE_UNWIND;
        }

        auto exc = get_exception_object_from_info(exception_info);

        for (auto& cs: t.callsites) {
            // Check actions for each callsite
            if (_el_rt_cs_matches(context, &t, &cs, exc)) {
                return _URC_HANDLER_FOUND;
            }
        }

        return _URC_CONTINUE_UNWIND;
    }
    else if (actions & _UA_CLEANUP_PHASE) {

        // Is this the handler frame that was marked during the search phase?
        if (actions & _UA_HANDLER_FRAME) {
            DwarfLSDATable t;

            if (!is_native_exception || !_el_rt_load_lsda(context, &t)) {
                return _URC_FATAL_PHASE2_ERROR;
            }

            auto exc = get_exception_object_from_info(exception_info);

            for (auto& cs: t.callsites) {
                if(_el_rt_cs_perform_actions(context, &t, &cs, exc) == _URC_INSTALL_CONTEXT) {
                    //_Unwind_SetGR(context, 0, (uintptr_t) exception_info);
                    _Unwind_SetGR(context, 0, (uintptr_t) exc->exception_type);

                    _Unwind_SetIP(context, t.landingpad_base_ptr+cs.landingpad_offset);
                    return _URC_INSTALL_CONTEXT;
                }
            }

            return _URC_FATAL_PHASE2_ERROR;
        }
        else {
            // TODO: Perform cleanup?

            return _URC_CONTINUE_UNWIND;
        }
    }

    // This point should never be reached
    return _URC_FATAL_PHASE1_ERROR;
}

void _el_rt_begin_catch() {
    printf("Begin catch!\n");
}

void _el_rt_end_catch() {
    printf("End catch!\n");
}
