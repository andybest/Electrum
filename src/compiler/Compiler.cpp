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

#include <boost/filesystem.hpp>
#include <utility>

#include <llvm/Support/raw_ostream.h>
#include <llvm/CodeGen/GCStrategy.h>
#include <llvm/CodeGen/BuiltinGCs.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>

namespace electrum {

#pragma mark - Compiler

Compiler::Compiler() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    llvm::linkAllBuiltinGCs();
    jit_ = std::make_shared<ElectrumJit>(es_);
}

void* Compiler::compileAndEvalString(const std::string& str) {
    static int cnt = 0;
    auto temp_path = boost::filesystem::temp_directory_path();
    std::stringstream ss;
    ss << "repl_" << cnt << ".el";
    temp_path /= ss.str();

    ++cnt;

    // Write the contents of the repl to a temporary file, so that the debugger can find it
    auto temp_file = fopen(temp_path.string().c_str(), "w");
    fputs(str.c_str(), temp_file);
    fflush(temp_file);
    fclose(temp_file);

    auto fname = temp_path.filename();
    temp_path.remove_filename();

    Parser p;
    auto ast = p.readString(str, temp_path.string());

    currentContext()->pushNewState("jit_module", temp_path.string()+"/", fname.string());
    createGCEntry();

    // Analyze as a top level form
    auto node = analyzer_.analyze(ast, 0);
    auto toplevel_forms = analyzer_.collapseTopLevelForms(node);
    void* rv = NIL_PTR;

    // Run each top level initializer
    for (const auto& f: toplevel_forms) {
        auto tl_def = compileTopLevelNode(f);
        rv = runInitializerWithJit(tl_def);
    }

    rt_gc_add_root(rv);

    return rv;
}

TopLevelInitializerDef Compiler::compileTopLevelNode(std::shared_ptr<AnalyzerNode> node) {
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
            llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace), false),
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            mangled_name,
            currentContext()->currentModule());

    mainfunc->setGC("statepoint-example");

    /* Function Debug Info */
    /*
    llvm::DIScope* f_ctx = currentContext()->currentDebugInfo()->currentScope();
    auto subprogram = currentContext()->currentDIBuilder()->createFunction(
            f_ctx,
            ss.str(),
            llvm::StringRef(),
            f_ctx->getFile(),
            node->sourcePosition->line,
            this->createFunctionDebugType(0),
            node->sourcePosition->line,
            llvm::DINode::FlagPrototyped);

    mainfunc->setSubprogram(subprogram);l
    currentContext()->currentDebugInfo()->lexical_blocks.push_back(subprogram);
//    currentContext()->emitLocation(node->sourcePosition);
     */

    currentContext()->pushFunc(mainfunc);

    auto entry = llvm::BasicBlock::Create(llvmContext(), "entry", mainfunc);

    currentBuilder()->SetInsertPoint(entry);
    compileNode(std::move(node));

    // Return result.
    auto result = currentContext()->popValue();

    // TODO: Figure out what to do with return values rather than adding as a root
    //buildGcAddRoot(result);

    currentBuilder()->CreateRet(result);
    currentContext()->popFunc();

    currentContext()->currentDebugInfo()->lexical_blocks.pop_back();

    return initializer;
}

void* Compiler::runInitializerWithJit(TopLevelInitializerDef& tl_def) {
    static int cnt = 0;

    if (currentModule()->getFunction(tl_def.mangled_name)!=nullptr) {
        jit_->addModule(currentContext()->popState());

        auto stackmap_ptr = jit_->getStackMapPointer();
        if (stackmap_ptr!=nullptr) {
            rt_gc_init_stackmap(stackmap_ptr);
        }

        std::stringstream ss;
        ss << "jit_module__" << cnt;
        currentContext()->pushNewState(ss.str(), "/tmp", "tl.el");
        createGCEntry();
    }

    auto f_addr = jit_->getSymbolAddress(tl_def.mangled_name);
    typedef void* (* InitFunc)();
    auto f_ptr = reinterpret_cast<InitFunc>(f_addr);
    return f_ptr();
}

void* Compiler::compileAndEvalExpander(std::shared_ptr<MacroExpandAnalyzerNode> node) {
    static int cnt = 0;

    std::stringstream moduless;
    moduless << "expander_module_" << cnt;

    currentContext()->pushNewState(moduless.str(), "/tmp", "expander.el");
    createGCEntry();

    std::stringstream ss;
    ss << "expansion_func_" << cnt;

    auto mainfunc = llvm::Function::Create(
            llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace), false),
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            ss.str(),
            currentContext()->currentModule());

    mainfunc->setGC("statepoint-example");

    /* Function Debug Info */
    /*
    llvm::DIScope* f_ctx = currentContext()->currentDebugInfo()->currentScope();
    auto subprogram = currentContext()->currentDIBuilder()->createFunction(
            f_ctx,
            ss.str(),
            llvm::StringRef(),
            f_ctx->getFile(),
            node->sourcePosition->line,
            this->createFunctionDebugType(0),
            node->sourcePosition->line,
            llvm::DINode::FlagPrototyped);

    mainfunc->setSubprogram(subprogram);
    currentContext()->currentDebugInfo()->lexical_blocks.push_back(subprogram);
//    currentContext()->emitLocation(node->sourcePosition);
*/
    currentContext()->pushFunc(mainfunc);

    auto entry = llvm::BasicBlock::Create(llvmContext(), "entry", mainfunc);

    currentBuilder()->SetInsertPoint(entry);
    compileMacroExpand(node);

    // Return result.
    auto result = currentContext()->popValue();

    // TODO: Figure out what to do with return values rather than adding as a root
    buildGcAddRoot(result);

    currentBuilder()->CreateRet(result);
    currentContext()->popFunc();

    currentContext()->currentDebugInfo()->lexical_blocks.pop_back();

    jit_->addModule(currentContext()->popState());

    auto faddr = jit_->getSymbolAddress(ss.str());
    rt_gc_init_stackmap(jit_->getStackMapPointer());

    typedef void* (* MainPtr)();

    auto fp = reinterpret_cast<MainPtr>(faddr);
    auto rv = fp();

    ++cnt;

    return rv;
}

