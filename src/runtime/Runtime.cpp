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
#include "GarbageCollector.h"
#include <cstdlib>
#include <cstring>

namespace electrum {

    bool is_object(void *val) {
        return (reinterpret_cast<uintptr_t>(val) & TAG_MASK) == OBJECT_TAG;
    }

    bool is_integer(void *val) {
        auto i = (reinterpret_cast<uintptr_t>(val) & 0x1) == INTEGER_TAG;
        return i;
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

    bool symbol_equal(void *s1, void *s2) {
        assert(is_object_with_tag(s1, kETypeTagSymbol));
        assert(is_object_with_tag(s2, kETypeTagSymbol));

        auto s1Val = static_cast<ESymbol *>(static_cast<void *>(TAG_TO_OBJECT(s1)));
        auto s2Val = static_cast<ESymbol *>(static_cast<void *>(TAG_TO_OBJECT(s2)));

        return (std::string(s1Val->name) == std::string(s2Val->name));
    }

    void print_pair(void *expr) {
        printf("(");

        bool first = true;
        auto currentPair = expr;
        while (currentPair != NIL_PTR) {
            if (!first) printf(" ");
            print_expr(rt_car(currentPair));
            currentPair = rt_cdr(currentPair);
            first = false;
        }

        printf(")");
    }

    void print_expr(void *expr) {
        if (is_integer(expr)) {
            printf("%li", TAG_TO_INTEGER(expr));
        } else if (is_object(expr)) {
            auto header = TAG_TO_OBJECT(expr);

            switch (header->tag) {
                case kETypeTagSymbol: {
                    auto val = static_cast<ESymbol *>(static_cast<void *>(header));
                    printf("%s", val->name);
                    break;
                }
                case kETypeTagPair:
                    print_pair(expr);
                    break;
                case kETypeTagFloat: {
                    auto val = static_cast<EFloat *>(static_cast<void *>(header));
                    printf("%f", val->floatValue);
                    break;
                }
                case kETypeTagInterpretedFunction:
                    printf("<Interpreted Function>");
                    break;
                case kETypeTagString: {
                    auto val = static_cast<EString *>(static_cast<void *>(header));
                    printf("%s", val->stringValue);
                    break;
                }

                default:
                    break;
            }
        } else if(expr == NIL_PTR) {
            printf("nil");
        } else if(is_boolean(expr)) {
            printf((expr == TRUE_PTR) ? "true" : "false");
        }
    }
}

//extern "C" {

void rt_init() {
    electrum::init_global_namespaces();
}

void rt_init_gc(electrum::GCMode gc_mode) {
    electrum::main_collector = std::make_shared<electrum::GarbageCollector>(gc_mode);
}

void rt_deinit_gc() {
    electrum::main_collector = nullptr;
}

std::shared_ptr<electrum::GarbageCollector> rt_get_gc() {
    return electrum::main_collector;
}

void *rt_is_object(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_object(val));
}

void *rt_make_boolean(int8_t booleanValue) {
    return (booleanValue) ? TRUE_PTR : FALSE_PTR;
}

void *rt_is_boolean(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_boolean(val));
}

void *rt_make_integer(int64_t value) {
    return INTEGER_TO_TAG(value);
}

void *rt_is_integer(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_integer(val));
}

void *rt_make_float(double value) {
    auto floatVal = static_cast<EFloat *>(GC_MALLOC(sizeof(EFloat)));
    floatVal->header.tag = kETypeTagFloat;
    floatVal->header.gc_mark = 0;
    floatVal->floatValue = value;
    return OBJECT_TO_TAG(floatVal);
}

void *rt_is_float(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_object_with_tag(val, kETypeTagFloat));
}

double rt_float_value(void *val) {
    auto header = TAG_TO_OBJECT(val);
    auto f = static_cast<EFloat *>(static_cast<void *>(header));
    return f->floatValue;
}

