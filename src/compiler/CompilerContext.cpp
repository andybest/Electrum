/*
 MIT License

 Copyright (c) 2019 Andy Best

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

#include "CompilerContext.h"

namespace electrum {
    #pragma mark Value Stack

    void CompilerContext::push_value(llvm::Value *val) {
        value_stack.push_back(val);
    }

    llvm::Value *CompilerContext::pop_value() {
        auto v = value_stack.back();
        value_stack.pop_back();
        return v;
    }

#pragma mark Current Function

    void CompilerContext::push_new_function(llvm::FunctionType *func_type, std::string name) {
        auto f = llvm::dyn_cast<llvm::Function>(current_module()->getOrInsertFunction(name, func_type));
        push_func(f);
    }

    void CompilerContext::push_func(llvm::Function *func) {
        func_stack.push_back(func);
    }

    llvm::Function *CompilerContext::pop_func() {
        auto f = func_stack.back();
        func_stack.pop_back();
        return f;
    }

    llvm::Function *CompilerContext::current_func() {
        // If there are no functions on the stack, return the current top level initializer
        if(func_stack.empty()) {
            return top_level_initializers.back().func;
        }
        return func_stack.back();
    }

    bool CompilerContext::is_top_level() {
        return func_stack.size() == 0;
    }

#pragma mark Local Environment

    void CompilerContext::push_local_environment() {
        local_bindings.emplace_back();
    }

    void CompilerContext::push_local_environment(const std::unordered_map<std::string, llvm::Value *> &new_env) {
        local_bindings.push_back(new_env);
    }

    void CompilerContext::pop_local_environment() {
        local_bindings.pop_back();
    }

    llvm::Value *CompilerContext::lookup_in_local_environment(const std::string name) {
        for (auto it = local_bindings.rbegin(); it != local_bindings.rend(); ++it) {
            auto env = *it;
            auto result = env.find(name);

            if (result != env.end()) {
                return result->second;
            }
        }

        return nullptr;
    }

#pragma mark Evaluation Context

    EvaluationContext CompilerContext::current_evaluation_context() {
        if(evaluation_context_stack.empty()) {
            return kEvaluationContextRuntime;
        } else {
            return evaluation_context_stack.back();
        }
    }

    void CompilerContext::push_evaluation_context(EvaluationContext ctx) {
        evaluation_context_stack.push_back(ctx);
    }

    void CompilerContext::pop_evaluation_context() {
        if(!evaluation_context_stack.empty()) {
            evaluation_context_stack.pop_back();
        }
    }

    void CompilerContext::create_module(std::string name) {
        assert(_module == nullptr);
        _module = std::make_unique<llvm::Module>(name, _context);
    }

    llvm::Module *CompilerContext::current_module() {
        return _module.get();
    }

    std::unique_ptr<llvm::Module> CompilerContext::move_module() {
        return std::move(_module);
    }

    llvm::LLVMContext &CompilerContext::llvm_context() {
        return _context;
    }
}