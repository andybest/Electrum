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

#include <stdint.h>
#include <stddef.h>
#include "GarbageCollector.h"

#define TAG_MASK    0xFU
#define OBJECT_TAG  0x1U
#define INTEGER_TAG 0x0U
#define BOOLEAN_TAG 0x3U
#define TRUE_TAG    0x13U
#define FALSE_TAG   0x3U
#define NIL_TAG     0xFU

#define TAG_TO_OBJECT(x)    reinterpret_cast<EObjectHeader*>(reinterpret_cast<uintptr_t>(x) & ~((uintptr_t)TAG_MASK))
#define OBJECT_TO_TAG(x)    reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(x) | OBJECT_TAG)
#define TAG_TO_INTEGER(x)   (reinterpret_cast<intptr_t>(x) >> 1)
#define INTEGER_TO_TAG(x)   reinterpret_cast<void *>(x << 1)

#define NIL_PTR     reinterpret_cast<void *>(NIL_TAG)
#define TRUE_PTR    reinterpret_cast<void *>(TRUE_TAG)
#define FALSE_PTR   reinterpret_cast<void *>(FALSE_TAG)
#define TO_TAGGED_BOOLEAN(pred)     (pred) ? TRUE_PTR : FALSE_PTR

#define GC_MALLOC rt_gc_malloc_tagged_object

/**
 * Type tags for objects
 */
enum ETypeTag : uint64_t {
  kETypeTagFloat,
  kETypeTagString,
  kETypeTagSymbol,
  kETypeTagKeyword,
  kETypeTagPair,
  kETypeTagFunction,
  kETypeTagInterpretedFunction,
  kETypeTagEnvironment,
  kETypeTagVar,
  kETypeTagException
};

struct EObjectHeader {
  uint32_t tag;
  uint32_t gc_mark;
};

struct EFloat {
  EObjectHeader header;
  double        floatValue;
};

struct EString {
  EObjectHeader header;
  uint64_t      length;
  char          stringValue[];
};

struct ESymbol {
  EObjectHeader header;
  uint64_t      length;
  char          name[];
};

struct EKeyword {
  EObjectHeader header;
  uint64_t      length;
  char          name[];
};

struct EPair {
  EObjectHeader header;
  void* value;
  void* next;
};

struct EVar {
  EObjectHeader header;
  void* sym;
  void* val;
};

struct ECompiledFunction {
  EObjectHeader header;
  uint32_t      arity;
  uint32_t      has_rest_args;

  /** Pointer to function implementation */
  void* f_ptr;

  uint64_t env_size;

  /** Closure environment */
  void* env[];
};

struct EInterpretedFunction {
  EObjectHeader header;
  uint64_t      arity;

  /** Argument names- a list of symbols */
  void* argnames;

  /** Body- A list of forms */
  void* body;

  /** Closure environment */
  void* env;
};

struct EEnvironment {
  EObjectHeader header;
  void* parent;
  /** A list comprising of symbols followed by values */
  void* values;
};

namespace electrum {
bool is_object(void* val);

bool is_integer(void* val);

bool is_boolean(void* val);

bool is_object_with_tag(void* val, uint64_t tag);

bool symbol_equal(void* s1, void* s2);

void print_expr(void* expr);

std::string kind_for_obj(void* obj);

std::string description_for_obj(void* obj);
}


//extern "C" {

void rt_init();
void rt_init_gc(electrum::GCMode gc_mode);
void rt_deinit_gc();

electrum::GarbageCollector* rt_get_gc();

void* rt_is_object(void* val);

extern "C" void* rt_make_boolean(int8_t booleanValue);
extern "C" void* rt_is_boolean(void* val);

extern "C" void* rt_make_integer(int64_t value);
extern "C" void* rt_is_integer(void* val);
extern "C" int64_t rt_integer_value(void* val);

extern "C" void* rt_make_float(double value);
extern "C" void* rt_is_float(void* val);
extern "C" double rt_float_value(void* val);

extern "C" void* rt_make_symbol(const char* name);
extern "C" void* rt_is_symbol(void* val);
extern "C" const char* rt_symbol_extract_string(void* val);

extern "C" void* rt_make_keyword(const char* name);
extern "C" void* rt_is_keyword(void* val);
extern "C" const char* rt_keyword_extract_string(void* val);

extern "C" void* rt_make_string(const char* str);
extern "C" void* rt_is_string(void* val);
extern "C" const char* rt_string_value(void* val);

extern "C" void* rt_make_var(void* sym);
extern "C" void* rt_is_var(void* v);
extern "C" void rt_set_var(void* v, void* val);
extern "C" void* rt_deref_var(void* v);

extern "C" void* rt_is_pair(void* value);
extern "C" void* rt_make_pair(void* value, void* next);
extern "C" void* rt_car(void* pair);
extern "C" void* rt_cdr(void* pair);
extern "C" void* rt_set_car(void* pair, void* val);
extern "C" void* rt_set_cdr(void* pair, void* next);

void* rt_make_interpreted_function(void* argnames, uint64_t arity, void* body, void* env);
void* rt_make_environment(void* parent);
void* rt_environment_add(void* env, void* binding, void* value);
void* rt_environment_get(void* env, void* binding);
void* rt_gc_malloc_tagged_object(size_t size);
extern "C" void rt_gc_add_root(void* obj);

extern "C" void el_rt_throw(void* exception);
extern "C" void* el_rt_allocate_exception(const char* exc_type, const char* message, void* meta);
extern "C" void* el_rt_make_exception(void* exc_type, void* message, void* meta);

extern "C" void* rt_apply_0(void*);
extern "C" void* rt_apply_1(void*, void*);
extern "C" void* rt_apply_2(void*, void*, void*);
extern "C" void* rt_apply_3(void*, void*, void*, void*);
extern "C" void* rt_apply_4(void*, void*, void*, void*, void*);
extern "C" void* rt_apply_5(void*, void*, void*, void*, void*, void*);
extern "C" void* rt_apply_6(void*, void*, void*, void*, void*, void*, void*);
extern "C" void* rt_apply_7(void*, void*, void*, void*, void*, void*, void*, void*);
extern "C" void* rt_apply_8(void*, void*, void*, void*, void*, void*, void*, void*, void*);
extern "C" void* rt_apply_9(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
extern "C" void* rt_apply_10(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
extern "C" void* rt_apply_11(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
extern "C" void* rt_apply_12(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
extern "C" void* rt_apply_13(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
        void*);
extern "C" void* rt_apply_14(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
        void*, void*);
extern "C" void* rt_apply_15(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
        void*, void*, void*);
extern "C" void* rt_apply_16(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
        void*, void*, void*, void*);
extern "C" void* rt_apply_17(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
        void*, void*, void*, void*, void*);
extern "C" void* rt_apply_18(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
        void*, void*, void*, void*, void*, void*);
extern "C" void* rt_apply_19(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
        void*, void*, void*, void*, void*, void*, void*);
extern "C" void* rt_apply_20(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
        void*, void*, void*, void*, void*, void*, void*, void*);

extern "C" void *rt_print(void* expr);

//}
/*EBoolean *rt_make_boolean(uint8_t booleanValue);
uint64_t rt_is_boolean(EObjectHeader *val);
EInteger *rt_make_integer(int64_t value);
uint64_t rt_is_integer(EObjectHeader *val);
EFloat *rt_make_float(double value);
ESymbol *rt_make_symbol(char *name);*/

#endif //ELECTRUM_RUNTIME_H
