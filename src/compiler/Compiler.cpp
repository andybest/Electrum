#include <utility>

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

#pragma mark - Compiler

Compiler::Compiler() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    llvm::linkStatepointExampleGC();
    jit_ = std::make_shared<ElectrumJit>(es_);
}

void* Compiler::compile_and_eval_string(std::string str) {
    Parser p;
    auto ast = p.readString(std::move(str));

    current_context()->push_new_state("jit_module");
    create_gc_entry();

    // Analyze as a top level form
    auto node = analyzer_.analyze(ast, 0);
    auto toplevel_forms = analyzer_.collapse_top_level_forms(node);
    void* rv = NIL_PTR;

    // Run each top level initializer
    for (const auto& f: toplevel_forms) {
        auto tl_def = compile_top_level_node(f);
        rv = run_initializer_with_jit(tl_def);
    }

    rt_gc_add_root(rv);

    return rv;
}

TopLevelInitializerDef Compiler::compile_top_level_node(std::shared_ptr<AnalyzerNode> node) {
    static int cnt = 0;
    std::stringstream ss;
    ss << "toplevel_" << cnt;
    auto mangled_name = ss.str();
    ++cnt;

    TopLevelInitializerDef initializer;
    initializer.mangled_name = mangled_name;
    initializer.evaluation_phases = node->evaluation_phase;
    initializer.evaluated_in = kEvaluationPhaseNone;

    auto mainfunc = llvm::Function::Create(
            llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace), false),
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            mangled_name,
            current_context()->current_module());

    mainfunc->setGC("statepoint-example");

    current_context()->push_func(mainfunc);

    auto entry = llvm::BasicBlock::Create(llvm_context(), "entry", mainfunc);

    current_builder()->SetInsertPoint(entry);
    compile_node(std::move(node));

    // Return result.
    auto result = current_context()->pop_value();

    // TODO: Figure out what to do with return values rather than adding as a root
    //build_gc_add_root(result);

    current_builder()->CreateRet(result);
    current_context()->pop_func();

    return initializer;
}

void* Compiler::run_initializer_with_jit(TopLevelInitializerDef& tl_def) {
    static int cnt = 0;

    if (current_module()->getFunction(tl_def.mangled_name)!=nullptr) {
        jit_->addModule(current_context()->pop_state());

        auto stackmap_ptr = jit_->get_stack_map_pointer();
        if (stackmap_ptr!=nullptr) {
            rt_gc_init_stackmap(stackmap_ptr);
        }

        std::stringstream ss;
        ss << "jit_module__" << cnt;
        current_context()->push_new_state(ss.str());
        create_gc_entry();
    }

    auto f_addr = jit_->get_symbol_address(tl_def.mangled_name);
    typedef void* (* InitFunc)();
    auto f_ptr = reinterpret_cast<InitFunc>(f_addr);
    return f_ptr();
}

void* Compiler::compile_and_eval_expander(std::shared_ptr<MacroExpandAnalyzerNode> node) {
    static int cnt = 0;

    std::stringstream moduless;
    moduless << "expander_module_" << cnt;

    current_context()->push_new_state(moduless.str());
    create_gc_entry();

    std::stringstream ss;
    ss << "expansion_func_" << cnt;

    auto mainfunc = llvm::Function::Create(
            llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace), false),
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            ss.str(),
            current_context()->current_module());

    mainfunc->setGC("statepoint-example");

    current_context()->push_func(mainfunc);

    auto entry = llvm::BasicBlock::Create(llvm_context(), "entry", mainfunc);

    current_builder()->SetInsertPoint(entry);
    compile_macro_expand(node);

    // Return result.
    auto result = current_context()->pop_value();

    // TODO: Figure out what to do with return values rather than adding as a root
    build_gc_add_root(result);

    current_builder()->CreateRet(result);
    current_context()->pop_func();

    jit_->addModule(current_context()->pop_state());

    auto faddr = jit_->get_symbol_address(ss.str());
    rt_gc_init_stackmap(jit_->get_stack_map_pointer());

    typedef void* (* MainPtr)();

    auto fp = reinterpret_cast<MainPtr>(faddr);
    auto rv = fp();

    ++cnt;

    return rv;
}

