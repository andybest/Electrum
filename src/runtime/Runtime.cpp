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
#include <sstream>
#include <cassert>

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

    std::string kind_for_obj(void *obj) {
        if(is_integer(obj)) {
            return "INTEGER";
        } else if(obj == NIL_PTR) {
            return "NIL";
        } else if(is_boolean(obj)) {
            return "BOOLEAN";
        } else if(!is_object(obj)) {
            return "";
        }

        auto header = TAG_TO_OBJECT(obj);

        switch(header->tag) {
            case kETypeTagInterpretedFunction: return "INT_FUNC";
            case kETypeTagFunction: return "CLOSURE";
            case kETypeTagVar: return "VAR";
            case kETypeTagPair: return "PAIR";
            case kETypeTagSymbol: return "SYMBOL";
            case kETypeTagKeyword: return "KEYWORD";
            case kETypeTagFloat: return "FLOAT";
            case kETypeTagEnvironment: return "ENVIRONMENT";
            case kETypeTagString: return "STRING";
        }

        return "";
    }

    std::string description_for_obj(void *obj) {
        std::stringstream ss;

        if (is_integer(obj)) {
            ss << TAG_TO_INTEGER(obj);
        } else if (is_object(obj)) {
            auto header = TAG_TO_OBJECT(obj);

            switch (header->tag) {
                case kETypeTagSymbol: {
                    auto val = static_cast<ESymbol *>(static_cast<void *>(header));
                    ss << val->name;
                    break;
                }
                case kETypeTagPair:
                    ss << "()";
                    break;
                case kETypeTagFloat: {
                    auto val = static_cast<EFloat *>(static_cast<void *>(header));
                    ss << val->floatValue << "f";
                    break;
                }
                case kETypeTagInterpretedFunction:
                    ss << "<Interpreted Function>";
                    break;
                case kETypeTagString: {
                    auto val = static_cast<EString *>(static_cast<void *>(header));
                    ss << "\"" << val->stringValue << "\"";
                    break;
                }
                case kETypeTagEnvironment:
                    ss << "<Environment>";
                    break;
                case kETypeTagFunction:
                    ss << "<Closure>";
                    break;
                case kETypeTagVar: {
                    auto val = static_cast<EVar *>(static_cast<void *>(header));
                    ss << "<Var " << description_for_obj(val->sym) << " : " << description_for_obj(val->val) << ">";
                    break;
                }
                case kETypeTagKeyword: {
                    auto val = static_cast<EString *>(static_cast<void *>(header));
                    ss << ":" << val->stringValue;
                    break;
                }

                default:
                    break;
            }
        } else if (obj == NIL_PTR) {
            ss << "NIL";
        } else if (is_boolean(obj)) {
            ss << ((obj == TRUE_PTR) ? "TRUE" : "FALSE");
        }

        return ss.str();
    }

    void print_expr(void *expr) {
        if (is_integer(expr)) {
            printf("Int:\t%li", TAG_TO_INTEGER(expr));
        } else if (is_object(expr)) {
            auto header = TAG_TO_OBJECT(expr);

            switch (header->tag) {
                case kETypeTagSymbol: {
                    auto val = static_cast<ESymbol *>(static_cast<void *>(header));
                    printf("Symbol:\t%s", val->name);
                    break;
                }
                case kETypeTagPair:
                    print_pair(expr);
                    break;
                case kETypeTagFloat: {
                    auto val = static_cast<EFloat *>(static_cast<void *>(header));
                    printf("Float:\t%f", val->floatValue);
                    break;
                }
                case kETypeTagInterpretedFunction:
                    printf("<Interpreted Function>");
                    break;
                case kETypeTagString: {
                    auto val = static_cast<EString *>(static_cast<void *>(header));
                    printf("String:\t%s", val->stringValue);
                    break;
                }
                case kETypeTagEnvironment:
                    printf("Function Environment");
                    break;
                case kETypeTagFunction:
                    printf("Closure");
                    break;
                case kETypeTagVar: {
                    auto val = static_cast<EVar *>(static_cast<void *>(header));
                    printf("Var");
                    printf("\t");
                    print_expr(val->sym);
                    printf("\t");
                    print_expr(val->val);
                    break;
                }
                case kETypeTagKeyword: {
                    auto val = static_cast<EString *>(static_cast<void *>(header));
                    printf("Keyword:\t%s", val->stringValue);
                    break;
                }


                default:
                    break;
            }
        } else if (expr == NIL_PTR) {
            printf("NIL PTR");
        } else if (is_boolean(expr)) {
            printf((expr == TRUE_PTR) ? "Boolean: true" : "Boolean: false");
        }

        printf("\n");
    }
}

//extern "C" {

void rt_init() {
    electrum::init_global_namespaces();
}

void rt_init_gc(electrum::GCMode gc_mode) {
    electrum::main_collector = new electrum::GarbageCollector(gc_mode);
}

void rt_deinit_gc() {
    delete(electrum::main_collector);
    electrum::main_collector = nullptr;
}

electrum::GarbageCollector *rt_get_gc() {
    return electrum::main_collector;
}

void *rt_is_object(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_object(val));
}

extern "C" void *rt_make_nil() {
    return NIL_PTR;
}

extern "C" void *rt_make_boolean(int8_t booleanValue) {
    return (booleanValue) ? TRUE_PTR : FALSE_PTR;
}

extern "C" void *rt_is_boolean(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_boolean(val));
}

extern "C" uint8_t rt_is_true(void *val) {
    if (!rt_is_boolean(val)) {
        // ERROR!
    }

    return static_cast<uint8_t>(val == TRUE_PTR);
}

extern "C" void *rt_make_integer(int64_t value) {
    return INTEGER_TO_TAG(value);
}

