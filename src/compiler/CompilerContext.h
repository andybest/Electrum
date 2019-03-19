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
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfo.h>
#include <boost/filesystem/path.hpp>
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

struct DebugInfo {
  std::shared_ptr<llvm::DIBuilder> builder;
  llvm::DICompileUnit *compile_unit;
  std::vector<llvm::DIScope*> lexical_blocks;

  llvm::DIScope *currentScope();
  llvm::DIType* getVoidPtrType();
  llvm::DIBasicType* void_ptr_type;
};

struct EHCompileInfo {
  llvm::BasicBlock *catch_dest;
};

struct ScopeInfo {
  vector<shared_ptr<EHCompileInfo>> eh_stack;

  void pushEHInfo(shared_ptr<EHCompileInfo> eh_info) {
      eh_stack.push_back(eh_info);
  }

  void popEHInfo() {
      assert(!eh_stack.empty());
      eh_stack.pop_back();
  }

  shared_ptr<EHCompileInfo> currentEHInfo() {
      if(eh_stack.empty()) {
          return nullptr;
      }

      return eh_stack.back();
  }
};

struct ContextState {
  std::shared_ptr<llvm::IRBuilder<>> builder;
  std::shared_ptr<DebugInfo> debug_info;
  std::unique_ptr<llvm::Module> module;
  std::vector<llvm::Value*> value_stack;
  std::vector<llvm::Function*> func_stack;
  vector<shared_ptr<ScopeInfo>> _scope_stack;
};

class  CompilerContext {
private:
    llvm::LLVMContext _context;
    std::vector<std::shared_ptr<ContextState>> _state_stack;

public:

    /// The global macro expanders
    std::unordered_map<std::string, std::shared_ptr<GlobalDef>> global_macros;

    /// The global var bindings
    std::unordered_map<std::string, std::shared_ptr<GlobalDef>> global_bindings;

    /// The local bindings for the current level in the AST
    std::vector<std::unordered_map<std::string, llvm::Value*>> local_bindings;

    /* State */
    void pushNewState(string module_name, const string& directory, const string& filename);
    std::shared_ptr<ContextState> currentState();
    std::unique_ptr<llvm::Module> popState();

    /* Value Stack */
    void pushValue(llvm::Value* val);
    llvm::Value* popValue();

    /* Current Function */
    void pushFunc(llvm::Function* func);
    llvm::Function* popFunc();
    llvm::Function* currentFunc();

    /* Local Environment */
    void pushLocalEnvironment(const std::unordered_map<std::string, llvm::Value*>& new_env);
    void popLocalEnvironment();
    llvm::Value* lookupInLocalEnvironment(std::string name);

    /* LLVM */
    llvm::Module* currentModule();
    llvm::LLVMContext& llvmContext();
    std::shared_ptr<llvm::IRBuilder<>> currentBuilder();
    std::shared_ptr<llvm::DIBuilder> currentDIBuilder();
    std::shared_ptr<DebugInfo> currentDebugInfo();

    /* Debug Info */
    void emitLocation(const shared_ptr<SourcePosition>& position);

    /* Scope */
    void pushScope();
    void popScope();
    shared_ptr<ScopeInfo> currentScope();
};
}

#endif //ELECTRUM_COMPILERCONTEXT_H