void Compiler::create_gc_entry() {
    auto gc_type = llvm::FunctionType::get(llvm::Type::getVoidTy(current_context()->llvm_context()), false);
    auto gcfunc = llvm::dyn_cast<llvm::Function>(
            current_context()->current_module()->getOrInsertFunction("gc.safepoint_poll", gc_type));

    gcfunc->addFnAttr(llvm::Attribute::NoUnwind);

    auto gc_entry = llvm::BasicBlock::Create(current_context()->llvm_context(), "entry", gcfunc);
    llvm::IRBuilder<> b(current_context()->llvm_context());
    b.SetInsertPoint(gc_entry);

    auto dogc = current_context()->current_module()->getOrInsertFunction("rt_enter_gc",
            llvm::Type::getVoidTy(current_context()
                    ->llvm_context()));
    b.CreateCall(dogc);
    b.CreateRet(nullptr);
}

void Compiler::compile_node(std::shared_ptr<AnalyzerNode> node) {
    switch (node->nodeType()) {
    case kAnalyzerNodeTypeConstant:compile_constant(std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeConstantList:compile_constant_list(std::dynamic_pointer_cast<ConstantListAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeLambda:compile_lambda(std::dynamic_pointer_cast<LambdaAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeDo:compile_do(std::dynamic_pointer_cast<DoAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeIf:compile_if(std::dynamic_pointer_cast<IfAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeDef:compile_def(std::dynamic_pointer_cast<DefAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeVarLookup:compile_var_lookup(std::dynamic_pointer_cast<VarLookupNode>(node));
        break;
    case kAnalyzerNodeTypeMaybeInvoke:compile_maybe_invoke(std::dynamic_pointer_cast<MaybeInvokeAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeDefFFIFunction:compile_def_ffi_fn(std::dynamic_pointer_cast<DefFFIFunctionNode>(node));
        break;
    case kAnalyzerNodeTypeDefMacro:compile_def_macro(std::dynamic_pointer_cast<DefMacroAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeMacroExpand: {
        auto macro_expand_node = std::dynamic_pointer_cast<MacroExpandAnalyzerNode>(node);
        if (!macro_expand_node->do_evaluate) {
            compile_macro_expand(macro_expand_node);
        }
        else {
            auto expansion = compile_and_eval_expander(macro_expand_node);
            Parser p;
            auto form = p.readLispValue(expansion);
            auto expanded_node = analyzer_.analyze(form, node->node_depth, node->evaluation_phase);
            compile_node(expanded_node);
        }
        break;
    }

    default:throw CompilerException("Unrecognized node type", node->sourcePosition);
    }
}

void Compiler::compile_constant(std::shared_ptr<ConstantValueAnalyzerNode> node) {
    llvm::Value* v;

    switch (node->type) {
    case kAnalyzerConstantTypeInteger:v = make_integer(boost::get<int64_t>(node->value));
        break;
    case kAnalyzerConstantTypeFloat:v = make_float(boost::get<double>(node->value));
        break;
    case kAnalyzerConstantTypeBoolean:v = make_boolean(boost::get<bool>(node->value));
        break;
    case kAnalyzerConstantTypeSymbol:v = make_symbol(boost::get<shared_ptr<std::string>>(node->value));
        break;
    case kAnalyzerConstantTypeString:v = make_string(boost::get<shared_ptr<std::string>>(node->value));
        break;
    case kAnalyzerConstantTypeNil:v = make_nil();
        break;
    default:throw CompilerException("Unrecognized constant type", node->sourcePosition);
    }

    current_context()->push_value(v);
}

void Compiler::compile_constant_list(const std::shared_ptr<ConstantListAnalyzerNode>& node) {
    // Special case- the empty list is nil
    if (node->values.empty()) {
        current_context()->push_value(make_nil());
        return;
    }

    llvm::Value* head = make_nil();

    for (auto it = node->values.rbegin(); it!=node->values.rend(); ++it) {
        compile_node(*it);
        head = make_pair(current_context()->pop_value(), head);
    }

    current_context()->push_value(head);
}

void Compiler::compile_do(const std::shared_ptr<DoAnalyzerNode>& node) {
    // Compile each node in the body, disregarding the result
    for (const auto& child: node->statements) {
        compile_node(child);
        current_context()->pop_value();
    }

    // Compile the last node, keeping the result on the stack
    compile_node(node->returnValue);
}

void Compiler::compile_if(const std::shared_ptr<IfAnalyzerNode>& node) {
    // Compile the condition to the stack
    compile_node(node->condition);

    // Create stack variable to hold result
    // TODO: Make sure this doesn't need to be in the GC Address space
    auto result = current_builder()->CreateAlloca(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace),
            0, nullptr, "if_result");

    auto cond = current_builder()->CreateICmpEQ(get_boolean_value(current_context()->pop_value()),
            llvm::ConstantInt::get(llvm::IntegerType::getInt8Ty(llvm_context()),
                    0));

    auto iftrueblock = llvm::BasicBlock::Create(llvm_context(), "if_true", current_context()->current_func());
    auto iffalseblock = llvm::BasicBlock::Create(llvm_context(), "if_false", current_context()->current_func());
    auto endifblock = llvm::BasicBlock::Create(llvm_context(), "endif", current_context()->current_func());

    current_builder()->CreateCondBr(cond, iffalseblock, iftrueblock);

    // True branch
    current_builder()->SetInsertPoint(iftrueblock);
    compile_node(node->consequent);
    current_builder()->CreateStore(current_context()->pop_value(), result);
    current_builder()->CreateBr(endifblock);

    // False branch
    current_builder()->SetInsertPoint(iffalseblock);
    compile_node(node->alternative);
    current_builder()->CreateStore(current_context()->pop_value(), result);
    current_builder()->CreateBr(endifblock);

    // End if
    current_builder()->SetInsertPoint(endifblock);
    // Push the result of the if expression to the compiler stack
    current_context()->push_value(current_builder()->CreateLoad(result));
}

void Compiler::compile_var_lookup(const std::shared_ptr<VarLookupNode>& node) {
    if (node->is_global) {
        auto result = current_context()->global_bindings.find(*node->name);

        if (result==current_context()->global_bindings.end()) {
            throw CompilerException("Fatal compiler error: no var",
                    node->sourcePosition);
        }

        auto def = result->second;

        auto var = current_module()->getOrInsertGlobal(def->mangled_name,
                llvm::IntegerType::getInt8PtrTy(llvm_context(),
                        kGCAddressSpace));
        auto v = current_builder()->CreateLoad(var);

        auto val = build_deref_var(v);
        current_context()->push_value(val);
        return;
    }

    auto result = current_context()->lookup_in_local_environment(*node->name);
    if (result!=nullptr) {
        current_context()->push_value(result);
        return;
    }

    throw CompilerException("Unsupported var type", node->sourcePosition);
}

void Compiler::compile_lambda(const std::shared_ptr<LambdaAnalyzerNode>& node) {
    // TODO: This is temporary
    static int cnt = 0;

    auto insert_block = current_builder()->GetInsertBlock();
    auto insert_point = current_builder()->GetInsertPoint();

    std::stringstream ss;
    ss << "lambda_" << cnt;

    std::vector<llvm::Type*> arg_types;

    arg_types.reserve(node->arg_names.size()+1);
    for (int i = 0; i<node->arg_names.size(); ++i) {
        arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace));
    }

    if (node->has_rest_arg) {
        // Add argument for rest args
        arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace));
    }

    // Add extra arg for closure, in order to ger environment values
    arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace));

    auto lambda_type = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace),
            arg_types,
            false);

    auto lambda = llvm::Function::Create(
            lambda_type,
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            ss.str(),
            current_module());

    lambda->setGC("statepoint-example");

    auto entry_block = llvm::BasicBlock::Create(llvm_context(), "entry", lambda);
    current_builder()->SetInsertPoint(entry_block);

    std::unordered_map<std::string, llvm::Value*> local_env;
    int arg_num = 0;
    auto arg_it = lambda->args().begin();
    for (auto& arg_name : node->arg_names) {
        auto& arg = *arg_it;
        arg.setName(*arg_name);

        local_env[*arg_name] = &arg;

        ++arg_num;
        ++arg_it;
    }

    // Add rest arg to env if it exists
    if (node->has_rest_arg) {
        auto& arg = *arg_it;
        arg.setName(*node->rest_arg_name);
        local_env[*node->rest_arg_name] = &arg;

        ++arg_it;
    }

    //++arg_it;
    auto& fn_arg = *arg_it;
    fn_arg.setName("env");

    for (uint64_t i = 0; i<node->closed_overs.size(); i++) {
        local_env[node->closed_overs[i]] = build_lambda_get_env(&fn_arg, i);
    }

    // Push the arguments onto the environment stack so that the compiler
    // can look them up later
    current_context()->push_local_environment(local_env);
    current_context()->push_func(lambda);

    // Compile the body of the function
    compile_node(node->body);
    current_builder()->CreateRet(current_context()->pop_value());

    // Scope ended, pop the arguments from the environment stack
    current_context()->pop_local_environment();
    current_context()->pop_func();

    // Restore back to previous insert point
    current_builder()->SetInsertPoint(insert_block, insert_point);

    auto closure = make_closure(node->arg_names.size(),
            node->has_rest_arg,
            lambda,
            node->closed_overs.size());

    for (uint64_t i = 0; i<node->closed_overs.size(); i++) {
        build_lambda_set_env(closure, i, current_context()->lookup_in_local_environment(node->closed_overs[i]));
    }

    current_context()->push_value(closure);

    ++cnt;
}

