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


#ifndef ELECTRUM_COMPILER_H
#define ELECTRUM_COMPILER_H

#include <lex.yy.h>
#include <cstdint>
#include <memory>
#include "Analyzer.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>

namespace electrum {

    struct CompilerContext {
        std::vector<llvm::Value *> value_stack;

        void push_value(llvm::Value *val) {
            value_stack.push_back(val);
        }

        llvm::Value *pop_value() {
            auto v = value_stack.back();
            value_stack.pop_back();
            return v;
        }

    };

    class Compiler {

    public:
        Compiler();

    private:

        llvm::LLVMContext _context;
        std::unique_ptr<llvm::Module> _module;
        std::unique_ptr<llvm::IRBuilder<>> _builder;
        CompilerContext _compilerContext;

        /// Address space for the garbage collector
        static const int kGCAddressSpace = 1;

        CompilerContext *current_context() { return &_compilerContext; }

        void compile_node(std::shared_ptr<AnalyzerNode> node);

        void compile_constant(std::shared_ptr<ConstantValueAnalyzerNode> node);

        void compile_lambda(std::shared_ptr<LambdaAnalyzerNode> node);

        void compile_do(std::shared_ptr<DoAnalyzerNode> node);

        void compile_if(std::shared_ptr<IfAnalyzerNode> node);

        llvm::Value *make_integer(int64_t value);
        llvm::Value *make_float(double value);
        llvm::Value *make_boolean(bool value);
        llvm::Value *make_symbol(std::shared_ptr<std::string> name);
        llvm::Value *make_closure(uint64_t arity, llvm::Value *environment, llvm::Value *func_ptr);
    };
}


#endif //ELECTRUM_COMPILER_H
