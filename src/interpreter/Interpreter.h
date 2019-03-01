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

#ifndef ELECTRUM_INTERPRETER_H
#define ELECTRUM_INTERPRETER_H

#include <memory>
#include <utility>
#include "types/Types.h"

namespace electrum {

using std::shared_ptr;
using std::make_shared;

class Interpreter {
public:
    Interpreter();
    ~Interpreter();

    void* evalExpr(void* expr);
    void* evalExpr(void* expr, void* env);

private:
    void* rootEnvironment_;
    void* lookup_symbol(void* symbol, void* env);
    void* eval_if(void* expr, void* env);
    void* eval_begin(void* symbol, void* env);
    void* eval_lambda(void* expr, void* env);
    void* eval_interpreted_function(void* expr);
    void eval_define(void* expr, void* env);
    std::pair<void*, void*> eval_apply(void* expr, void* proc, void* env);
};
}

#endif //ELECTRUM_INTERPRETER_H