void Compiler::createGCEntry() {
    auto gc_type = llvm::FunctionType::get(llvm::Type::getVoidTy(currentContext()->llvmContext()), false);
    auto gcfunc = llvm::dyn_cast<llvm::Function>(
            currentContext()->currentModule()->getOrInsertFunction("gc.safepoint_poll", gc_type));

    gcfunc->addFnAttr(llvm::Attribute::NoUnwind);

    auto gc_entry = llvm::BasicBlock::Create(currentContext()->llvmContext(), "entry", gcfunc);
    llvm::IRBuilder<> b(currentContext()->llvmContext());
    b.SetInsertPoint(gc_entry);

    auto dogc = currentContext()->currentModule()->getOrInsertFunction("rt_enter_gc",
            llvm::Type::getVoidTy(currentContext()
                    ->llvmContext()));
    b.CreateCall(dogc);
    b.CreateRet(nullptr);
}

void Compiler::compileNode(std::shared_ptr<AnalyzerNode> node) {
    //currentContext()->emitLocation(node->sourcePosition);

    switch (node->nodeType()) {
    case kAnalyzerNodeTypeConstant:compileConstant(std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeConstantList:compileConstantList(std::dynamic_pointer_cast<ConstantListAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeLambda:compileLambda(std::dynamic_pointer_cast<LambdaAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeDo:compileDo(std::dynamic_pointer_cast<DoAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeIf:compileIf(std::dynamic_pointer_cast<IfAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeDef:compileDef(std::dynamic_pointer_cast<DefAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeVarLookup:compileVarLookup(std::dynamic_pointer_cast<VarLookupNode>(node));
        break;
    case kAnalyzerNodeTypeMaybeInvoke:compileMaybeInvoke(std::dynamic_pointer_cast<MaybeInvokeAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeDefFFIFunction:compileDefFFIFn(std::dynamic_pointer_cast<DefFFIFunctionNode>(node));
        break;
    case kAnalyzerNodeTypeDefMacro:compileDefMacro(std::dynamic_pointer_cast<DefMacroAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeMacroExpand: {
        auto macro_expand_node = std::dynamic_pointer_cast<MacroExpandAnalyzerNode>(node);
        if (!macro_expand_node->do_evaluate) {
            compileMacroExpand(macro_expand_node);
        }
        else {
            auto expansion = compileAndEvalExpander(macro_expand_node);
            Parser p;
            auto form = p.readLispValue(expansion, node->sourcePosition);
            auto expanded_node = analyzer_.analyze(form, node->node_depth, node->evaluation_phase);
            compileNode(expanded_node);
        }
        break;
    }
    case kAnalyzerNodeTypeTry: compileTry(std::dynamic_pointer_cast<TryAnalyzerNode>(node));
        break;
    case kAnalyzerNodeTypeThrow: compileThrow(std::dynamic_pointer_cast<ThrowAnalyzerNode>(node));
        break;

    default:throw CompilerException("Unrecognized node type", node->sourcePosition);
    }
}

void Compiler::compileConstant(std::shared_ptr<ConstantValueAnalyzerNode> node) {
    llvm::Value* v;

    switch (node->type) {
    case kAnalyzerConstantTypeInteger:v = makeInteger(boost::get<int64_t>(node->value));
        break;
    case kAnalyzerConstantTypeFloat:v = makeFloat(boost::get<double>(node->value));
        break;
    case kAnalyzerConstantTypeBoolean:v = makeBoolean(boost::get<bool>(node->value));
        break;
    case kAnalyzerConstantTypeSymbol:v = makeSymbol(boost::get<shared_ptr<std::string>>(node->value));
        break;
    case kAnalyzerConstantTypeString:v = makeString(boost::get<shared_ptr<std::string>>(node->value));
        break;
    case kAnalyzerConstantTypeNil:v = makeNil();
        break;
    default:throw CompilerException("Unrecognized constant type", node->sourcePosition);
    }

    currentContext()->pushValue(v);
}

void Compiler::compileConstantList(const std::shared_ptr<ConstantListAnalyzerNode>& node) {
    // Special case- the empty list is nil
    if (node->values.empty()) {
        currentContext()->pushValue(makeNil());
        return;
    }

    llvm::Value* head = makeNil();

    for (auto it = node->values.rbegin(); it!=node->values.rend(); ++it) {
        compileNode(*it);
        head = makePair(currentContext()->popValue(), head);
    }

    currentContext()->pushValue(head);
}

void Compiler::compileDo(const std::shared_ptr<DoAnalyzerNode>& node) {
    // Compile each node in the body, disregarding the result
    for (const auto& child: node->statements) {
        compileNode(child);
        currentContext()->popValue();
    }

    // Compile the last node, keeping the result on the stack
    compileNode(node->returnValue);
}

void Compiler::compileIf(const std::shared_ptr<IfAnalyzerNode>& node) {
    // Compile the condition to the stack
    compileNode(node->condition);

    // Create stack variable to hold result
    // TODO: Make sure this doesn't need to be in the GC Address space
    auto result = currentBuilder()->CreateAlloca(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            0, nullptr, "if_result");

    auto cond = currentBuilder()->CreateICmpEQ(getBooleanValue(currentContext()->popValue()),
            llvm::ConstantInt::get(llvm::IntegerType::getInt8Ty(llvmContext()),
                    0));

    auto iftrueblock = llvm::BasicBlock::Create(llvmContext(), "if_true", currentContext()->currentFunc());
    auto iffalseblock = llvm::BasicBlock::Create(llvmContext(), "if_false", currentContext()->currentFunc());
    auto endifblock = llvm::BasicBlock::Create(llvmContext(), "endif", currentContext()->currentFunc());

    currentBuilder()->CreateCondBr(cond, iffalseblock, iftrueblock);

    // True branch
    currentBuilder()->SetInsertPoint(iftrueblock);
    compileNode(node->consequent);
    currentBuilder()->CreateStore(currentContext()->popValue(), result);
    currentBuilder()->CreateBr(endifblock);

    // False branch
    currentBuilder()->SetInsertPoint(iffalseblock);
    compileNode(node->alternative);
    currentBuilder()->CreateStore(currentContext()->popValue(), result);
    currentBuilder()->CreateBr(endifblock);

    // End if
    currentBuilder()->SetInsertPoint(endifblock);
    // Push the result of the if expression to the compiler stack
    currentContext()->pushValue(currentBuilder()->CreateLoad(result));
}

void Compiler::compileVarLookup(const std::shared_ptr<VarLookupNode>& node) {
    if (node->is_global) {
        auto result = currentContext()->global_bindings.find(*node->name);

        if (result==currentContext()->global_bindings.end()) {
            throw CompilerException("Fatal compiler error: no var",
                    node->sourcePosition);
        }

        auto def = result->second;

        auto var = currentModule()->getOrInsertGlobal(def->mangled_name,
                llvm::IntegerType::getInt8PtrTy(llvmContext(),
                        kGCAddressSpace));
        auto v = currentBuilder()->CreateLoad(var);

        auto val = buildDerefVar(v);
        currentContext()->pushValue(val);
        return;
    }

    auto result = currentContext()->lookupInLocalEnvironment(*node->name);
    if (result!=nullptr) {
        currentContext()->pushValue(result);
        return;
    }

    throw CompilerException("Unsupported var type", node->sourcePosition);
}

void Compiler::compileLambda(const std::shared_ptr<LambdaAnalyzerNode>& node) {
    // TODO: This is temporary
    static int cnt = 0;

    auto insert_block = currentBuilder()->GetInsertBlock();
    auto insert_point = currentBuilder()->GetInsertPoint();

    std::stringstream ss;
    ss << "lambda_" << cnt;

    std::vector<llvm::Type*> arg_types;

    arg_types.reserve(node->arg_names.size()+1);
    for (int i = 0; i<node->arg_names.size(); ++i) {
        arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));
    }

    if (node->has_rest_arg) {
        // Add argument for rest args
        arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));
    }

    // Add extra arg for closure, in order to ger environment values
    arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));

    auto lambda_type = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            arg_types,
            false);

    auto lambda = llvm::Function::Create(
            lambda_type,
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            ss.str(),
            currentModule());

    lambda->setGC("statepoint-example");

    /* Function Debug Info */
    /*
    llvm::DIScope* f_ctx = currentContext()->currentDebugInfo()->currentScope();
     auto subprogram = currentContext()->currentDIBuilder()->createFunction(
            f_ctx,
            ss.str(),
            llvm::StringRef(),
            f_ctx->getFile(),
            node->sourcePosition->line,
            this->createFunctionDebugType(node->arg_name_nodes.size()+(node->has_rest_arg ? 1 : 0)),
            node->sourcePosition->line,
            llvm::DINode::FlagPrototyped);
    lambda->setSubprogram(subprogram);
*/
//    std::cout << subprogram->getFilename().str() << std::endl;
//    std::cout << subprogram->getDirectory().str() << std::endl;

//    currentContext()->currentDebugInfo()->lexical_blocks.push_back(subprogram);
//    currentContext()->emitLocation(node->sourcePosition);

    auto entry_block = llvm::BasicBlock::Create(llvmContext(), "entry", lambda);
    currentBuilder()->SetInsertPoint(entry_block);

    currentContext()->pushScope();

    std::unordered_map<std::string, llvm::Value*> local_env;
    int arg_num = 0;
    auto arg_it = lambda->args().begin();
    for (auto& arg_name : node->arg_names) {
        auto& arg = *arg_it;
        arg.setName(*arg_name);

        auto arg_pos = node->arg_name_nodes[arg_num]->sourcePosition;

        local_env[*arg_name] = &arg;

        /*auto d = currentContext()->currentDIBuilder()->createParameterVariable(
                currentContext()->currentDebugInfo()->currentScope(),
                *arg_name,
                arg_num,
                f_ctx->getFile(),
                node->arg_name_nodes[arg_num]->sourcePosition->line,
                currentContext()->currentDebugInfo()->void_ptr_type,
                true);

        currentContext()->currentDIBuilder()->insertDeclare(
                &arg,
                d,
                currentContext()->currentDIBuilder()->createExpression(),
                llvm::DebugLoc::get(arg_pos->line,
                        arg_pos->column,
                        currentContext()->currentDebugInfo()->currentScope()),
                currentBuilder()->GetInsertBlock());
*/
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
        local_env[node->closed_overs[i]] = buildLambdaGetEnv(&fn_arg, i);
    }

    // Push the arguments onto the environment stack so that the compiler
    // can look them up later
    currentContext()->pushLocalEnvironment(local_env);
    currentContext()->pushFunc(lambda);

    // Compile the body of the function
    compileNode(node->body);
    currentBuilder()->CreateRet(currentContext()->popValue());

//    currentContext()->currentDebugInfo()->lexical_blocks.pop_back();
//    currentContext()->currentDIBuilder()->finalizeSubprogram(subprogram);

    // Scope ended, pop the arguments from the environment stack
    currentContext()->popLocalEnvironment();
    currentContext()->popFunc();

    // Restore back to previous insert point
    currentBuilder()->SetInsertPoint(insert_block, insert_point);

    currentContext()->popScope();

    // Emit location, as the following will be called from the parent scope
    currentContext()->emitLocation(node->sourcePosition);

    auto closure = makeClosure(node->arg_names.size(),
            node->has_rest_arg,
            lambda,
            node->closed_overs.size());

    for (uint64_t i = 0; i<node->closed_overs.size(); i++) {
        buildLambdaSetEnv(closure, i, currentContext()->lookupInLocalEnvironment(node->closed_overs[i]));
    }

    currentContext()->pushValue(closure);

    ++cnt;
}

void Compiler::compileDef(const std::shared_ptr<DefAnalyzerNode>& node) {
    auto mangled_name = mangleSymbolName("", *node->name);

    auto glob = new llvm::GlobalVariable(*currentModule(),
            llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            false,
            llvm::GlobalValue::LinkageTypes::InternalLinkage,
            nullptr,
            mangled_name);

    glob->setInitializer(
            llvm::ConstantPointerNull::get(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace)));

    auto name_sym = makeSymbol(node->name);
    auto v = makeVar(name_sym);

    buildGcAddRoot(v);

    // Store var in global
    currentBuilder()->CreateStore(v, glob, false);

    // Compile value
    compileNode(node->value);

    // Set initial value for var
    buildSetVar(v, currentContext()->popValue());

    currentContext()->pushValue(makeNil());

    auto d = std::make_shared<GlobalDef>();
    d->name = *node->name;
    d->mangled_name = mangled_name;

    currentContext()->global_bindings[*node->name] = d;
}

