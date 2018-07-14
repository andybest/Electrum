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

#ifndef ELECTRUM_RUNTIME_H
#define ELECTRUM_RUNTIME_H

#include <cstdint>

#define TAG_MASK    0xFU
#define OBJECT_TAG  0x1U
#define INTEGER_TAG 0x0U
#define BOOLEAN_TAG 0x3U
#define TRUE_TAG    0x13U
#define FALSE_TAG   0x3U

#define TAG_TO_OBJECT(x)    reinterpret_cast<EObjectHeader*>(reinterpret_cast<uintptr_t>(x) & ~TAG_MASK)
#define OBJECT_TO_TAG(x)    reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(x) & OBJECT_TAG)
#define TAG_TO_INTEGER(x)   (reinterpret_cast<intptr_t>(x) >> 1)
#define INTEGER_TO_TAG(x)   reinterpret_cast<void *>(x << 1)

#define TRUE_PTR    reinterpret_cast<void *>(TRUE_TAG)
#define FALSE_PTR   reinterpret_cast<void *>(FALSE_TAG)
#define TO_TAGGED_BOOLEAN(pred)     (pred) ? TRUE_PTR : FALSE_PTR;

enum ETypeTag: uint64_t {
    kETypeTagInteger,
    kETypeTagFloat,
    kETypeTagString,
    kETypeTagSymbol,
    kETypeTagBoolean
};

struct EObjectHeader {
    uint32_t tag;
    uint32_t gc_mark;
};

struct EBoolean {
    EObjectHeader header;
    uint8_t booleanValue;
};

struct EInteger {
    EObjectHeader header;
    int64_t intValue;
};

struct EFloat {
    EObjectHeader header;
    double floatValue;
};

struct EString {
    EObjectHeader header;
    uint64_t length;
    char *stringValue;
};

struct ESymbol {
    EObjectHeader header;
    uint64_t length;
    char *name;
};

/*EBoolean *rt_make_boolean(uint8_t booleanValue);
uint64_t rt_is_boolean(EObjectHeader *val);
EInteger *rt_make_integer(int64_t value);
uint64_t rt_is_integer(EObjectHeader *val);
EFloat *rt_make_float(double value);
ESymbol *rt_make_symbol(char *name);*/

#endif //ELECTRUM_RUNTIME_H
