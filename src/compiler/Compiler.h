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

    void* compileAndEvalString(const std::string& str);
    void* compileAndEvalExpander(std::shared_ptr<MacroExpandAnalyzerNode> node);

private:

    llvm::orc::ExecutionSession es_;
    CompilerContext compiler_context_;
    Analyzer analyzer_;
    std::shared_ptr<ElectrumJit> jit_;

    /// Address space for the garbage collector
    static const int kGCAddressSpace = 1;

    CompilerContext* currentContext() { return &compiler_context_; }
    llvm::Module* currentModule() { return currentContext()->currentModule(); }
    llvm::LLVMContext& llvmContext() { return currentContext()->llvmContext(); }
    shared_ptr<llvm::IRBuilder<>> currentBuilder() { return currentContext()->currentBuilder(); }

    void* runInitializerWithJit(TopLevelInitializerDef& tl_def);
    TopLevelInitializerDef compileTopLevelNode(std::shared_ptr<AnalyzerNode> node);

    void compileNode(std::shared_ptr<AnalyzerNode> node);
    void compileConstant(std::shared_ptr<ConstantValueAnalyzerNode> node);
    void compileConstantList(const std::shared_ptr<ConstantListAnalyzerNode>& node);
    void compileLambda(const std::shared_ptr<LambdaAnalyzerNode>& node);
    void compileDef(const std::shared_ptr<DefAnalyzerNode>& node);
    void compileDo(const std::shared_ptr<DoAnalyzerNode>& node);
    void compileIf(const std::shared_ptr<IfAnalyzerNode>& node);
    void compileVarLookup(const std::shared_ptr<VarLookupNode>& node);
    void compileMaybeInvoke(const std::shared_ptr<MaybeInvokeAnalyzerNode>& node);
    void compileDefFFIFn(const std::shared_ptr<DefFFIFunctionNode>& node);
    void compileDefMacro(const std::shared_ptr<DefMacroAnalyzerNode>& node);
    void compileMacroExpand(const shared_ptr<MacroExpandAnalyzerNode>& node);

    std::string mangleSymbolName(std::string ns, const std::string& name);

    void createGCEntry();

    /* Standard library helpers */
    llvm::Value* makeNil();
    llvm::Value* makeInteger(int64_t value);
    llvm::Value* makeFloat(double value);
    llvm::Value* makeBoolean(bool value);
    llvm::Value* makeSymbol(std::shared_ptr<std::string> name);
    llvm::Value* makeString(std::shared_ptr<std::string> str);
    llvm::Value* makeKeyword(std::shared_ptr<std::string> name);
    llvm::Value* makeClosure(uint64_t arity, bool has_rest_args, llvm::Value* func_ptr, uint64_t env_size);
    llvm::Value* makePair(llvm::Value* v, llvm::Value* next);
    llvm::Value* getBooleanValue(llvm::Value* val);
    llvm::Value* makeVar(llvm::Value* sym);
    void buildSetVar(llvm::Value* var, llvm::Value* new_val);
    llvm::Value* buildDerefVar(llvm::Value* var);
    llvm::Value* buildGetLambdaPtr(llvm::Value* fn);
    llvm::Value* buildLambdaSetEnv(llvm::Value* fn, uint64_t idx, llvm::Value* val);
    llvm::Value* buildLambdaGetEnv(llvm::Value* fn, uint64_t idx);
    llvm::Value* buildGcAddRoot(llvm::Value* obj);
    llvm::Value* buildGcRemoveRoot(llvm::Value* obj);
    llvm::Value* buildApply(llvm::Value* f, llvm::Value* args);

    llvm::DISubroutineType* createFunctionDebugType(int num_args);
};
}

#endif //ELECTRUM_COMPILER_H