void Compiler::compileMaybeInvoke(const std::shared_ptr<MaybeInvokeAnalyzerNode>& node) {
    compileNode(node->fn);
    auto fn = currentContext()->popValue();

    auto list_node = std::make_shared<ConstantListAnalyzerNode>();
    list_node->sourcePosition = node->sourcePosition;
    list_node->evaluation_phase = node->evaluation_phase;
    list_node->values = node->args;
    compileNode(list_node);
    auto args = currentContext()->popValue();

    // TODO: Is there a better way to save the args?
    buildGcAddRoot(args);

    auto eh_info = currentContext()->currentScope()->currentEHInfo();
    if (eh_info!=nullptr) {
        currentContext()->pushValue(buildApplyInvoke(fn, args, eh_info));
    }
    else {
        currentContext()->pushValue(buildApply(fn, args));
    }
    buildGcRemoveRoot(args);

//    std::vector<llvm::Value *> args;
//    args.reserve(node->args.size() + 1);
//
//    for (const auto &a: node->args) {
//        compileNode(a);
//        args.push_back(currentContext()->popValue());
//    }
//
//    args.push_back(fn);
//
//    std::vector<llvm::Type *> arg_types;
//    for (uint64_t i = 0; i < node->args.size(); ++i) {
//        arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));
//    }
//
//    // Extra arg for lambda pointer
//    arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));
//
//    auto fn_type = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
//                                           arg_types,
//                                           false);
//
//    auto fn_ptr = currentBuilder()->CreateFPCast(buildGetLambdaPtr(fn),
//                                                  llvm::PointerType::get(fn_type, 0));
//
//    auto result = currentBuilder()->CreateCall(fn_ptr, args);
//
//    currentContext()->pushValue(result);
}

