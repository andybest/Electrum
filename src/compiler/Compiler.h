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
#include "ElectrumJit.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/ExecutionEngine/Orc/Core.h>

namespace electrum {

    struct GlobalDef {
        std::string name;
        std::string mangled_name;
    };

    struct CompilerContext {
        std::vector<llvm::Value *> value_stack;
        std::vector<llvm::Function *> func_stack;

        /// The global var bindings
        std::unordered_map<std::string, shared_ptr<GlobalDef>> global_bindings;

        /** Global function bindings. Functions that end up here will be
         * called statically, rather than going through var -> closure
         * indirection.
         */
       // std::unordered_map<std::string, shared_ptr<GlobalDef>> global_func_bindings;

        /// The local bindings for the current level in the AST
        std::vector<std::unordered_map<std::string, llvm::Value*>> local_bindings;

        void push_value(llvm::Value *val) {
            value_stack.push_back(val);
        }

        llvm::Value *pop_value() {
            auto v = value_stack.back();
            value_stack.pop_back();
            return v;
        }

        void push_func(llvm::Function *func) {
            func_stack.push_back(func);
        }

        llvm::Function *pop_func() {
            auto f = func_stack.back();
            func_stack.pop_back();
            return f;
        }

        llvm::Function *current_func() {
            return func_stack.back();
        }

        void push_local_environment() {
            local_bindings.emplace_back();
        }

        void push_local_environment(const std::unordered_map<std::string, llvm::Value*> &new_env) {
            local_bindings.push_back(new_env);
        }

        void pop_local_environment() {
            local_bindings.pop_back();
        }

        llvm::Value *lookup_in_local_environment(const std::string name) {
            for(auto it = local_bindings.rbegin(); it != local_bindings.rend(); ++it) {
                auto env = *it;
                auto result = env.find(name);

                if(result != env.end()) {
                    return result->second;
                }
            }

            return nullptr;
        }

    };

    class Compiler {

    public:
        Compiler();

        void *compile_and_eval_string(std::string str);

        void *compile_and_eval_node(std::shared_ptr<AnalyzerNode> node);

    private:

        llvm::LLVMContext _context;
        llvm::orc::ExecutionSession _es;
        std::unique_ptr<llvm::Module> _module;
        std::unique_ptr<llvm::IRBuilder<>> _builder;
        CompilerContext _compilerContext;
        Analyzer _analyzer;
        std::shared_ptr<ElectrumJit> _jit;

        /// Address space for the garbage collector
        static const int kGCAddressSpace = 1;

        CompilerContext *current_context() { return &_compilerContext; }

        void compile_node(std::shared_ptr<AnalyzerNode> node);

        void compile_constant(std::shared_ptr<ConstantValueAnalyzerNode> node);

        void compile_lambda(std::shared_ptr<LambdaAnalyzerNode> node);

        void compile_def(std::shared_ptr<DefAnalyzerNode> node);

        void compile_do(std::shared_ptr<DoAnalyzerNode> node);

        void compile_if(std::shared_ptr<IfAnalyzerNode> node);

        void compile_var_lookup(std::shared_ptr<VarLookupNode> node);

        void compile_maybe_invoke(std::shared_ptr<MaybeInvokeAnalyzerNode> node);

        std::string mangle_symbol_name(std::string ns, const std::string &name);

        llvm::Value *make_nil();

        llvm::Value *make_integer(int64_t value);

        llvm::Value *make_float(double value);

        llvm::Value *make_boolean(bool value);

        llvm::Value *make_symbol(std::shared_ptr<std::string> name);

        llvm::Value *make_string(std::shared_ptr<std::string> str);

        llvm::Value *make_keyword(std::shared_ptr<std::string> name);

        llvm::Value *make_closure(uint64_t arity, llvm::Value *environment, llvm::Value *func_ptr);

        llvm::Value *get_boolean_value(llvm::Value *val);

        llvm::Value *make_var(llvm::Value *sym);

        void build_set_var(llvm::Value *var, llvm::Value *newVal);

        llvm::Value *build_deref_var(llvm::Value *var);

        llvm::Value *build_get_lambda_ptr(llvm::Value *fn);
    };
}


#endif //ELECTRUM_COMPILER_H
