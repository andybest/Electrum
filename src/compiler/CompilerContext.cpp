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

#include <include/types/Types.h>
#include <sstream>
#include "CompilerContext.h"

namespace electrum {
#pragma mark Value Stack

void CompilerContext::pushValue(llvm::Value* val) {
    currentState()->value_stack.push_back(val);
}

llvm::Value* CompilerContext::popValue() {
    auto v = currentState()->value_stack.back();
    currentState()->value_stack.pop_back();
    return v;
}

#pragma mark Current Function

void CompilerContext::pushFunc(llvm::Function* func) {
    currentState()->func_stack.push_back(func);
}

llvm::Function* CompilerContext::popFunc() {
    auto f = currentState()->func_stack.back();
    currentState()->func_stack.pop_back();
    return f;
}

llvm::Function* CompilerContext::currentFunc() {
    // If there are no functions on the stack, return the current top level initializer
    if (currentState()->func_stack.empty()) {
        return nullptr;
    }
    return currentState()->func_stack.back();
}

#pragma mark Local Environment

void CompilerContext::pushLocalEnvironment(const std::unordered_map<std::string, llvm::Value*>& new_env) {
    local_bindings.push_back(new_env);
}

void CompilerContext::popLocalEnvironment() {
    local_bindings.pop_back();
}

llvm::Value* CompilerContext::lookupInLocalEnvironment(const std::string name) {
    for (auto it = local_bindings.rbegin(); it!=local_bindings.rend(); ++it) {
        auto env = *it;
        auto result = env.find(name);

        if (result!=env.end()) {
            return result->second;
        }
    }

    return nullptr;
}

llvm::Module* CompilerContext::currentModule() {
    return currentState()->module.get();
}

llvm::LLVMContext& CompilerContext::llvmContext() {
    return _context;
}

std::shared_ptr<llvm::IRBuilder<>> CompilerContext::currentBuilder() {
    return currentState()->builder;
}

std::shared_ptr<llvm::DIBuilder> CompilerContext::currentDIBuilder() {
    return currentState()->debug_info->builder;
}

std::shared_ptr<DebugInfo> CompilerContext::currentDebugInfo() {
    return currentState()->debug_info;
}

void CompilerContext::pushNewState(std::string module_name) {
    auto s = std::make_shared<ContextState>();

    s->module = std::make_unique<llvm::Module>(module_name, _context);
    s->builder = std::make_shared<llvm::IRBuilder<>>(_context);
    s->debug_info = std::make_shared<DebugInfo>();
    s->debug_info->builder = std::make_shared<llvm::DIBuilder>(*s->module);

    // TODO: Temporary
    static int count = 0;

    std::stringstream ss;
    ss << "REPL_" << count << ".el";

    s->debug_info->compile_unit = s->debug_info->builder->createCompileUnit(
            llvm::dwarf::DW_LANG_C,
            s->debug_info->builder->createFile(ss.str(), "."),
            "Electrum Compiler",
            false,
            "",
            0);

    _state_stack.push_back(s);
}

std::shared_ptr<ContextState> CompilerContext::currentState() {
    assert(!_state_stack.empty());
    return _state_stack.back();
}

std::unique_ptr<llvm::Module> CompilerContext::popState() {
    auto state = _state_stack.back();
    _state_stack.pop_back();

    // TODO: Is this the best place for this?
    state->debug_info->builder->finalize();
    return std::move(state->module);
}

#pragma mark - DebugInfo

void CompilerContext::emitLocation(const std::shared_ptr<SourcePosition>& position) {
    llvm::DIScope *scope = currentDebugInfo()->currentScope();

    currentBuilder()->SetCurrentDebugLocation(
            llvm::DebugLoc::get(
                    position->line,
                    position->column,
                    scope
                    ));
}

llvm::DIScope* DebugInfo::currentScope() {
    if(lexical_blocks.empty()) {
        return compile_unit;
    }
    return lexical_blocks.back();
}

llvm::DIType* DebugInfo::getVoidPtrType() {
    if(this->void_ptr_type != nullptr) {
        return void_ptr_type;
    }

    void_ptr_type = builder->createBasicType("variable", 64, llvm::dwarf::DW_ATE_address);
    return void_ptr_type;
}
}