void Compiler::compileDefFFIFn(const std::shared_ptr<electrum::DefFFIFunctionNode>& node) {
    auto insert_block = currentBuilder()->GetInsertBlock();
    auto insert_point = currentBuilder()->GetInsertPoint();

    // Create wrapper function with arg conversion and return type conversion

    std::vector<llvm::Type*> arg_types;

    // Add arguments for each argument we are passing the the FFI function
    arg_types.reserve(node->arg_types.size());
    for (int i = 0; i<node->arg_types.size(); i++) {
        arg_types.push_back(llvm::PointerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));
    }

    arg_types.push_back(llvm::PointerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));

    auto ffi_wrapper_type = llvm::FunctionType::get(
            llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            arg_types,
            false);

    auto mangled_name = mangleSymbolName("", *node->binding);
    auto wrapper_name = mangled_name+"_impl";

    auto ffi_wrapper = llvm::Function::Create(
            ffi_wrapper_type,
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            mangleSymbolName("", mangled_name),
            currentModule());

    ffi_wrapper->setGC("statepoint-example");

    auto entry_block = llvm::BasicBlock::Create(llvmContext(), "entry", ffi_wrapper);
    currentBuilder()->SetInsertPoint(entry_block);

    arg_types.pop_back();

    auto c_func_type = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            arg_types,
            false);
    auto c_func = currentModule()->getOrInsertFunction(*node->func_name, c_func_type);

    std::vector<llvm::Value*> c_args;

    for (auto it = ffi_wrapper->arg_begin(); it!=(ffi_wrapper->arg_end()-1); ++it) {
        auto& arg = *it;
        c_args.push_back(dynamic_cast<llvm::Value*>(&arg));
    }

    auto rv = currentBuilder()->CreateCall(c_func, c_args);

    currentBuilder()->CreateRet(rv);

    currentBuilder()->SetInsertPoint(insert_block, insert_point);


    // Make global symbol

    auto glob = new llvm::GlobalVariable(*currentModule(),
            llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            false,
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            nullptr,
            mangled_name);

    glob->setInitializer(llvm::UndefValue::getNullValue(llvm::IntegerType::getInt8PtrTy(llvmContext(),
            kGCAddressSpace)));

    auto name_sym = makeSymbol(node->binding);
    auto v = makeVar(name_sym);

    buildGcAddRoot(v);

    // Store var in global
    currentBuilder()->CreateStore(v, glob, false);

    // Set initial value for var
    buildSetVar(v, makeClosure(node->arg_types.size(), false, ffi_wrapper, 0));

    currentContext()->pushValue(makeNil());

    auto d = std::make_shared<GlobalDef>();
    d->name = *node->binding;
    d->mangled_name = mangled_name;

    currentContext()->global_bindings[*node->binding] = d;
}