void Compiler::compile_def(const std::shared_ptr<DefAnalyzerNode>& node) {
    auto mangled_name = mangle_symbol_name("", *node->name);

    auto glob = new llvm::GlobalVariable(*current_module(),
            llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace),
            false,
            llvm::GlobalValue::LinkageTypes::InternalLinkage,
            nullptr,
            mangled_name);

    glob->setInitializer(
            llvm::ConstantPointerNull::get(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace)));

    auto name_sym = make_symbol(node->name);
    auto v = make_var(name_sym);

    build_gc_add_root(v);

    // Store var in global
    current_builder()->CreateStore(v, glob, false);

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

void Compiler::compile_maybe_invoke(const std::shared_ptr<MaybeInvokeAnalyzerNode>& node) {
    compile_node(node->fn);
    auto fn = current_context()->pop_value();

    auto list_node = std::make_shared<ConstantListAnalyzerNode>();
    list_node->sourcePosition = node->sourcePosition;
    list_node->evaluation_phase = node->evaluation_phase;
    list_node->values = node->args;
    compile_node(list_node);
    auto args = current_context()->pop_value();

    // TODO: Is there a better way to save the args?
    build_gc_add_root(args);
    current_context()->push_value(build_apply(fn, args));
    build_gc_remove_root(args);

//    std::vector<llvm::Value *> args;
//    args.reserve(node->args.size() + 1);
//
//    for (const auto &a: node->args) {
//        compile_node(a);
//        args.push_back(current_context()->pop_value());
//    }
//
//    args.push_back(fn);
//
//    std::vector<llvm::Type *> arg_types;
//    for (uint64_t i = 0; i < node->args.size(); ++i) {
//        arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace));
//    }
//
//    // Extra arg for lambda pointer
//    arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace));
//
//    auto fn_type = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace),
//                                           arg_types,
//                                           false);
//
//    auto fn_ptr = current_builder()->CreateFPCast(build_get_lambda_ptr(fn),
//                                                  llvm::PointerType::get(fn_type, 0));
//
//    auto result = current_builder()->CreateCall(fn_ptr, args);
//
//    current_context()->push_value(result);
}

