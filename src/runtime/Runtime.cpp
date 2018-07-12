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
#include <cstdlib>

#define GC_MALLOC malloc

EBoolean *make_boolean(uint8_t booleanValue) {
    EBoolean *boolVal = static_cast<EBoolean *>(GC_MALLOC(sizeof(EInteger)));
    boolVal->header.tag = kETypeTagBoolean;
    boolVal->booleanValue = booleanValue;
    return boolVal;
}

uint64_t is_boolean(EObjectHeader *val) {
    return static_cast<uint64_t>(val->tag == kETypeTagBoolean);
}

EInteger *make_integer(int64_t value) {
    EInteger *intVal = static_cast<EInteger *>(GC_MALLOC(sizeof(EInteger)));
    intVal->header.tag = kETypeTagInteger;
    intVal->intValue = value;
    return intVal;
}

uint64_t is_integer(EObjectHeader *val) {
    return static_cast<uint64_t>(val->tag == kETypeTagInteger);
}

EFloat *make_float(double value) {
    EFloat *intVal = static_cast<EFloat *>(GC_MALLOC(sizeof(EFloat)));
    intVal->header.tag = kETypeTagFloat;
    intVal->floatValue = value;
    return intVal;
}