void Compiler::compileDefMacro(const std::shared_ptr<electrum::DefMacroAnalyzerNode>& node) {
    auto insert_block = currentBuilder()->GetInsertBlock();
    auto insert_point = currentBuilder()->GetInsertPoint();

    std::stringstream ss;
    ss << "MX_lambda_" << *node->name;

    std::vector<llvm::Type*> arg_types;

    arg_types.reserve(node->arg_names.size()+1);
    for (int i = 0; i<node->arg_names.size(); ++i) {
        arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));
    }

    // Add extra arg for closure, in order to ger environment values
    arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));

    auto expander_type = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            arg_types,
            false);

    auto expander = llvm::Function::Create(
            expander_type,
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            ss.str(),
            currentModule());

    expander->setGC("statepoint-example");

    /* Function Debug Info */
    /*
    llvm::DIScope* f_ctx = currentContext()->currentDebugInfo()->currentScope();
    auto subprogram = currentContext()->currentDIBuilder()->createFunction(
            f_ctx,
            ss.str(),
            llvm::StringRef(),
            f_ctx->getFile(),
            node->sourcePosition->line,
            this->createFunctionDebugType(node->arg_name_nodes.size()),
            node->sourcePosition->line,
            llvm::DINode::FlagPrototyped);
    expander->setSubprogram(subprogram);

    currentContext()->currentDebugInfo()->lexical_blocks.push_back(subprogram);
//    currentContext()->emitLocation(node->sourcePosition);
*/
    currentContext()->pushScope();

    auto entry_block = llvm::BasicBlock::Create(llvmContext(), "entry", expander);
    currentBuilder()->SetInsertPoint(entry_block);

    std::unordered_map<std::string, llvm::Value*> local_env;
    int arg_num = 0;
    auto arg_it = expander->args().begin();
    for (auto& arg_name : node->arg_names) {
        auto& arg = *arg_it;
        arg.setName(*arg_name);

        local_env[*arg_name] = &arg;

        auto arg_pos = node->arg_name_nodes[arg_num]->sourcePosition;

        /*
        auto d = currentContext()->currentDIBuilder()->createParameterVariable(
                currentContext()->currentDebugInfo()->currentScope(),
                *arg_name,
                arg_num,
                f_ctx->getFile(),
                node->arg_name_nodes[arg_num]->sourcePosition->line,
                currentContext()->currentDebugInfo()->void_ptr_type,
                true);

        currentContext()->currentDIBuilder()->insertDeclare(
                &arg,
                d,
                currentContext()->currentDIBuilder()->createExpression(),
                llvm::DebugLoc::get(arg_pos->line,
                        arg_pos->column,
                        currentContext()->currentDebugInfo()->currentScope()),
                currentBuilder()->GetInsertBlock());
*/
        ++arg_num;
        ++arg_it;
    }

    for (uint64_t i = 0; i<node->closed_overs.size(); i++) {
        auto& arg = *arg_it;
        arg.setName("environment");

        local_env[node->closed_overs[i]] = buildLambdaGetEnv(&arg, i);
    }

    // Push the arguments onto the environment stack so that the compiler
    // can look them up later
    currentContext()->pushLocalEnvironment(local_env);
    currentContext()->pushFunc(expander);

    // Compile the body of the expander
    compileNode(node->body);
    currentBuilder()->CreateRet(currentContext()->popValue());

    // Scope ended, pop the arguments from the environment stack
    currentContext()->popLocalEnvironment();
    currentContext()->popFunc();

    // Restore back to previous insert point
    currentBuilder()->SetInsertPoint(insert_block, insert_point);

    currentContext()->popScope();

    currentContext()->currentDebugInfo()->lexical_blocks.pop_back();