extern "C" void *rt_is_integer(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_integer(val));
}

extern "C" int64_t rt_integer_value(void *val) {
    return TAG_TO_INTEGER(val);
}

extern "C" void *rt_make_float(double value) {
    auto floatVal = static_cast<EFloat *>(GC_MALLOC(sizeof(EFloat)));
    floatVal->header.tag = kETypeTagFloat;
    floatVal->header.gc_mark = 0;
    floatVal->floatValue = value;
    return OBJECT_TO_TAG(floatVal);
}

extern "C" void *rt_is_float(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_object_with_tag(val, kETypeTagFloat));
}

extern "C" double rt_float_value(void *val) {
    auto header = TAG_TO_OBJECT(val);
    auto f = static_cast<EFloat *>(static_cast<void *>(header));
    return f->floatValue;
}

extern "C" void *rt_make_symbol(const char *name) {
    size_t len = strlen(name);

    // Allocate enough space for the string
    auto *symbolVal = static_cast<ESymbol *>(GC_MALLOC(sizeof(ESymbol) + (sizeof(char) * len) + 1));
    symbolVal->header.tag = kETypeTagSymbol;
    symbolVal->header.gc_mark = 0;
    strcpy(symbolVal->name, name);
    symbolVal->length = (uint64_t) len;

    // Add null termination
    symbolVal->name[len] = 0;
    return OBJECT_TO_TAG(symbolVal);
}

extern "C" void *rt_is_symbol(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_object_with_tag(val, kETypeTagSymbol));
}

extern "C" void *rt_make_string(const char *str) {
    size_t len = strlen(str);

    // Allocate enough space for the string
    auto *strVal = static_cast<EString *>(GC_MALLOC(sizeof(EString) + (sizeof(char) * len) + 1));
    strVal->header.tag = kETypeTagString;
    strVal->header.gc_mark = 0;
    strcpy(strVal->stringValue, str);
    strVal->length = (uint64_t) len;

    // Add null termination
    strVal->stringValue[len] = 0;
    return OBJECT_TO_TAG(strVal);
}

extern "C" void *rt_is_string(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_object_with_tag(val, kETypeTagString));
}

extern "C" const char *rt_string_value(void *val) {
    auto str = reinterpret_cast<EString *>(TAG_TO_OBJECT(val));
    return str->stringValue;
}

void *rt_make_keyword(const char *str) {
    size_t len = strlen(str);

    // Allocate enough space for the string
    auto *keywordVal = static_cast<EKeyword *>(GC_MALLOC(sizeof(EKeyword) + (sizeof(char) * len) + 1));
    keywordVal->header.tag = kETypeTagKeyword;
    keywordVal->header.gc_mark = 0;
    strcpy(keywordVal->name, str);
    keywordVal->length = (uint64_t) len;

    // Add null termination
    keywordVal->name[len] = 0;
    return OBJECT_TO_TAG(keywordVal);
}

void *rt_is_keyword(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_object_with_tag(val, kETypeTagKeyword));
}

extern "C" void *rt_make_var(void *sym) {

    auto var = static_cast<EVar *>(GC_MALLOC(sizeof(EVar)));
    var->header.gc_mark = 0;
    var->header.tag = kETypeTagVar;
    var->sym = sym;
    var->val = NIL_PTR;

    return OBJECT_TO_TAG(var);
}

extern "C" void *rt_is_var(void *v) {
    return TO_TAGGED_BOOLEAN(electrum::is_object_with_tag(v, kETypeTagVar));
}

extern "C" void rt_set_var(void *v, void *val) {
    auto var = reinterpret_cast<EVar *>(TAG_TO_OBJECT(v));
    var->val = val;
}

extern "C" void *rt_deref_var(void *v) {
    auto var = reinterpret_cast<EVar *>(TAG_TO_OBJECT(v));
    return var->val;
}

void *rt_is_pair(void *val) {
    return TO_TAGGED_BOOLEAN(electrum::is_object_with_tag(val, kETypeTagPair));
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

extern "C" void *rt_make_compiled_function(uint64_t arity, void *fp, uint64_t env_size) {
    auto funcVal = static_cast<ECompiledFunction *>(GC_MALLOC(sizeof(ECompiledFunction) + (sizeof(void *) * env_size)));
    funcVal->header.tag = kETypeTagFunction;
    funcVal->header.gc_mark = 0;
    funcVal->arity = arity;
    funcVal->f_ptr = fp;
    funcVal->env_size = env_size;
    return OBJECT_TO_TAG(funcVal);
}

extern "C" uint64_t rt_compiled_function_get_arity(void *func) {
    auto header = TAG_TO_OBJECT(func);

    if (header->tag != kETypeTagFunction) {
        // TODO: Exception
    }

    auto f = static_cast<ECompiledFunction *>(static_cast<void *>(header));
    return f->arity;
}

extern "C" void *rt_compiled_function_get_ptr(void *func) {
    auto header = TAG_TO_OBJECT(func);

    if (header->tag != kETypeTagFunction) {
        // TODO: Exception
    }

    auto f = static_cast<ECompiledFunction *>(static_cast<void *>(header));
    return f->f_ptr;
}

extern "C" void *rt_compiled_function_set_env(void *func, uint64_t index, void *value) {
    auto funcVal = static_cast<ECompiledFunction *>(static_cast<void *>(TAG_TO_OBJECT(func)));
    funcVal->env[index] = value;
    return funcVal;
}

extern "C" void *rt_compiled_function_get_env(void *func, uint64_t index) {
    auto funcVal = static_cast<ECompiledFunction *>(static_cast<void *>(TAG_TO_OBJECT(func)));
    return funcVal->env[index];
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