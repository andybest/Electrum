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

    }

    /*void* Interpreter::evalExpr(shared_ptr<ASTNode> expr, shared_ptr<Environment> env) {
        auto theExpr = std::move(expr);

        evalBegin:

        switch (theExpr->tag) {
            case kTypeTagFloat:
                return rt_make_float(theExpr->floatValue);
            case kTypeTagInteger:
                return rt_make_integer(theExpr->integerValue);
            case kTypeTagBoolean:
                return rt_make_boolean(static_cast<int8_t>(theExpr->booleanValue));
            case kTypeTagNil:
                return NIL_PTR;
            case kTypeTagString:
                return rt_make_string(theExpr->stringValue->c_str());

            case kTypeTagSymbol:
                // Lookup in environment
                return lookupVariable(*theExpr->stringValue, env, theExpr);

            case kTypeTagList:auto list = theExpr->listValue;
                if (list->empty()) {
                    throw InterpreterException("Cannot evaluate empty list.", theExpr->sourcePosition);
                }

                auto firstElem = list->at(0);
                if (firstElem->tag == kTypeTagSymbol) {
                    auto symName = *firstElem->stringValue;

                    if (symName == "if") {
                        theExpr = evalIf(theExpr);
                        goto evalBegin; // TCO
                    } else if (symName == "define") {
                        return evalDefine(theExpr, env);
                    } else if (symName == "do") {
                        theExpr = evalDo(theExpr, env); // TCO
                        goto evalBegin;
                    }
                }
        }

        throw InterpreterException("Unrecognized form in eval.", theExpr->sourcePosition);
    }

    shared_ptr<ASTNode> Interpreter::evalIf(shared_ptr<ASTNode> expr) {
        if (expr->listValue->size() < 3) {
            // If must have at least a condition and subsequent
            throw InterpreterException("If form must have at least a condition and a subsequent.",
                                       expr->sourcePosition);
        }

        auto condition = expr->listValue->at(1);
        auto subsequent = expr->listValue->at(2);

        auto evaluatedCondition = evalExpr(condition);

        if (evaluatedCondition->tag != kTypeTagBoolean) {
            // If condition must be a boolean
            throw InterpreterException("If condition must be a boolean", expr->sourcePosition);
        }

        if (evaluatedCondition->booleanValue) {
            return subsequent;
        } else {
            if (expr->listValue->size() > 3) {
                return expr->listValue->at(3);
            }

            auto nil = make_shared<ASTNode>();
            nil->tag = kTypeTagNil;
            return nil;
        }
    }

    void Interpreter::storeInEnvironment(string name, shared_ptr<ASTNode> val, shared_ptr<Environment> env) {
        env->bindings.insert(make_pair(name, val));
    }

    shared_ptr<Environment> Interpreter::extendEnvironment(shared_ptr<Environment> env) {
        auto newEnv = make_shared<Environment>();
        newEnv->parent = std::move(env);
        return newEnv;
    }

    shared_ptr<ASTNode> Interpreter::evalDefine(shared_ptr<ASTNode> expr, shared_ptr<Environment> env) {
        if(expr->listValue->size() != 3) {
            throw InterpreterException("Define requires a symbol and a value.", expr->sourcePosition);
        }

        auto binding = expr->listValue->at(1);
        auto val = evalExpr(expr->listValue->at(2), env);

        if(binding->tag != kTypeTagSymbol) {
            throw InterpreterException("Define requires the binding to be a symbol.", binding->sourcePosition);
        }

        storeInEnvironment(*binding->stringValue, val, env);
        return val;
    }

    shared_ptr<ASTNode> Interpreter::evalDo(shared_ptr<ASTNode> expr, shared_ptr<Environment> env) {
        if(expr->listValue->size() < 2) {
            // Do bodies must have at least one statement
            throw InterpreterException("Do forms must have at least one body form.", expr->sourcePosition);
        }

        for(auto it = expr->listValue->begin() + 1; it < expr->listValue->end() - 1; ++it) {
            evalExpr(*it, env);
        }

        return *(expr->listValue->end() - 1);
    }

    shared_ptr <ASTNode>
    Interpreter::lookupVariable(const string name, shared_ptr <Environment> env, shared_ptr <ASTNode> expr) const {
        if (env == nullptr) {
            std::stringstream ss;
            ss << "Cannot find variable '" << name << "' in environment.";
            throw InterpreterException(ss.str(), expr->sourcePosition);
        }

        auto result = env->bindings.find(name);
        if (result != env->bindings.end()) {
            return result->second;
        }

        return lookupVariable(name, env->parent, expr);
    }*/

    void *Interpreter::evalExpr(void *expr) {
        return evalExpr(expr, rt_make_environment(NIL_PTR));
    }

    void *Interpreter::evalExpr(void *expr, void *env) {
        void *theExpr = expr;
        void *theEnv = env;

        evalStart:

        printf("Evaluating :");
        print_expr(theExpr);
        printf("\n");

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
                case kETypeTagSymbol:
                    return this->lookup_symbol(theExpr, theEnv);
                case kETypeTagPair: {
                    auto pair = static_cast<EPair *>(static_cast<void *>(header));
                    if (is_object_with_tag(pair->value, kETypeTagSymbol)) {
                        auto sym = static_cast<ESymbol *>(static_cast<void *>(TAG_TO_OBJECT(pair->value)));
                        auto name = std::string(sym->name);

                        if (name == "if") {
                            theExpr = eval_if(theExpr, theEnv);
                            goto evalStart; // TCO
                        } else if (name == "begin") {
                            theExpr = eval_begin(theExpr, theEnv);
                            goto evalStart; // TCO
                        } else if (name == "lambda") {
                            return eval_lambda(theExpr, theEnv);
                        } else {
                            // Lookup symbol
                            // Eval as function
                        }
                    } else {
                        // Try to apply

                        auto proc = evalExpr(rt_car(theExpr), theEnv);

                        if(!is_object_with_tag(proc, kETypeTagInterpretedFunction)) {
                            throw InterpreterException("Unable to apply form", nullptr);
                        }

                        auto funcVal = static_cast<EInterpretedFunction *>(static_cast<void *>(TAG_TO_OBJECT(proc)));
                        auto funcEnv = rt_make_environment(funcVal->env);

                        auto current_arg_pair = rt_cdr(theExpr);
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

                        theEnv = funcEnv;
                        theExpr = rt_car(current_body_form);
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

    void *Interpreter::eval_interpreted_function(void *expr) {

    }

    void *Interpreter::lookup_symbol(void *symbol, void *env) {
        return rt_environment_get(env, symbol);
    }

    Interpreter::~Interpreter() {

    }

}
