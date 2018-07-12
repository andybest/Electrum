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
#include "Interpreter.h"
#include "InterpreterExceptions.h"

namespace electrum {

    Interpreter::Interpreter(): rootEnvironment_(make_shared<Environment>()) {

    }

    shared_ptr<ASTNode> Interpreter::evalExpr(shared_ptr<ASTNode> expr) {
        return evalExpr(std::move(expr), rootEnvironment_);
    }

    shared_ptr<ASTNode> Interpreter::evalExpr(shared_ptr<ASTNode> expr, shared_ptr<Environment> env) {
        auto theExpr = std::move(expr);

        evalBegin:

        switch (theExpr->tag) {
            case kTypeTagFloat:
            case kTypeTagInteger:
            case kTypeTagBoolean:
            case kTypeTagNil:
            case kTypeTagString:
                // Return these types as-is
                return theExpr;

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
    }




}
