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


#include "Compiler.h"
#include "Analyzer.h"
#include "CompilerExceptions.h"
#include "Parser.h"
#include <runtime/Runtime.h>
#include "ElectrumJit.h"

#include <llvm/Support/raw_ostream.h>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"

namespace electrum {
    Compiler::Compiler() {
        //module_ = std::make_unique<llvm::Module>("electrumModule", context_);
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();
    }

    /*void Compiler::compile_analyzer_nodes(std::vector<std::shared_ptr<AnalyzerNode>> nodes) {

    }*/

    void *Compiler::compile_and_eval_string(std::string str) {
        Parser p;
        auto ast = p.readString(str);
        Analyzer a;
        auto node = a.analyzeForm(ast);

        return compile_and_eval_node(node);
    }

    void *Compiler::compile_and_eval_node(std::shared_ptr<AnalyzerNode> node) {
        _module = std::make_unique<llvm::Module>("test_module", _context);

        auto mainfunc = llvm::Function::Create(
                llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace), false),
                llvm::GlobalValue::LinkageTypes::ExternalLinkage,
                "jitmain",
                _module.get());

        current_context()->push_func(mainfunc);

        auto entry = llvm::BasicBlock::Create(_context, "entry", mainfunc);
        _builder = std::make_unique<llvm::IRBuilder<>>(_context);

        _builder->SetInsertPoint(entry);
        compile_node(node);

        // Return result.
        auto result = current_context()->pop_value();
        _builder->CreateRet(result);

        _module->print(llvm::errs(), nullptr);

        ElectrumJit jit;
        jit.addModule(std::move(_module));

        auto faddr = jit.get_symbol_address("jitmain");

        typedef void* (*MainPtr)();

        auto fp = reinterpret_cast<MainPtr>(faddr);
        auto rv = fp();

        current_context()->pop_func();

        return rv;
    }

    void Compiler::compile_node(std::shared_ptr<AnalyzerNode> node) {
        switch (node->nodeType()) {
            case kAnalyzerNodeTypeConstant:compile_constant(std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node));
                break;
            case kAnalyzerNodeTypeLambda:compile_lambda(std::dynamic_pointer_cast<LambdaAnalyzerNode>(node));
                break;
            case kAnalyzerNodeTypeDo:compile_do(std::dynamic_pointer_cast<DoAnalyzerNode>(node));
                break;
            case kAnalyzerNodeTypeIf:compile_if(std::dynamic_pointer_cast<IfAnalyzerNode>(node));
                break;
            default:throw CompilerException("Unrecognized node type", node->sourcePosition);
        }
    }

    void Compiler::compile_constant(std::shared_ptr<ConstantValueAnalyzerNode> node) {
        llvm::Value *v;

        switch (node->type) {
            case kAnalyzerConstantTypeInteger: v = make_integer(boost::get<int64_t>(node->value));
                break;
            case kAnalyzerConstantTypeFloat: v = make_float(boost::get<double>(node->value));
                break;
            case kAnalyzerConstantTypeBoolean: v = make_boolean(boost::get<bool>(node->value));
                break;
            case kAnalyzerConstantTypeSymbol: v = make_symbol(boost::get<shared_ptr<std::string>>(node->value));
                break;
            case kAnalyzerConstantTypeString: v = make_string(boost::get<shared_ptr<std::string>>(node->value));
                break;
            default:throw CompilerException("Unrecognized constant type", node->sourcePosition);
        }

        current_context()->push_value(v);
    }

    void Compiler::compile_do(std::shared_ptr<DoAnalyzerNode> node) {
        // Compile each node in the body, desregarding the result
        for (const auto &child: node->statements) {
            compile_node(child);
            current_context()->pop_value();
        }

        // Compile the last node, keeping the result on the stack
        compile_node(node->returnValue);
    }

    void Compiler::compile_if(std::shared_ptr<IfAnalyzerNode> node) {
        // Compile the condition to the stack
        compile_node(node->condition);

        // Create stack variable to hold result
        auto result = _builder->CreateAlloca(llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                             kGCAddressSpace, nullptr, "if_result");

        auto cond = _builder->CreateICmpEQ(get_boolean_value(current_context()->pop_value()),
                                           llvm::ConstantInt::get(llvm::IntegerType::getInt8Ty(_context), 0));

        auto iftrueblock = llvm::BasicBlock::Create(_context, "if_true", current_context()->current_func());
        auto iffalseblock = llvm::BasicBlock::Create(_context, "if_false", current_context()->current_func());
        auto endifblock = llvm::BasicBlock::Create(_context, "endif", current_context()->current_func());

        _builder->CreateCondBr(cond, iffalseblock, iftrueblock);

        // True branch
        _builder->SetInsertPoint(iftrueblock);
        compile_node(node->consequent);
        _builder->CreateStore(current_context()->pop_value(), result);
        _builder->CreateBr(endifblock);

        // False branch
        _builder->SetInsertPoint(iffalseblock);
        compile_node(node->alternative);
        _builder->CreateStore(current_context()->pop_value(), result);
        _builder->CreateBr(endifblock);

        // End if
        _builder->SetInsertPoint(endifblock);
        // Push the result of the if expression to the compiler stack
        current_context()->push_value(_builder->CreateLoad(result));
    }

    void Compiler::compile_lambda(std::shared_ptr<LambdaAnalyzerNode> node) {

    }


