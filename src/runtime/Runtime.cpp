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

#include "Runtime.h"
#include "ENamespace.h"
#include <cstdlib>
#include <cstring>

#define GC_MALLOC malloc


bool is_object(void *val) {
    return (reinterpret_cast<uintptr_t>(val) & TAG_MASK) == OBJECT_TAG;
}

bool is_integer(void *val) {
    return (reinterpret_cast<uintptr_t>(val) & TAG_MASK) == INTEGER_TAG;
}

bool is_boolean(void *val) {
    return (reinterpret_cast<uintptr_t>(val) & TAG_MASK) == BOOLEAN_TAG;
}

bool is_object_with_tag(void *val, uint64_t tag) {
    if (is_object(val)) {
        auto header = TAG_TO_OBJECT(val);
        return header->tag == tag;
    }

    return false;
}

extern "C" {

void rt_init() {
    electrum::init_global_namespaces();
}

void *rt_make_boolean(uint8_t booleanValue) {
    return (booleanValue) ? TRUE_PTR : FALSE_PTR;
}

void *rt_is_boolean(void *val) {
    return TO_TAGGED_BOOLEAN(is_boolean(val));
}

void *rt_make_integer(int64_t value) {

    auto intVal = static_cast<EInteger *>(GC_MALLOC(sizeof(EInteger)));
    intVal->header.tag = kETypeTagInteger;
    intVal->intValue = value;
    return OBJECT_TO_TAG(intVal);
}

void *rt_is_integer(void *val) {
    return TO_TAGGED_BOOLEAN(is_integer(val));
}

void *rt_make_float(double value) {
    auto floatVal = static_cast<EFloat *>(GC_MALLOC(sizeof(EFloat)));
    floatVal->header.tag = kETypeTagFloat;
    floatVal->floatValue = value;
    return OBJECT_TO_TAG(floatVal);
}

void *rt_is_float(void *val) {
    return TO_TAGGED_BOOLEAN(is_object_with_tag(val, kETypeTagFloat));
}

ESymbol *rt_make_symbol(char *name) {
    auto *symbolVal = static_cast<ESymbol *>(GC_MALLOC(sizeof(ESymbol)));
    symbolVal->header.tag = kETypeTagSymbol;
    symbolVal->name = static_cast<char *>(GC_MALLOC(strlen(name)));
    strcpy(symbolVal->name, name);
    return symbolVal;
}

void *rt_is_symbol(void *val) {
    return TO_TAGGED_BOOLEAN(is_object_with_tag(val, kETypeTagSymbol));
}

} /* extern "C" */