void Compiler::compile_def_ffi_fn(const std::shared_ptr<electrum::DefFFIFunctionNode>& node) {
    auto insert_block = current_builder()->GetInsertBlock();
    auto insert_point = current_builder()->GetInsertPoint();

    // Create wrapper function with arg conversion and return type conversion

    std::vector<llvm::Type*> arg_types;

    // Add arguments for each argument we are passing the the FFI function
    arg_types.reserve(node->arg_types.size());
    for (int i = 0; i<node->arg_types.size(); i++) {
        arg_types.push_back(llvm::PointerType::getInt8PtrTy(llvm_context(), kGCAddressSpace));
    }

    arg_types.push_back(llvm::PointerType::getInt8PtrTy(llvm_context(), kGCAddressSpace));

    auto ffi_wrapper_type = llvm::FunctionType::get(
            llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace),
            arg_types,
            false);

    auto mangled_name = mangle_symbol_name("", *node->binding);
    auto wrapper_name = mangled_name+"_impl";

    auto ffi_wrapper = llvm::Function::Create(
            ffi_wrapper_type,
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            mangle_symbol_name("", mangled_name),
            current_module());

    ffi_wrapper->setGC("statepoint-example");

    auto entry_block = llvm::BasicBlock::Create(llvm_context(), "entry", ffi_wrapper);
    current_builder()->SetInsertPoint(entry_block);

    arg_types.pop_back();

    auto c_func_type = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace),
            arg_types,
            false);
    auto c_func = current_module()->getOrInsertFunction(*node->func_name, c_func_type);

    std::vector<llvm::Value*> c_args;

    for (auto it = ffi_wrapper->arg_begin(); it!=(ffi_wrapper->arg_end()-1); ++it) {
        auto& arg = *it;
        c_args.push_back(dynamic_cast<llvm::Value*>(&arg));
    }

    auto rv = current_builder()->CreateCall(c_func, c_args);

    current_builder()->CreateRet(rv);

    current_builder()->SetInsertPoint(insert_block, insert_point);


    // Make global symbol

    auto glob = new llvm::GlobalVariable(*current_module(),
            llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace),
            false,
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            nullptr,
            mangled_name);

    glob->setInitializer(llvm::UndefValue::getNullValue(llvm::IntegerType::getInt8PtrTy(llvm_context(),
            kGCAddressSpace)));

    auto name_sym = make_symbol(node->binding);
    auto v = make_var(name_sym);

    build_gc_add_root(v);

    // Store var in global
    current_builder()->CreateStore(v, glob, false);

    // Set initial value for var
    build_set_var(v, make_closure(node->arg_types.size(), false, ffi_wrapper, 0));

    current_context()->push_value(make_nil());

    auto d = std::make_shared<GlobalDef>();
    d->name = *node->binding;
    d->mangled_name = mangled_name;

    current_context()->global_bindings[*node->binding] = d;
}