void *rt_make_symbol(const char *name) {
    auto *symbolVal = static_cast<ESymbol *>(GC_MALLOC(sizeof(ESymbol)));
    symbolVal->header.tag = kETypeTagSymbol;
    symbolVal->header.gc_mark = 0;
#warning Temporary malloc
    symbolVal->name = static_cast<char *>(malloc(strlen(name)));
    strcpy(symbolVal->name, name);
    return OBJECT_TO_TAG(symbolVal);
}

void *rt_is_symbol(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_object_with_tag(val, kETypeTagSymbol));
}

void *rt_make_pair(void *value, void *next) {
    auto *pairVal = static_cast<EPair *>(GC_MALLOC(sizeof(EPair)));
    pairVal->header.gc_mark = 0;
    pairVal->header.tag = kETypeTagPair;
    pairVal->value = value;
    pairVal->next = next;
    return OBJECT_TO_TAG(pairVal);
}

void *rt_car(void *pair) {
    auto header = TAG_TO_OBJECT(pair);
    assert(header->tag == kETypeTagPair);
    auto pairVal = static_cast<EPair *>(static_cast<void *>(header));
    return pairVal->value;
}

void *rt_cdr(void *pair) {
    auto header = TAG_TO_OBJECT(pair);
    assert(header->tag == kETypeTagPair);
    auto pairVal = static_cast<EPair *>(static_cast<void *>(header));
    return pairVal->next;
}

void *rt_set_car(void *pair, void *val) {
    auto header = TAG_TO_OBJECT(pair);
    assert(header->tag == kETypeTagPair);
    auto pairVal = static_cast<EPair *>(static_cast<void *>(header));
    pairVal->value = val;
    return pair;
}

void *rt_set_cdr(void *pair, void *next) {
    auto header = TAG_TO_OBJECT(pair);
    assert(header->tag == kETypeTagPair);
    auto pairVal = static_cast<EPair *>(static_cast<void *>(header));
    pairVal->next = next;
    return pair;
}

void *rt_make_interpreted_function(void *argnames, uint64_t arity, void *body, void *env) {
    auto funcVal = static_cast<EInterpretedFunction *>(GC_MALLOC(sizeof(EInterpretedFunction)));
    funcVal->header.tag = kETypeTagInterpretedFunction;
    funcVal->header.gc_mark = 0;
    funcVal->argnames = argnames;
    funcVal->arity = arity;
    funcVal->body = body;
    funcVal->env = env;
    return OBJECT_TO_TAG(funcVal);
}

void *rt_make_environment(void *parent) {
    auto envVal = static_cast<EEnvironment *>(GC_MALLOC(sizeof(EEnvironment)));
    envVal->header.tag = kETypeTagEnvironment;
    envVal->header.gc_mark = 0;
    envVal->parent = parent;
    envVal->values = NIL_PTR;
    return OBJECT_TO_TAG(envVal);
}

void *rt_environment_add(void *env, void *binding, void *value) {
    auto envVal = static_cast<EEnvironment *>(static_cast<void *>(TAG_TO_OBJECT(env)));
    auto currentValues = envVal->values;
    envVal->values = rt_make_pair(binding, rt_make_pair(value, currentValues));
    return env;
}

void *rt_environment_get(void *env, void *binding) {
    auto currentEnv = env;

    while (currentEnv != NIL_PTR) {
        auto envVal = static_cast<EEnvironment *>(static_cast<void *>(TAG_TO_OBJECT(currentEnv)));
        auto currentValue = envVal->values;

        while (currentValue != NIL_PTR) {
            auto b = rt_car(currentValue);
            auto v_pair = rt_cdr(currentValue);
            auto v = rt_car(v_pair);

            if (electrum::symbol_equal(binding, b)) {
                return v;
            }

            currentValue = rt_cdr(v_pair);
        }

        currentEnv = envVal->parent;
    }

    // Cannot find symbol in environment!
    // TODO: This should actually throw a catchable error
    return NIL_PTR;
}

/**
 * Malloc a tagged object. It is assumed by the GC that this object will be
 * converted to a tagged pointer.
 * @return A pointer to the allocated memory
 */
void *rt_gc_malloc_tagged_object(size_t size) {
    return electrum::main_collector->malloc_tagged_object(size);
}

//} /* extern "C" */