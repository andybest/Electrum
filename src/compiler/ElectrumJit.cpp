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

namespace electrum {
    ElectrumJit::ElectrumJit()
            : target_machine_(llvm::EngineBuilder().selectTarget()),
              data_layout_(target_machine_->createDataLayout()),
              object_layer_([]() { return std::make_shared<llvm::SectionMemoryManager>(); }),
              compile_layer_(object_layer_, llvm::orc::SimpleCompiler(*target_machine_)) {
        llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
    }

    llvm::TargetMachine &ElectrumJit::getTargetMachine() { return *target_machine_; }

    ElectrumJit::ModuleHandle ElectrumJit::addModule(std::unique_ptr<llvm::Module> module) {
        auto resolver = llvm::orc::createLambdaResolver(
                [&](const std::string &name) {
                    if(auto sym = compile_layer_.findSymbol(name, false)) {
                        return sym;
                    }
                    return llvm::JITSymbol(nullptr);
                },
                [&](const std::string &name) {
                    if(auto symaddr = llvm::RTDyldMemoryManager::getSymbolAddressInProcess(name)) {
                        return llvm::JITSymbol(symaddr, llvm::JITSymbolFlags::Exported);
                    }
                    return llvm::JITSymbol(nullptr);
                });

        return llvm::cantFail(compile_layer_.addModule(std::move(module), std::move(resolver)));
    }

    llvm::JITSymbol ElectrumJit::find_symbol(const std::string &name) {
        std::string mangled_name;
        llvm::raw_string_ostream mangled_name_stream(mangled_name);
        llvm::Mangler::getNameWithPrefix(mangled_name_stream, name, data_layout_);
        return compile_layer_.findSymbol(name, false);//mangled_name_stream.str(), false);
    }

    llvm::JITTargetAddress ElectrumJit::get_symbol_address(const std::string &name) {
        return llvm::cantFail(find_symbol(name).getAddress());
    }

    void ElectrumJit::remove_module(ElectrumJit::ModuleHandle h) {
        llvm::cantFail(compile_layer_.removeModule(h));
    }
}