void Compiler::compile_def_macro(const std::shared_ptr<electrum::DefMacroAnalyzerNode>& node) {
    auto insert_block = current_builder()->GetInsertBlock();
    auto insert_point = current_builder()->GetInsertPoint();

    std::stringstream ss;
    ss << "MX_lambda_" << *node->name;

    std::vector<llvm::Type*> arg_types;

    arg_types.reserve(node->arg_names.size()+1);
    for (int i = 0; i<node->arg_names.size(); ++i) {
        arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace));
    }

    // Add extra arg for closure, in order to ger environment values
    arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace));

    auto expander_type = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace),
            arg_types,
            false);

    auto expander = llvm::Function::Create(
            expander_type,
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            ss.str(),
            current_module());

    expander->setGC("statepoint-example");

    auto entry_block = llvm::BasicBlock::Create(llvm_context(), "entry", expander);
    current_builder()->SetInsertPoint(entry_block);

    std::unordered_map<std::string, llvm::Value*> local_env;
    int arg_num = 0;
    auto arg_it = expander->args().begin();
    for (auto& arg_name : node->arg_names) {
        auto& arg = *arg_it;
        arg.setName(*arg_name);

        local_env[*arg_name] = &arg;

        ++arg_num;
        ++arg_it;
    }

    for (uint64_t i = 0; i<node->closed_overs.size(); i++) {
        auto& arg = *arg_it;
        arg.setName("environment");

        local_env[node->closed_overs[i]] = build_lambda_get_env(&arg, i);
    }

    // Push the arguments onto the environment stack so that the compiler
    // can look them up later
    current_context()->push_local_environment(local_env);
    current_context()->push_func(expander);

    // Compile the body of the expander
    compile_node(node->body);
    current_builder()->CreateRet(current_context()->pop_value());

    // Scope ended, pop the arguments from the environment stack
    current_context()->pop_local_environment();
    current_context()->pop_func();

    // Restore back to previous insert point
    current_builder()->SetInsertPoint(insert_block, insert_point);

    auto closure = make_closure(node->arg_names.size(),
            false,
            expander,
            node->closed_overs.size());

    for (uint64_t i = 0; i<node->closed_overs.size(); i++) {
        build_lambda_set_env(closure, i, current_context()->lookup_in_local_environment(node->closed_overs[i]));
    }


    /* DEF */

    auto mangled_name = mangle_symbol_name("", "MXC_"+*node->name);
    auto glob = new llvm::GlobalVariable(*current_module(),
            llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace),
            false,
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            nullptr,
            mangled_name);

    glob->setInitializer(
            llvm::ConstantPointerNull::get(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace)));

    build_gc_add_root(closure);

    // Store var in global
    current_builder()->CreateStore(closure, glob, false);

    current_context()->push_value(make_nil());

    auto d = std::make_shared<GlobalDef>();
    d->name = *node->name;
    d->mangled_name = mangled_name;

    current_context()->global_macros[*node->name] = d;
    current_context()->push_value(make_nil());
}