//    currentContext()->emitLocation(node->sourcePosition);

    auto closure = makeClosure(node->arg_names.size(),
            false,
            expander,
            node->closed_overs.size());

    for (uint64_t i = 0; i<node->closed_overs.size(); i++) {
        buildLambdaSetEnv(closure, i, currentContext()->lookupInLocalEnvironment(node->closed_overs[i]));
    }


    /* DEF */

    auto mangled_name = mangleSymbolName("", "MXC_"+*node->name);
    auto glob = new llvm::GlobalVariable(*currentModule(),
            llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            false,
            llvm::GlobalValue::LinkageTypes::ExternalLinkage,
            nullptr,
            mangled_name);

    glob->setInitializer(
            llvm::ConstantPointerNull::get(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace)));

    buildGcAddRoot(closure);

    // Store var in global
    currentBuilder()->CreateStore(closure, glob, false);

    currentContext()->pushValue(makeNil());

    auto d = std::make_shared<GlobalDef>();
    d->name = *node->name;
    d->mangled_name = mangled_name;

    currentContext()->global_macros[*node->name] = d;
    currentContext()->pushValue(makeNil());
}

void Compiler::compileMacroExpand(const std::shared_ptr<electrum::MacroExpandAnalyzerNode>& node) {
    assert(node->macro->nodeType()==kAnalyzerNodeTypeDefMacro);

    auto macroNode = std::dynamic_pointer_cast<DefMacroAnalyzerNode>(node->macro);

    auto result = currentContext()->global_macros.find(*macroNode->name);
    if (result==currentContext()->global_macros.end()) {
        throw CompilerException("Unable to find macro expander!", node->sourcePosition);
    }

    auto expanderDef = result->second;
    auto expanderRef = currentModule()->getOrInsertGlobal(expanderDef->mangled_name,
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace));
    auto expanderClosure = currentBuilder()->CreateLoad(expanderRef);

    std::vector<llvm::Value*> args;
    args.reserve(node->args.size()+1);

    for (const auto& a: node->args) {
        compileNode(a);
        args.push_back(currentContext()->popValue());
    }

    args.push_back(expanderClosure);

    std::vector<llvm::Type*> arg_types;
    for (uint64_t i = 0; i<node->args.size(); ++i) {
        arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));
    }

    arg_types.push_back(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));

    auto fn_type = llvm::FunctionType::get(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            arg_types,
            false);

    auto fn_ptr = currentBuilder()->CreateFPCast(buildGetLambdaPtr(expanderClosure),
            llvm::PointerType::get(fn_type, 0));

    auto expander_result = currentBuilder()->CreateCall(fn_ptr, args);

    currentContext()->pushValue(expander_result);
}

void Compiler::compileTry(const shared_ptr<TryAnalyzerNode> node) {
    if (!currentContext()->currentFunc()->hasPersonalityFn()) {
        auto personality_type = llvm::FunctionType::get(
                llvm::IntegerType::getInt32Ty(llvmContext()),
                {llvm::IntegerType::getInt32Ty(llvmContext()),
                 llvm::IntegerType::getInt32Ty(llvmContext()),
                 llvm::IntegerType::getInt64Ty(llvmContext()),
                 llvm::IntegerType::getInt64Ty(llvmContext()),
                 llvm::IntegerType::getInt8PtrTy(llvmContext(), 0),
                 llvm::IntegerType::getInt8PtrTy(llvmContext(), 0)},
                false);

        auto p_fn = currentModule()->getOrInsertFunction("el_rt_eh_personality", personality_type);
        currentContext()->currentFunc()->setPersonalityFn(p_fn);
    }

    // Create a value on the stack that will be returned either from a try or catch block
    auto rv = currentBuilder()->CreateAlloca(llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            0, nullptr, "exc_rv");

    // Block that will contain all of the landing pads
    auto catch_block = llvm::BasicBlock::Create(llvmContext(), "catch", currentContext()->currentFunc());

    // Block that will be jumped to at the end of the try/catch blocks
    auto done_block = llvm::BasicBlock::Create(llvmContext(), "try_done", currentContext()->currentFunc());

    // Set up EHCompileInfo so that any calls compiled in the try block know to use 'invoke' rather
    // than 'call', and where to unwind to.
    auto eh_info = make_shared<EHCompileInfo>();
    eh_info->catch_dest = catch_block;
    currentContext()->currentScope()->pushEHInfo(eh_info);

    // Compile try block

    for (int i = 0; i<node->body.size(); i++) {
        compileNode(node->body[i]);

        if (i<node->body.size()-1) {
            currentContext()->popValue();
        }
    }

    currentBuilder()->CreateStore(currentContext()->popValue(), rv);
    currentBuilder()->CreateBr(done_block);
    currentContext()->currentScope()->popEHInfo();

    // Compile catch blocks
    currentBuilder()->SetInsertPoint(catch_block);

    auto lp_type = llvm::StructType::get(llvmContext(),
            {llvm::IntegerType::getInt8PtrTy(llvmContext(), 0)},
            false);
    auto landing_pad = currentBuilder()->CreateLandingPad(lp_type, node->catch_nodes.size());
    auto exception_type = currentBuilder()->CreateExtractValue(landing_pad, 0);

    auto e_eq = currentModule()->getOrInsertFunction("el_rt_exception_matches",
            llvm::IntegerType::getInt64Ty(llvmContext()),
            llvm::IntegerType::getInt8PtrTy(llvmContext()),
            llvm::IntegerType::getInt8PtrTy(llvmContext()));

    // Create basic blocks for each catch node
    vector<llvm::BasicBlock*> catch_blocks;
    for (auto n: node->catch_nodes) {
        auto bblock = llvm::BasicBlock::Create(llvmContext(),
                "catch_handler",
                currentContext()->currentFunc());

        catch_blocks.push_back(bblock);
        auto exc_type = currentBuilder()->CreateGlobalStringPtr(*n->exception_type);
        landing_pad->addClause(exc_type);

        // Check if the current catch block matches
        auto cond = currentBuilder()->CreateICmpEQ(
                currentBuilder()->CreateCall(e_eq,
                        {exception_type,
                        exc_type}),
                llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(llvmContext()), 1));

        auto nextblock = llvm::BasicBlock::Create(llvmContext(), "catch_cont", currentContext()->currentFunc());
        currentBuilder()->CreateCondBr(cond, bblock, nextblock);

        currentBuilder()->SetInsertPoint(nextblock);
    }

    currentBuilder()->CreateUnreachable();

    for (int i = 0; i<node->catch_nodes.size(); i++) {
        currentBuilder()->SetInsertPoint(catch_blocks[i]);

        auto catch_node = node->catch_nodes[i];

        std::unordered_map<std::string, llvm::Value*> local_env;

//        local_env[*catch_node->exception_binding] = landing_pad;
        currentContext()->pushLocalEnvironment(local_env);

        for (const auto& body_node: catch_node->body) {
            compileNode(body_node);
            if (body_node!=catch_node->body.back()) {
                currentContext()->popValue();
            }
        }

        currentBuilder()->CreateStore(currentContext()->popValue(), rv);
        currentBuilder()->CreateBr(done_block);
        currentContext()->popLocalEnvironment();
    }

    currentBuilder()->SetInsertPoint(done_block);
    currentContext()->pushValue(currentBuilder()->CreateLoad(rv));
}

