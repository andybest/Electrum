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
        current_state()->value_stack.push_back(val);
    }

    llvm::Value *CompilerContext::pop_value() {
        auto v = current_state()->value_stack.back();
        current_state()->value_stack.pop_back();
        return v;
    }

#pragma mark Current Function


    void CompilerContext::push_func(llvm::Function *func) {
        current_state()->func_stack.push_back(func);
    }

    llvm::Function *CompilerContext::pop_func() {
        auto f = current_state()->func_stack.back();
        current_state()->func_stack.pop_back();
        return f;
    }

    llvm::Function *CompilerContext::current_func() {
        // If there are no functions on the stack, return the current top level initializer
        if(current_state()->func_stack.empty()) {
            return nullptr;
        }
        return current_state()->func_stack.back();
    }

#pragma mark Local Environment

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

    llvm::Module *CompilerContext::current_module() {
        return current_state()->module.get();
    }

    llvm::LLVMContext &CompilerContext::llvm_context() {
        return _context;
    }

    std::shared_ptr<llvm::IRBuilder<>> CompilerContext::current_builder() {
        return current_state()->builder;
    }

    void CompilerContext::push_new_state(std::string module_name) {
        auto s = std::make_shared<ContextState>();

        s->module = std::make_unique<llvm::Module>(module_name, _context);
        s->builder = std::make_shared<llvm::IRBuilder<>>(_context);
        _state_stack.push_back(s);
    }

    std::shared_ptr<ContextState> CompilerContext::current_state() {
        assert(!_state_stack.empty());
        return _state_stack.back();
    }

    std::unique_ptr<llvm::Module> CompilerContext::pop_state() {
        auto state = _state_stack.back();
        _state_stack.pop_back();

        return std::move(state->module);
    }
}