void Compiler::compile_macro_expand(const std::shared_ptr<electrum::MacroExpandAnalyzerNode>& node) {
    assert(node->macro->nodeType()==kAnalyzerNodeTypeDefMacro);

    auto macroNode = std::dynamic_pointer_cast<DefMacroAnalyzerNode>(node->macro);

    auto result = current_context()->global_macros.find(*macroNode->name);
    if (result==current_context()->global_macros.end()) {
        throw CompilerException("Unable to find macro expander!", node->sourcePosition);
    }

    auto expanderDef = result->second;
    auto expanderRef = current_module()->getOrInsertGlobal(expanderDef->mangled_name,
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace));
    auto expanderClosure = current_builder()->CreateLoad(expanderRef);

    std::vector<llvm::Value*> args;
    args.reserve(node->args.size()+1);

    for (const auto& a: node->args) {
        compile_node(a);
        args.push_back(current_context()->pop_value());
    }

    args.push_back(expanderClosure);

    std::vector<llvm::Type*> arg_types;
    for (uint64_t i = 0; i<node->args.size(); ++i) {
        arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace));
    }

    arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace));

    auto fn_type = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvm_context(), kGCAddressSpace),
            arg_types,
            false);

    auto fn_ptr = current_builder()->CreateFPCast(build_get_lambda_ptr(expanderClosure),
            llvm::PointerType::get(fn_type, 0));

    auto expander_result = current_builder()->CreateCall(fn_ptr, args);

    current_context()->push_value(expander_result);
}

std::string Compiler::mangle_symbol_name(const std::string ns, const std::string& name) {
    // Ignore ns for now, as we don't support yet
    std::stringstream ss;
    ss << "__elec__" << name << "__";
    return ss.str();
}

#pragma mark - Helpers

llvm::Value* Compiler::make_nil() {
    auto func = current_module()->getOrInsertFunction("rt_make_nil",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace));

    return current_builder()->CreateCall(func);
}

llvm::Value* Compiler::make_integer(int64_t value) {
    // Don't put returned pointer in GC address space, as integers are tagged,
    // and not heap allocated.
    auto func = current_module()->getOrInsertFunction("rt_make_integer",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt64Ty(llvm_context()));

    return current_builder()->CreateCall(func,
            {llvm::ConstantInt::getSigned(llvm::IntegerType::getInt64Ty(llvm_context()),
                    value)});
}

llvm::Value* Compiler::make_float(double value) {
    auto func = current_module()->getOrInsertFunction("rt_make_float",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::Type::getDoubleTy(llvm_context()));

    return current_builder()->CreateCall(func,
            {llvm::ConstantFP::get(llvm::Type::getDoubleTy(llvm_context()), value)});
}

llvm::Value* Compiler::make_boolean(bool value) {
    // Don't put returned pointer in GC address space, as booleans are tagged,
    // and not heap allocated.
    auto func = current_module()->getOrInsertFunction("rt_make_boolean",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8Ty(llvm_context()));

    return current_builder()->CreateCall(func,
            {llvm::ConstantInt::getSigned(llvm::IntegerType::getInt8Ty(llvm_context()),
                    value ? 1 : 0)});
}

llvm::Value* Compiler::make_symbol(std::shared_ptr<std::string> name) {
    auto func = current_module()->getOrInsertFunction("rt_make_symbol",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvm_context(), 0));

    auto strptr = current_builder()->CreateGlobalStringPtr(*name);
    return current_builder()->CreateCall(func, {strptr});
}

llvm::Value* Compiler::make_string(std::shared_ptr<std::string> str) {
    auto func = current_module()->getOrInsertFunction("rt_make_string",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvm_context(), 0));

    auto strptr = current_builder()->CreateGlobalStringPtr(*str);
    return current_builder()->CreateCall(func, {strptr});
}

llvm::Value* Compiler::make_keyword(std::shared_ptr<std::string> name) {
    auto func = current_module()->getOrInsertFunction("rt_make_keyword",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvm_context(), 0));

    auto strptr = current_builder()->CreateGlobalStringPtr(*name);
    return current_builder()->CreateCall(func, {strptr});
}