void Compiler::compileThrow(const std::shared_ptr<electrum::ThrowAnalyzerNode> node) {
    auto alloc_func = currentModule()->getOrInsertFunction(
            "el_rt_allocate_exception",
            llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvmContext(), 0),
            llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));

    std::stringstream ss;
    ss << "el_exc_" << *node->exception_type;

    compileNode(node->metadata);
    auto meta = currentContext()->popValue();

    auto exc_type = currentBuilder()->CreateGlobalStringPtr(*node->exception_type,
            ss.str());
    auto exc = currentBuilder()->CreateCall(alloc_func,
            {exc_type,
             meta});

    auto throw_func = currentModule()->getOrInsertFunction(
            "el_rt_throw",
            llvm::Type::getVoidTy(llvmContext()),
            llvm::IntegerType::getInt8PtrTy(llvmContext(), kGCAddressSpace));

    auto unreachable_dest = llvm::BasicBlock::Create(llvmContext(), "throw_unreachable",
            currentContext()->currentFunc());
    auto eh_info = currentContext()->currentScope()->currentEHInfo();

    if (eh_info!=nullptr) {
        currentBuilder()->CreateInvoke(throw_func, unreachable_dest, eh_info->catch_dest, {exc});
    }
    else {
        currentBuilder()->CreateCall(throw_func, {exc});
    }

    currentBuilder()->SetInsertPoint(unreachable_dest);
    currentContext()->pushValue(makeNil());
}

#pragma mark - Helpers

std::string Compiler::mangleSymbolName(const std::string ns, const std::string& name) {
    // Ignore ns for now, as we don't support yet
    std::stringstream ss;
    ss << "__elec__" << name << "__";
    return ss.str();
}

llvm::Value* Compiler::makeNil() {
    auto func = currentModule()->getOrInsertFunction("rt_make_nil",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace));

    return currentBuilder()->CreateCall(func);
}

llvm::Value* Compiler::makeInteger(int64_t value) {
    // Don't put returned pointer in GC address space, as integers are tagged,
    // and not heap allocated.
    auto func = currentModule()->getOrInsertFunction("rt_make_integer",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt64Ty(llvmContext()));

    return currentBuilder()->CreateCall(func,
            {llvm::ConstantInt::getSigned(llvm::IntegerType::getInt64Ty(llvmContext()),
                    value)});
}

llvm::Value* Compiler::makeFloat(double value) {
    auto func = currentModule()->getOrInsertFunction("rt_make_float",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::Type::getDoubleTy(llvmContext()));

    return currentBuilder()->CreateCall(func,
            {llvm::ConstantFP::get(llvm::Type::getDoubleTy(llvmContext()), value)});
}

llvm::Value* Compiler::makeBoolean(bool value) {
    // Don't put returned pointer in GC address space, as booleans are tagged,
    // and not heap allocated.
    auto func = currentModule()->getOrInsertFunction("rt_make_boolean",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8Ty(llvmContext()));

    return currentBuilder()->CreateCall(func,
            {llvm::ConstantInt::getSigned(llvm::IntegerType::getInt8Ty(llvmContext()),
                    value ? 1 : 0)});
}

llvm::Value* Compiler::makeSymbol(std::shared_ptr<std::string> name) {
    auto func = currentModule()->getOrInsertFunction("rt_make_symbol",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvmContext(), 0));

    auto strptr = currentBuilder()->CreateGlobalStringPtr(*name);
    return currentBuilder()->CreateCall(func, {strptr});
}

llvm::Value* Compiler::makeString(std::shared_ptr<std::string> str) {
    auto func = currentModule()->getOrInsertFunction("rt_make_string",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvmContext(), 0));

    auto strptr = currentBuilder()->CreateGlobalStringPtr(*str);
    return currentBuilder()->CreateCall(func, {strptr});
}

llvm::Value* Compiler::makeKeyword(std::shared_ptr<std::string> name) {
    auto func = currentModule()->getOrInsertFunction("rt_make_keyword",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvmContext(), 0));

    auto strptr = currentBuilder()->CreateGlobalStringPtr(*name);
    return currentBuilder()->CreateCall(func, {strptr});
}

