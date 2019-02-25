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

#ifndef ELECTRUM_COMPILERCONTEXT_H
#define ELECTRUM_COMPILERCONTEXT_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include "EvaluationPhase.h"

namespace electrum {

    struct GlobalDef {
        std::string name;
        std::string mangled_name;
    };

    struct TopLevelInitializerDef {
        /// The phases in which this initializer will be evaluated
        EvaluationPhase evaluation_phases = kEvaluationPhaseNone;

        /// Which phases the initializer has been evaluated in already
        EvaluationPhase evaluated_in = kEvaluationPhaseNone;

        /// The mangled name of the initializer function
        std::string mangled_name;
    };

    struct ContextState {
        std::shared_ptr<llvm::IRBuilder<>> builder;
        std::unique_ptr<llvm::Module> module;
        std::vector<llvm::Value *> value_stack;
        std::vector<llvm::Function *> func_stack;
    };

    class CompilerContext {
    private:
        llvm::LLVMContext _context;

        std::vector<std::shared_ptr<ContextState>> _state_stack;

    public:
        std::vector<TopLevelInitializerDef> top_level_initializers;

        std::vector<EvaluationPhase> evaluation_context_stack;

        /// The global macro expanders
        std::unordered_map<std::string, std::shared_ptr<GlobalDef>> global_macros;

        /// The global var bindings
        std::unordered_map<std::string, std::shared_ptr<GlobalDef>> global_bindings;

        /// The local bindings for the current level in the AST
        std::vector<std::unordered_map<std::string, llvm::Value *>> local_bindings;

        /* State */

        void push_new_state(std::string module_name);

        std::shared_ptr<ContextState> current_state();

        std::unique_ptr<llvm::Module> pop_state();

        /* Value Stack */
        void push_value(llvm::Value *val);

        llvm::Value *pop_value();

        /* Current Function */

        void push_func(llvm::Function *func);

        llvm::Function *pop_func();

        llvm::Function *current_func();

        /* Local Environment */

        void push_local_environment(const std::unordered_map<std::string, llvm::Value *> &new_env);

        void pop_local_environment();

        llvm::Value *lookup_in_local_environment(std::string name);

        llvm::Module * current_module();

        llvm::LLVMContext &llvm_context();

        std::shared_ptr<llvm::IRBuilder<>> current_builder();
    };
}

#endif //ELECTRUM_COMPILERCONTEXT_H