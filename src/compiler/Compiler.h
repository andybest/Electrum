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
#include "CompilerContext.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/ExecutionEngine/Orc/Core.h>

namespace electrum {

class Compiler {

 public:
  Compiler();

  void *compile_and_eval_string(std::string str);

  void *compile_and_eval_expander(std::shared_ptr<MacroExpandAnalyzerNode> node);

 private:

  llvm::orc::ExecutionSession es_;
  CompilerContext compiler_context_;
  Analyzer analyzer_;
  std::shared_ptr<ElectrumJit> jit_;

  /// Address space for the garbage collector
  static const int kGCAddressSpace = 1;

  CompilerContext *current_context() { return &compiler_context_; }

  llvm::Module *current_module() { return current_context()->current_module(); }

  llvm::LLVMContext &llvm_context() { return current_context()->llvm_context(); }

  shared_ptr<llvm::IRBuilder<>> current_builder() { return current_context()->current_builder(); }

  void create_gc_entry();

  void *run_initializer_with_jit(TopLevelInitializerDef &tl_def);

  TopLevelInitializerDef compile_top_level_node(std::shared_ptr<AnalyzerNode> node);

  void compile_node(std::shared_ptr<AnalyzerNode> node);

  void compile_constant(std::shared_ptr<ConstantValueAnalyzerNode> node);

  void compile_constant_list(const std::shared_ptr<ConstantListAnalyzerNode> &node);

  void compile_lambda(const std::shared_ptr<LambdaAnalyzerNode> &node);

  void compile_def(const std::shared_ptr<DefAnalyzerNode> &node);

  void compile_do(const std::shared_ptr<DoAnalyzerNode> &node);

  void compile_if(const std::shared_ptr<IfAnalyzerNode> &node);

  void compile_var_lookup(const std::shared_ptr<VarLookupNode> &node);

  void compile_maybe_invoke(const std::shared_ptr<MaybeInvokeAnalyzerNode> &node);

  void compile_def_ffi_fn(const std::shared_ptr<DefFFIFunctionNode> &node);

  void compile_def_macro(const std::shared_ptr<DefMacroAnalyzerNode> &node);

  void compile_macro_expand(const shared_ptr <MacroExpandAnalyzerNode> &node);

  std::string mangle_symbol_name(std::string ns, const std::string &name);

  llvm::Value *make_nil();

  llvm::Value *make_integer(int64_t value);

  llvm::Value *make_float(double value);

  llvm::Value *make_boolean(bool value);

  llvm::Value *make_symbol(std::shared_ptr<std::string> name);

  llvm::Value *make_string(std::shared_ptr<std::string> str);

  llvm::Value *make_keyword(std::shared_ptr<std::string> name);

  llvm::Value *make_closure(uint64_t arity, llvm::Value *func_ptr, uint64_t env_size);

  llvm::Value *make_pair(llvm::Value *v, llvm::Value *next);

  llvm::Value *get_boolean_value(llvm::Value *val);

  llvm::Value *make_var(llvm::Value *sym);

  void build_set_var(llvm::Value *var, llvm::Value *new_val);

  llvm::Value *build_deref_var(llvm::Value *var);

  llvm::Value *build_get_lambda_ptr(llvm::Value *fn);

  llvm::Value *build_lambda_set_env(llvm::Value *fn, uint64_t idx, llvm::Value *val);

  llvm::Value *build_lambda_get_env(llvm::Value *fn, uint64_t idx);

  llvm::Value *build_gc_add_root(llvm::Value *obj);
};
}

#endif //ELECTRUM_COMPILER_H