llvm::Value* Compiler::makeClosure(uint64_t arity, bool has_rest_args, llvm::Value* func_ptr, uint64_t env_size) {
    auto func = currentModule()->getOrInsertFunction("rt_make_compiled_function",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt32Ty(llvmContext()),
            llvm::IntegerType::getInt32Ty(llvmContext()),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt64Ty(llvmContext()));

    return currentBuilder()->CreateCall(func,
            {llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(llvmContext()), arity),
             llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(llvmContext()),
                     has_rest_args ? 1 : 0),
             currentBuilder()->CreatePointerCast(func_ptr,
                     llvm::IntegerType::getInt8PtrTy(
                             llvmContext(),
                             kGCAddressSpace)),
             llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(llvmContext()),
                     env_size)});
}

llvm::Value* Compiler::makePair(llvm::Value* v, llvm::Value* next) {
    auto func = currentModule()->getOrInsertFunction("rt_make_pair",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace));

    return currentBuilder()->CreateCall(func, {v, next});
}

llvm::Value* Compiler::getBooleanValue(llvm::Value* val) {
    auto func = currentModule()->getOrInsertFunction("rt_is_true",
            llvm::IntegerType::getInt8Ty(llvmContext()),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace));

    return currentBuilder()->CreateCall(func, {val});
}

llvm::Value* Compiler::makeVar(llvm::Value* sym) {
    auto func = currentModule()->getOrInsertFunction("rt_make_var",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace));

    return currentBuilder()->CreateCall(func, {sym});
}

void Compiler::buildSetVar(llvm::Value* var, llvm::Value* new_val) {
    auto func = currentModule()->getOrInsertFunction("rt_set_var",
            llvm::Type::getVoidTy(llvmContext()),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace));

    currentBuilder()->CreateCall(func, {var, new_val});
}

llvm::Value* Compiler::buildDerefVar(llvm::Value* var) {
    auto func = currentModule()->getOrInsertFunction("rt_deref_var",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace));

    return currentBuilder()->CreateCall(func, {var});
}

llvm::Value* Compiler::buildGetLambdaPtr(llvm::Value* fn) {
    auto func = currentModule()->getOrInsertFunction("rt_compiled_function_get_ptr",
            llvm::IntegerType::getInt8PtrTy(llvmContext(), 0),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace));
    return currentBuilder()->CreateCall(func, {fn});
}

llvm::Value* Compiler::buildLambdaSetEnv(llvm::Value* fn, uint64_t idx, llvm::Value* val) {
    auto func = currentModule()->getOrInsertFunction("rt_compiled_function_set_env",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt64Ty(llvmContext()),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace));

    llvm::Value* idx_val = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(llvmContext()), idx);

    return currentBuilder()->CreateCall(func, {fn, idx_val, val});
}

llvm::Value* Compiler::buildLambdaGetEnv(llvm::Value* fn, uint64_t idx) {
    auto func = currentModule()->getOrInsertFunction("rt_compiled_function_get_env",
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace),
            llvm::IntegerType::getInt64Ty(llvmContext()));

    llvm::Value* idx_val = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(llvmContext()), idx);

    return currentBuilder()->CreateCall(func, {fn, idx_val});
}

llvm::Value* Compiler::buildGcAddRoot(llvm::Value* obj) {
    auto func = currentModule()->getOrInsertFunction("rt_gc_add_root",
            llvm::Type::getVoidTy(llvmContext()),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace));

    return currentBuilder()->CreateCall(func, {obj});
}

llvm::Value* Compiler::buildGcRemoveRoot(llvm::Value* obj) {
    auto func = currentModule()->getOrInsertFunction("rt_gc_remove_root",
            llvm::Type::getVoidTy(llvmContext()),
            llvm::IntegerType::getInt8PtrTy(llvmContext(),
                    kGCAddressSpace));

    return currentBuilder()->CreateCall(func, {obj});
}

llvm::Value* Compiler::buildApply(llvm::Value* f, llvm::Value* args) {
    auto func = currentModule()->getOrInsertFunction("rt_apply",
            llvm::Type::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            llvm::Type::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            llvm::Type::getInt8PtrTy(llvmContext(), kGCAddressSpace));

    return currentBuilder()->CreateCall(func, {f, args});
}

llvm::Value* Compiler::buildApplyInvoke(llvm::Value* f, llvm::Value* args, shared_ptr<EHCompileInfo> eh_info) {
    auto func = currentModule()->getOrInsertFunction("rt_apply",
            llvm::Type::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            llvm::Type::getInt8PtrTy(llvmContext(), kGCAddressSpace),
            llvm::Type::getInt8PtrTy(llvmContext(), kGCAddressSpace));

    auto cont_block = llvm::BasicBlock::Create(llvmContext(), "cont", currentContext()->currentFunc());
    auto inv = currentBuilder()->CreateInvoke(func, cont_block, eh_info->catch_dest, {f, args});
    currentBuilder()->SetInsertPoint(cont_block);
    return inv;
}

llvm::DISubroutineType* Compiler::createFunctionDebugType(int num_args) {
    llvm::SmallVector<llvm::Metadata*, 8> element_types;

    auto arg_type = currentContext()->currentDebugInfo()->getVoidPtrType();

    // Add the result type.
    element_types.push_back(arg_type);

    for (unsigned i = 0, e = num_args; i!=e; ++i)
        element_types.push_back(arg_type);

    return currentContext()->currentDIBuilder()->createSubroutineType(
            currentContext()->currentDIBuilder()->getOrCreateTypeArray(element_types));
}

} // namespace electrum
