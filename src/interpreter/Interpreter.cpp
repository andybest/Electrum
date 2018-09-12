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

#include <string>
#include <sstream>
#include <utility>
#include <runtime/Runtime.h>
#include "Interpreter.h"
#include "InterpreterExceptions.h"

namespace electrum {

    Interpreter::Interpreter() {
        // TODO: Exception when GC hasn't been initialized
        // TODO: Add root environment as GC root
        // TODO: Runtime function for getting/making root environment
        rootEnvironment_ = rt_make_environment(NIL_PTR);
    }

    void *Interpreter::evalExpr(void *expr) {
        return evalExpr(expr, rootEnvironment_);
    }

    void *Interpreter::evalExpr(void *expr, void *env) {
        void *theExpr = expr;
        void *theEnv = env;

        evalStart:

        // Return self-evaluating forms
        if (is_integer(theExpr)) {
            return theExpr;
        } else if (is_boolean(theExpr)) {
            return theExpr;
        } else if (expr == NIL_PTR) {
            return theExpr;
        } else if (is_object(theExpr)) {
            auto header = TAG_TO_OBJECT(theExpr);

            switch (header->tag) {
                case kETypeTagFloat:
                    return theExpr;
                case kETypeTagString:
                    return theExpr;
                case kETypeTagKeyword:
                    return theExpr;
                case kETypeTagSymbol:
                    return this->lookup_symbol(theExpr, theEnv);
                case kETypeTagPair: {
                    auto pair = static_cast<EPair *>(static_cast<void *>(header));
                    if (is_object_with_tag(pair->value, kETypeTagSymbol)) {
                        auto sym = static_cast<ESymbol *>(static_cast<void *>(TAG_TO_OBJECT(pair->value)));
                        auto name = std::string(sym->name);

                        // Eval special forms
                        if (name == "if") {
                            theExpr = eval_if(theExpr, theEnv);
                            goto evalStart; // TCO
                        } else if (name == "begin") {
                            theExpr = eval_begin(theExpr, theEnv);
                            goto evalStart; // TCO
                        } else if (name == "lambda") {
                            return eval_lambda(theExpr, theEnv);
                        } else if (name == "define") {
                            eval_define(theExpr, theEnv);
                            return NIL_PTR;
                        } else {
                            // Try to apply it
                            auto proc = evalExpr(rt_car(theExpr), theEnv);
                            auto result = eval_apply(theExpr, proc, theEnv);
                            theEnv = result.first;
                            theExpr = result.second;

                            goto evalStart; // TCO
                        }
                    } else {
                        // Try to apply

                        auto proc = evalExpr(rt_car(theExpr), theEnv);
                        auto result = eval_apply(theExpr, proc, theEnv);
                        theEnv = result.first;
                        theExpr = result.second;

                        goto evalStart; // TCO
                    }
                }
                case kETypeTagInterpretedFunction:
                    return theExpr;
            }

            throw InterpreterException("Unrecognised expression", nullptr);

        } else {
            throw InterpreterException("Unrecognised pointer", nullptr);
        }
    }

    void *Interpreter::eval_if(void *expr, void *env) {
        auto predExpr = rt_cdr(expr);
        if (predExpr == NIL_PTR) {
            throw InterpreterException("If expects a predicate", nullptr);
        }

        auto consequentExpr = rt_cdr(predExpr);
        if (consequentExpr == NIL_PTR) {
            throw InterpreterException("If expects a consequent", nullptr);
        }

        auto alternativeExpr = rt_cdr(consequentExpr);

        if (alternativeExpr != NIL_PTR) {
            auto next = rt_cdr(alternativeExpr);
            if (next != NIL_PTR) {
                throw InterpreterException("Too many forms in if body", nullptr);
            }
        }

        auto pred = evalExpr(rt_car(predExpr), env);
        if (!is_boolean(pred)) {
            throw InterpreterException("If predicate must be a boolean.", nullptr);
        }

        if (pred == TRUE_PTR) {
            return rt_car(consequentExpr);
        }

        return rt_car(alternativeExpr);
    }