llvm::Value* Compiler::make_closure(uint64_t arity, bool has_rest_args, llvm::Value* func_ptr, uint64_t env_size) {
    auto func = current_module()->getOrInsertFunction("rt_make_compiled_function",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt32Ty(llvm_context()),
            llvm::IntegerType::getInt32Ty(llvm_context()),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt64Ty(llvm_context()));

    return current_builder()->CreateCall(func,
            {llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(llvm_context()), arity),
             llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(llvm_context()),
                     has_rest_args ? 1 : 0),
             current_builder()->CreatePointerCast(func_ptr,
                     llvm::IntegerType::getInt8PtrTy(
                             llvm_context(),
                             kGCAddressSpace)),
             llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(llvm_context()),
                     env_size)});
}

llvm::Value* Compiler::make_pair(llvm::Value* v, llvm::Value* next) {
    auto func = current_module()->getOrInsertFunction("rt_make_pair",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace));

    return current_builder()->CreateCall(func, {v, next});
}

llvm::Value* Compiler::get_boolean_value(llvm::Value* val) {
    auto func = current_module()->getOrInsertFunction("rt_is_true",
            llvm::IntegerType::getInt8Ty(llvm_context()),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace));

    return current_builder()->CreateCall(func, {val});
}

llvm::Value* Compiler::make_var(llvm::Value* sym) {
    auto func = current_module()->getOrInsertFunction("rt_make_var",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace));

    return current_builder()->CreateCall(func, {sym});
}

void Compiler::build_set_var(llvm::Value* var, llvm::Value* new_val) {
    auto func = current_module()->getOrInsertFunction("rt_set_var",
            llvm::Type::getVoidTy(llvm_context()),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace));

    current_builder()->CreateCall(func, {var, new_val});
}

llvm::Value* Compiler::build_deref_var(llvm::Value* var) {
    auto func = current_module()->getOrInsertFunction("rt_deref_var",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace));

    return current_builder()->CreateCall(func, {var});
}

llvm::Value* Compiler::build_get_lambda_ptr(llvm::Value* fn) {
    auto func = current_module()->getOrInsertFunction("rt_compiled_function_get_ptr",
            llvm::IntegerType::getInt8PtrTy(llvm_context(), 0),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace));
    return current_builder()->CreateCall(func, {fn});
}

llvm::Value* Compiler::build_lambda_set_env(llvm::Value* fn, uint64_t idx, llvm::Value* val) {
    auto func = current_module()->getOrInsertFunction("rt_compiled_function_set_env",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt64Ty(llvm_context()),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace));

    llvm::Value* idx_val = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(llvm_context()), idx);

    return current_builder()->CreateCall(func, {fn, idx_val, val});
}

llvm::Value* Compiler::build_lambda_get_env(llvm::Value* fn, uint64_t idx) {
    auto func = current_module()->getOrInsertFunction("rt_compiled_function_get_env",
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt64Ty(llvm_context()));

    llvm::Value* idx_val = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(llvm_context()), idx);

    return current_builder()->CreateCall(func, {fn, idx_val});
}

llvm::Value* Compiler::build_gc_add_root(llvm::Value* obj) {
    auto func = current_module()->getOrInsertFunction("rt_gc_add_root",
            llvm::Type::getVoidTy(llvm_context()),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace));

    return current_builder()->CreateCall(func, {obj});
}

llvm::Value* Compiler::build_gc_remove_root(llvm::Value* obj) {
    auto func = current_module()->getOrInsertFunction("rt_gc_remove_root",
            llvm::Type::getVoidTy(llvm_context()),
            llvm::IntegerType::getInt8PtrTy(llvm_context(),
                    kGCAddressSpace));

    return current_builder()->CreateCall(func, {obj});
}

llvm::Value* Compiler::build_apply(llvm::Value* f, llvm::Value* args) {
    auto func = current_module()->getOrInsertFunction("rt_apply",
            llvm::Type::getInt8PtrTy(llvm_context(), kGCAddressSpace),
            llvm::Type::getInt8PtrTy(llvm_context(), kGCAddressSpace),
            llvm::Type::getInt8PtrTy(llvm_context(), kGCAddressSpace));

    return current_builder()->CreateCall(func, {f, args});
}

} // namespace electrum
