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

#include <llvm/Support/raw_ostream.h>

#include <llvm/CodeGen/GCs.h>
#include <llvm/CodeGen/GCStrategy.h>
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
        llvm::linkStatepointExampleGC();
        _jit = std::make_shared<ElectrumJit>(_es);
    }

    /*void Compiler::compile_analyzer_nodes(std::vector<std::shared_ptr<AnalyzerNode>> nodes) {

    }*/

    void *Compiler::compile_and_eval_string(std::string str) {
        Parser p;
        auto ast = p.readString(str);
        auto node = _analyzer.analyzeForm(ast);
        _analyzer.closedOversForNode(node);

        return compile_and_eval_node(node);
    }

    void *Compiler::compile_and_eval_node(std::shared_ptr<AnalyzerNode> node) {
        static int cnt = 0;

        std::stringstream moduless;
        moduless << "test_module_" << cnt;
        _module = std::make_unique<llvm::Module>(moduless.str(), _context);

        std::stringstream ss;
        ss << "jit_func_" << cnt;

        auto mainfunc = llvm::Function::Create(
                llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace), false),
                llvm::GlobalValue::LinkageTypes::ExternalLinkage,
                ss.str(),
                _module.get());

        mainfunc->setGC("statepoint-example");

        auto gcType = llvm::FunctionType::get(llvm::Type::getVoidTy(_context), false);
        auto gcfunc = llvm::dyn_cast<llvm::Function>(_module->getOrInsertFunction("gc.safepoint_poll", gcType));

        gcfunc->addFnAttr(llvm::Attribute::NoUnwind);

        auto gcEntry = llvm::BasicBlock::Create(_context, "entry", gcfunc);
        llvm::IRBuilder<> b(_context);
        b.SetInsertPoint(gcEntry);

        auto dogc = _module->getOrInsertFunction("rt_enter_gc",
                                                 llvm::Type::getVoidTy(_context));
        //b.CreateGCStatepointCall(2863311530, 0, dogc, nullptr, nullptr, nullptr);
        auto inst = b.CreateCall(dogc);
        b.CreateRet(nullptr);

        current_context()->push_func(mainfunc);

        auto entry = llvm::BasicBlock::Create(_context, "entry", mainfunc);
        _builder = std::make_unique<llvm::IRBuilder<>>(_context);

        _builder->SetInsertPoint(entry);
        compile_node(node);

        // Return result.
        auto result = current_context()->pop_value();
        _builder->CreateRet(result);
        current_context()->pop_func();

        _jit->addModule(std::move(_module));

        auto faddr = _jit->get_symbol_address(ss.str());
        rt_gc_init_stackmap(_jit->get_stack_map_pointer());

        typedef void *(*MainPtr)();

        auto fp = reinterpret_cast<MainPtr>(faddr);
        auto rv = fp();

        ++cnt;

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
            case kAnalyzerNodeTypeDef: compile_def(std::dynamic_pointer_cast<DefAnalyzerNode>(node));
                break;
            case kAnalyzerNodeTypeVarLookup: compile_var_lookup(std::dynamic_pointer_cast<VarLookupNode>(node));
                break;
            case kAnalyzerNodeTypeMaybeInvoke: compile_maybe_invoke(std::dynamic_pointer_cast<MaybeInvokeAnalyzerNode>(node));
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
            case kAnalyzerConstantTypeNil: v = make_nil();
                break;
            default:throw CompilerException("Unrecognized constant type", node->sourcePosition);
        }

        current_context()->push_value(v);
    }

    void Compiler::compile_do(std::shared_ptr<DoAnalyzerNode> node) {
        // Compile each node in the body, disregarding the result
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

    void Compiler::compile_var_lookup(std::shared_ptr<VarLookupNode> node) {
        if (node->is_global) {
            auto result = current_context()->global_bindings.find(*node->name);

            if (result == current_context()->global_bindings.end()) {
                throw CompilerException("Fatal compiler error: no var",
                                        node->sourcePosition);
            }

            auto def = result->second;

            auto var = _module->getOrInsertGlobal(def->mangled_name,
                                                  llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace));
            auto v = _builder->CreateLoad(var);

            auto val = build_deref_var(v);
            current_context()->push_value(val);
            return;
        }

        auto result = current_context()->lookup_in_local_environment(*node->name);
        if(result != nullptr) {
            current_context()->push_value(result);
            return;
        }

        throw CompilerException("Unsupported var type", node->sourcePosition);
    }

    void Compiler::compile_lambda(std::shared_ptr<LambdaAnalyzerNode> node) {
        // TODO: This is temporary
        static int cnt = 0;

        auto insert_block = _builder->GetInsertBlock();
        auto insert_point = _builder->GetInsertPoint();

        std::stringstream ss;
        ss << "lambda_" << cnt;

        std::vector<llvm::Type *> argTypes;

        argTypes.reserve(node->arg_names.size() + 1);
        for (int i = 0; i < node->arg_names.size(); ++i) {
            argTypes.push_back(llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace));

        }

        // Add extra arg for closure, in order to ger environment values
        argTypes.push_back(llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace));


        auto lambdaType = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                  argTypes,
                                                  false);

        auto lambda = llvm::Function::Create(
                lambdaType,
                llvm::GlobalValue::LinkageTypes::ExternalLinkage,
                ss.str(),
                _module.get());

        lambda->setGC("statepoint-example");

        auto entryBlock = llvm::BasicBlock::Create(_context, "entry", lambda);
        _builder->SetInsertPoint(entryBlock);

        std::unordered_map<std::string, llvm::Value *> local_env;
        int argNum = 0;
        auto arg_it = lambda->args().begin();
        for (auto &arg_name : node->arg_names) {
            auto &arg = *arg_it;
            arg.setName(*arg_name);

            local_env[*arg_name] = &arg;

            ++argNum;
            ++arg_it;
        }

        //++arg_it;
        for(uint64_t i = 0; i < node->closed_overs.size(); i++) {
            auto &arg = *arg_it;
            arg.setName("environment");

            local_env[node->closed_overs[i]] = build_lambda_get_env(&arg, i);
        }

        // Push the arguments onto the environment stack so that the compiler
        // can look them up later
        current_context()->push_local_environment(local_env);
        current_context()->push_func(lambda);

        // Compile the body of the function
        compile_node(node->body);
        _builder->CreateRet(current_context()->pop_value());

        // Scope ended, pop the arguments from the environment stack
        current_context()->pop_local_environment();
        current_context()->pop_func();

        // Restore back to previous insert point
        _builder->SetInsertPoint(insert_block, insert_point);


        auto closure = make_closure(node->arg_names.size(),
                                    lambda,
                                    node->closed_overs.size());

        for(uint64_t i = 0; i < node->closed_overs.size(); i++) {
            build_lambda_set_env(closure, i, current_context()->lookup_in_local_environment(node->closed_overs[i]));
        }

        current_context()->push_value(closure);
    }

    void Compiler::compile_def(std::shared_ptr<DefAnalyzerNode> node) {
        auto mangled_name = mangle_symbol_name("", *node->name);

        auto glob = new llvm::GlobalVariable(*_module,
                                             llvm::IntegerType::getInt8PtrTy(_context, 0),
                                             false,
                                             llvm::GlobalValue::LinkageTypes::ExternalLinkage,
                                             nullptr,
                                             mangled_name);

        glob->setInitializer(llvm::UndefValue::getNullValue(llvm::IntegerType::getInt8PtrTy(_context, 0)));

        auto nameSym = make_symbol(node->name);
        auto v = make_var(nameSym);

        // Store var in global
        _builder->CreateStore(v, glob, false);

        // Compile value
        compile_node(node->value);

        // Set initial value for var
        build_set_var(v, current_context()->pop_value());

        current_context()->push_value(make_nil());

        auto d = std::make_shared<GlobalDef>();
        d->name = *node->name;
        d->mangled_name = mangled_name;

        current_context()->global_bindings[*node->name] = d;
    }

    void Compiler::compile_maybe_invoke(std::shared_ptr<MaybeInvokeAnalyzerNode> node) {
        compile_node(node->fn);
        auto fn = current_context()->pop_value();

        std::vector<llvm::Value*> args;
        args.reserve(node->args.size() + 1);

        for(const auto &a: node->args) {
            compile_node(a);
            args.push_back(current_context()->pop_value());
        }

        args.push_back(fn);

        std::vector<llvm::Type*> argTypes;
        for(uint64_t i = 0; i < node->args.size() + 1; ++i) {
            argTypes.push_back(llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace));
        }

        argTypes.push_back(llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace));

        auto fn_type = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                               argTypes,
                                               false);

        auto fn_ptr = _builder->CreateFPCast(build_get_lambda_ptr(fn),
                llvm::PointerType::get(fn_type, 0));

        //auto result = _builder->CreateGCStatepointCall(0, 0, fn_ptr, args, nullptr, nullptr);
        auto result = _builder->CreateCall(fn_ptr, args);

        current_context()->push_value(result);
    }

    std::string Compiler::mangle_symbol_name(const std::string ns, const std::string &name) {
        // Ignore ns for now, as we don't support yet
        std::stringstream ss;
        ss << "__elec__" << name << "__";
        return ss.str();
    }