    void *Interpreter::eval_begin(void *expr, void *env) {
        auto theExpr = rt_cdr(expr);

        if (theExpr == NIL_PTR) {
            throw InterpreterException("Begin must have at least one form in the body", nullptr);
        }

        auto nextExpr = rt_cdr(theExpr);

        while (nextExpr != NIL_PTR) {
            evalExpr(rt_car(theExpr), env);

            theExpr = nextExpr;
            nextExpr = rt_cdr(theExpr);
        }

        return rt_car(theExpr);
    }

    void *Interpreter::eval_lambda(void *expr, void *env) {
        auto argList = rt_cdr(expr);

        if (argList == NIL_PTR) {
            throw InterpreterException("Lambda requires an argument list", nullptr);
        }

        if (!is_object_with_tag(argList, kETypeTagPair)) {
            throw InterpreterException("Lambda requires an argument list", nullptr);
        }

        argList = rt_car(argList);

        void *argHead = nullptr;
        void *currentArg = nullptr;
        uint64_t arity = 0;


        while (argList != NIL_PTR) {
            auto arg = rt_car(argList);
            // If the arg name is a function, evaluate it
            if (is_object_with_tag(arg, kETypeTagPair)) {
                arg = evalExpr(arg, env);
            }

            if (!is_object_with_tag(arg, kETypeTagSymbol)) {
                throw InterpreterException("Lambda arguments must be symbols", nullptr);
            }

            if (argHead == nullptr) {
                argHead = rt_make_pair(arg, NIL_PTR);
                currentArg = argHead;
            } else {
                auto thisArg = rt_make_pair(arg, NIL_PTR);
                rt_set_cdr(currentArg, thisArg);
                currentArg = thisArg;
            }

            ++arity;

            argList = rt_cdr(argList);
        }

        auto body = rt_cdr(rt_cdr(expr));
        if (body == NIL_PTR) {
            throw InterpreterException("Lambda requires a body", nullptr);
        }

        return rt_make_interpreted_function(argHead, arity, body, env);
    }

    void Interpreter::eval_define(void *expr, void *env) {
        auto binding = rt_car(rt_cdr(expr));

        if(binding == NIL_PTR) {
            throw InterpreterException("define requires a symbol to bind to!", nullptr);
        }

        if(!is_object_with_tag(binding, kETypeTagSymbol)) {
            throw InterpreterException("define requires a symbol to bind to!", nullptr);
        }

        auto value = evalExpr(rt_car(rt_cdr(rt_cdr(expr))), env);

        rt_environment_add(rootEnvironment_, binding, value);
    }

    std::pair<void*, void*> Interpreter::eval_apply(void *expr, void *proc, void *env) {
        if(!is_object_with_tag(proc, kETypeTagInterpretedFunction)) {
            throw InterpreterException("Unable to apply form", nullptr);
        }

        auto funcVal = static_cast<EInterpretedFunction *>(static_cast<void *>(TAG_TO_OBJECT(proc)));
        auto funcEnv = rt_make_environment(funcVal->env);

        auto current_arg_pair = rt_cdr(expr);
        auto current_binding = funcVal->argnames;

        while (current_binding != NIL_PTR) {
            if (current_arg_pair == NIL_PTR) {
                throw InterpreterException("Argument count mismatch", nullptr);
            }

            rt_environment_add(env, rt_car(current_binding), rt_car(current_arg_pair));

            current_arg_pair = rt_cdr(current_arg_pair);
            current_binding = rt_cdr(current_binding);
        }

        if (current_arg_pair != NIL_PTR) {
            throw InterpreterException("Argument count mismatch", nullptr);
        }

        auto current_body_form = funcVal->body;
        void *next_body_form = rt_cdr(current_body_form);

        while (next_body_form != NIL_PTR) {
            evalExpr(rt_car(current_body_form), funcEnv);
            current_body_form = next_body_form;
            next_body_form = rt_cdr(current_body_form);
        }

        return std::pair<void *, void *>(funcEnv, rt_car(current_body_form));
    }

    void *Interpreter::lookup_symbol(void *symbol, void *env) {
        return rt_environment_get(env, symbol);
    }

    Interpreter::~Interpreter() {

    }

}
