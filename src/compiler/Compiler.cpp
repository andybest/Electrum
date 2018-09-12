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

namespace electrum {
    Compiler::Compiler() {
        //module_ = std::make_unique<llvm::Module>("electrumModule", context_);
    }

    /*void Compiler::compile_analyzer_nodes(std::vector<std::shared_ptr<AnalyzerNode>> nodes) {

    }*/

    void Compiler::compile_node(std::shared_ptr<AnalyzerNode> node) {
        switch (node->nodeType()) {
            case kAnalyzerNodeTypeConstant:compile_constant(std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node));
            case kAnalyzerNodeTypeLambda:compile_lambda(std::dynamic_pointer_cast<LambdaAnalyzerNode>(node));
            case kAnalyzerNodeTypeDo:compile_do(std::dynamic_pointer_cast<DoAnalyzerNode>(node));
            case kAnalyzerNodeTypeIf:compile_if(std::dynamic_pointer_cast<IfAnalyzerNode>(node));
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
                //case kAnalyzerConstantTypeString:break;
            default:throw CompilerException("Unrecognized constant type", node->sourcePosition);
        }

        current_context()->push_value(v);
    }

    void Compiler::compile_lambda(std::shared_ptr<LambdaAnalyzerNode> node) {

    }

    void Compiler::compile_do(std::shared_ptr<DoAnalyzerNode> node) {

    }

    void Compiler::compile_if(std::shared_ptr<IfAnalyzerNode> node) {

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
}