#pragma mark - Helpers

    llvm::Value *Compiler::make_nil() {
        auto func = _module->getOrInsertFunction("rt_make_nil",
                                                 llvm::IntegerType::getInt8PtrTy(_context, 0));

        return _builder->CreateCall(func);
    }

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

    llvm::Value *Compiler::make_closure(uint64_t arity, llvm::Value *func_ptr, uint64_t envSize) {
        auto func = _module->getOrInsertFunction("rt_make_compiled_function",
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt64Ty(_context),
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt64Ty(_context));


        return _builder->CreateCall(func,
                                    {llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(_context), arity),
                                     func_ptr,
                                     llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(_context), envSize)});
    }

    llvm::Value *Compiler::get_boolean_value(llvm::Value *val) {
        auto func = _module->getOrInsertFunction("rt_is_true",
                                                 llvm::IntegerType::getInt8Ty(_context),
                                                 llvm::IntegerType::getInt8PtrTy(_context, 0));

        return _builder->CreateCall(func, {val});
    }

    llvm::Value *Compiler::make_var(llvm::Value *sym) {
        auto func = _module->getOrInsertFunction("rt_make_var",
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace));

        return _builder->CreateCall(func, {sym});
    }

    void Compiler::build_set_var(llvm::Value *var, llvm::Value *newVal) {
        auto func = _module->getOrInsertFunction("rt_set_var",
                                                 llvm::Type::getVoidTy(_context),
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace));

        _builder->CreateCall(func, {var, newVal});
    }

    llvm::Value *Compiler::build_deref_var(llvm::Value *var) {
        auto func = _module->getOrInsertFunction("rt_deref_var",
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace));

        return _builder->CreateCall(func, {var});
    }

    llvm::Value *Compiler::build_get_lambda_ptr(llvm::Value *fn) {
        auto func = _module->getOrInsertFunction("rt_compiled_function_get_ptr",
                llvm::IntegerType::getInt8PtrTy(_context, 0));
        return _builder->CreateCall(func, {fn});
    }

    llvm::Value *Compiler::build_lambda_set_env(llvm::Value *fn, uint64_t idx, llvm::Value *val) {
        auto func = _module->getOrInsertFunction("rt_compiled_function_set_env",
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt64Ty(_context),
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace));

        llvm::Value *idxVal = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(_context), idx);

        return _builder->CreateCall(func, {fn, idxVal, val});
    }

    llvm::Value *Compiler::build_lambda_get_env(llvm::Value *fn, uint64_t idx) {
        auto func = _module->getOrInsertFunction("rt_compiled_function_get_env",
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt8PtrTy(_context, kGCAddressSpace),
                                                 llvm::IntegerType::getInt64Ty(_context));

        llvm::Value *idxVal = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(_context), idx);

        return _builder->CreateCall(func, {fn, idxVal});
    }
}
