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
#include "Interpreter.h"

namespace electrum {
    shared_ptr<ASTNode> Interpreter::evalExpr(shared_ptr <ASTNode> expr) {
        auto theExpr = expr;

        evalBegin:

        switch(theExpr->tag) {
            case kTypeTagFloat:
            case kTypeTagInteger:
            case kTypeTagBoolean:
            case kTypeTagString:
                // Return these types as-is
                return theExpr;

            case kTypeTagSymbol:
                // Lookup in environment
                return lookupVariable();

            case kTypeTagList:
                auto list = theExpr->listValue;
                if(list->empty()) {
                    throw std::exception(); // TODO: Replace with proper exception
                }

                auto firstElem = list->at(0);
                if(firstElem->tag == kTypeTagSymbol) {
                    auto symName = *firstElem->stringValue;

                    if(symName == "if") {
                        theExpr = evalIf(theExpr);
                        goto evalBegin; // TCO
                    }
                }

        }
    }

    shared_ptr<ASTNode> Interpreter::evalIf(shared_ptr<ASTNode> expr) {
        if(expr->listValue->size() < 3) {
            // If must have at least a condition and subsequent
            throw std::exception(); // TODO: replace with proper exception
        }

        auto condition = expr->listValue->at(1);
        auto subsequent = expr->listValue->at(2);

        auto evaluatedCondition = evalExpr(condition);

        if(evaluatedCondition->tag != kTypeTagBoolean) {
            // If condition must be a boolean
            throw std::exception(); // TODO: replace with proper exception
        }

        if(evaluatedCondition->booleanValue) {
            return subsequent;
        } else {
            if(expr->listValue->size() > 3) {
                return expr->listValue->at(3);
            }

            auto nil = make_shared<ASTNode>();
            nil->tag = kTypeTagNil;
            return nil;
        }
    }

    shared_ptr <ASTNode> Interpreter::lookupVariable() {
        return make_shared<ASTNode>();
    }
}
