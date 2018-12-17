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

#ifndef ELECTRUM_ELECTRUMJIT_H
#define ELECTRUM_ELECTRUMJIT_H

#include <llvm/ADT/STLExtras.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/OrcRemoteTargetClient.h>
#include <llvm/ExecutionEngine/Orc/IRTransformLayer.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Mangler.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "JitMemoryManager.h"

namespace electrum {

    class ElectrumJit {

    private:
        llvm::orc::ExecutionSession &es_;
        std::shared_ptr<llvm::orc::SymbolResolver> resolver_;
        std::unique_ptr<llvm::TargetMachine> target_machine_;

        const llvm::DataLayout data_layout_;
        llvm::orc::RTDyldObjectLinkingLayer object_layer_;
        llvm::orc::IRCompileLayer<decltype(object_layer_), llvm::orc::SimpleCompiler> compile_layer_;

        using OptimizeFunction =
        std::function<std::unique_ptr<llvm::Module>(std::unique_ptr<llvm::Module>)>;
        llvm::orc::IRTransformLayer <decltype(compile_layer_), OptimizeFunction > optimize_layer_;

        llvm::orc::JITCompileCallbackManager *compile_callback_mgr_;
        std::unique_ptr<llvm::orc::IndirectStubsManager> indirect_stubs_mgr_;
        void *stack_map_ptr_;

    public:
        using MyRemote = llvm::orc::remote::OrcRemoteTargetClient;

        ElectrumJit(llvm::orc::ExecutionSession &es);

        llvm::TargetMachine &getTargetMachine();

        llvm::orc::VModuleKey addModule(std::unique_ptr<llvm::Module> module);

        llvm::JITSymbol find_symbol(const std::string &name);

        llvm::JITTargetAddress get_symbol_address(const std::string &name);

        void remove_module(llvm::orc::VModuleKey h);

        void *get_stack_map_pointer() {
            return stack_map_ptr_;
        }
    };

}


#endif //ELECTRUM_ELECTRUMJIT_H
