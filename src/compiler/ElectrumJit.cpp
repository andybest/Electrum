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

#include "ElectrumJit.h"
#include <memory>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/IR/Verifier.h>
#include <iostream>

namespace electrum {

static std::unique_ptr<llvm::Module> optimize_module(std::unique_ptr<llvm::Module> module) {
    auto fpm                 = std::make_unique<llvm::legacy::FunctionPassManager>(module.get());

//    fpm->add(llvm::createPlaceSafepointsPass());
    fpm->doInitialization();

    for (auto& f: *module) {
        fpm->run(f);
    }

//    auto error_stream = llvm::raw_string_ostream(errors);
//    auto has_errors = llvm::verifyModule(*module, &error_stream, nullptr);
//    if (has_errors) {
//        error_stream.flush();
//        std::cout << errors << std::endl;
//        module->print(llvm::errs(), nullptr);
//        throw std::exception();
//    }

//    module->print(llvm::errs(), nullptr);

    /*llvm::legacy::PassManager pm;
    pm.add(llvm::createRewriteStatepointsForGCLegacyPass());
    pm.run(*module);
     */

    std::string errors;
    auto        error_stream = llvm::raw_string_ostream(errors);
    auto        has_errors   = llvm::verifyModule(*module, &error_stream, nullptr);
    if (has_errors) {
        std::cout << errors << std::endl;
        std::fflush(stdout);
        error_stream.flush();
        module->print(llvm::errs(), nullptr);
        throw std::exception();
    }

    return module;
}

ElectrumJit::ElectrumJit(llvm::orc::ExecutionSession& es)
        :es_(es),
         resolver_(createLegacyLookupResolver(
                 es_,
                 [this](const std::string& Name) -> llvm::JITSymbol {
                   if (auto Sym = optimize_layer_.findSymbol(Name, false)) {
                       return Sym;
                   }
                   else if (auto Err     = Sym.takeError()) {
                       std::cerr << "JIT- Could not resolve symbol: " << Name << std::endl;
                       return std::move(Err);
                   }
                   if (auto      SymAddr =
                                         llvm::RTDyldMemoryManager::getSymbolAddressInProcess(Name)) {
                       return llvm::JITSymbol(SymAddr, llvm::JITSymbolFlags::Exported);
                   }

                   std::cerr << "JIT- Could not resolve symbol: " << Name << std::endl;
                   throw std::exception();
                 },
                 [](llvm::Error Err) { cantFail(std::move(Err), "lookupFlags failed"); })),
         target_machine_(llvm::EngineBuilder().selectTarget()),
         data_layout_(target_machine_->createDataLayout()),
         object_layer_(es_,
                 [this](llvm::orc::VModuleKey) {
                   return llvm::orc::LegacyRTDyldObjectLinkingLayer::Resources{
                           std::make_shared<JitMemoryManager>([this](void* stackMapPtr) {
                             this->stack_map_ptr_ = stackMapPtr;
                           }), resolver_};
                 },

                 // Notify Loaded
                 [this](llvm::orc::VModuleKey, const llvm::object::ObjectFile& obj,
                         const llvm::RuntimeDyld::LoadedObjectInfo& info) {
                   //this->gdb_listener_->notifyObjectLoaded();
                   //NotifyObjectEmitted(obj, info);
                 },
                 [this](llvm::orc::VModuleKey, const llvm::object::ObjectFile& obj,
                         const llvm::RuntimeDyld::LoadedObjectInfo& info) {
                   uint64_t key = static_cast<uint64_t>(
                           reinterpret_cast<uintptr_t>(obj.getData().data()));
                   this->gdb_listener_->notifyObjectLoaded(key, obj, info);
                 }),
         compile_layer_(object_layer_, llvm::orc::SimpleCompiler(*target_machine_)),
         optimize_layer_(compile_layer_, [this](std::unique_ptr<llvm::Module> M) {
           return optimize_module(std::move(M));
         }),
         stack_map_ptr_(nullptr) {
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

    object_layer_.setProcessAllSections(true);
    gdb_listener_ = llvm::JITEventListener::createGDBRegistrationListener();
}

llvm::TargetMachine& ElectrumJit::getTargetMachine() { return *target_machine_; }

llvm::orc::VModuleKey ElectrumJit::addModule(std::unique_ptr<llvm::Module> module) {
    auto k = es_.allocateVModule();
    llvm::cantFail(optimize_layer_.addModule(k, std::move(module)));

    auto error = optimize_layer_.emitAndFinalize(k);
    return k;
}

llvm::JITSymbol ElectrumJit::findSymbol(const std::string& name) {
    std::string              mangled_name;
    llvm::raw_string_ostream mangled_name_stream(mangled_name);
    llvm::Mangler::getNameWithPrefix(mangled_name_stream, name, data_layout_);
    return optimize_layer_.findSymbol(name, false);
}

llvm::JITTargetAddress ElectrumJit::getSymbolAddress(const std::string& name) {
    return llvm::cantFail(findSymbol(name).getAddress());
}

void ElectrumJit::removeModule(llvm::orc::VModuleKey h) {
    llvm::cantFail(optimize_layer_.removeModule(h));
}

}