#pragma mark - Helpers

    llvm::Value *Compiler::make_integer(int64_t value) {
        // Don't put returned pointer in GC address space, as integers are tagged,
        // and not heap allocated.
        auto func = _module->getOrInsertFunction("rt_make_integer",
                                                 llvm::IntegerType::getInt8PtrTy(_context, 0),
                                                 llvm::IntegerType::getInt64Ty(_context));

        return _builder->CreateCall(func,
                                    {llvm::ConstantInt::getSigned(llvm::IntegerType::getInt64Ty(_context), value)});
    }

    llvm::Value *Compiler::make_float(double value) {
        auto func = _module->getOrInsertFunction("rt_make_float",
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::Type::getDoubleTy(_context));

        return _builder->CreateCall(func,
                                    {llvm::ConstantFP::get(llvm::Type::getDoubleTy(_context), value)});
    }

    llvm::Value *Compiler::make_boolean(bool value) {
        // Don't put returned pointer in GC address space, as booleans are tagged,
        // and not heap allocated.
        auto func = _module->getOrInsertFunction("rt_make_boolean",
                                                 llvm::IntegerType::getInt8PtrTy(_context, 0),
                                                 llvm::IntegerType::getInt8Ty(_context));

        return _builder->CreateCall(func,
                                    {llvm::ConstantInt::getSigned(llvm::IntegerType::getInt8Ty(_context),
                                                                  value ? 1 : 0)});
    }

    llvm::Value *Compiler::make_symbol(std::shared_ptr<std::string> name) {
        auto func = _module->getOrInsertFunction("rt_make_symbol",
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt8PtrTy(_context, 0));

        auto strptr = _builder->CreateGlobalStringPtr(*name);
        return _builder->CreateCall(func, {strptr});
    }

    llvm::Value *Compiler::make_string(std::shared_ptr<std::string> str) {
        auto func = _module->getOrInsertFunction("rt_make_string",
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt8PtrTy(_context, 0));

        auto strptr = _builder->CreateGlobalStringPtr(*str);
        return _builder->CreateCall(func, {strptr});
    }

    llvm::Value *Compiler::make_keyword(std::shared_ptr<std::string> name) {
        auto func = _module->getOrInsertFunction("rt_make_keyword",
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt8PtrTy(_context, 0));

        auto strptr = _builder->CreateGlobalStringPtr(*name);
        return _builder->CreateCall(func, {strptr});
    }

    llvm::Value *Compiler::make_closure(uint64_t arity, llvm::Value *environment, llvm::Value *func_ptr) {

        // void *rt_make_compiled_function(uint64_t arity, void *env, void *fp)
        auto func = _module->getOrInsertFunction("rt_make_compiled_function",
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt64Ty(_context),
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt8PtrTy(_context, 0));


        return _builder->CreateCall(func,
                                    {llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(_context), arity),
                                     environment,
                                     func_ptr});
    }

    llvm::Value *Compiler::get_boolean_value(llvm::Value *val) {
        auto func = _module->getOrInsertFunction("rt_is_true",
                                                 llvm::IntegerType::getInt8Ty(_context),
                                                 llvm::IntegerType::getInt8PtrTy(_context, 0));

        return _builder->CreateCall(func, {val});
    }
}
