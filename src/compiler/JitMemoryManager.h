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


#ifndef ELECTRUM_JITMEMORYMANAGER_H
#define ELECTRUM_JITMEMORYMANAGER_H

#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include "llvm/ADT/StringRef.h"
#include "ElectrumJit.h"
#include <memory>

namespace electrum {
using llvm::SectionMemoryManager;
using llvm::StringRef;

class JitMemoryManager : public SectionMemoryManager {

public:
    JitMemoryManager(std::function<void(void*)>);

    uint8_t* allocateCodeSection(uintptr_t Size, unsigned Alignment,
            unsigned SectionID,
            StringRef SectionName) override;

    uint8_t* allocateDataSection(uintptr_t Size, unsigned Alignment,
            unsigned SectionID, StringRef SectionName,
            bool isReadOnly) override;

    void* getStackMapPtr() {
        return stackMapPtr_;
    }

private:
    void* stackMapPtr_;
    std::function<void(void*)> stackMapCB;
};
}

#endif //ELECTRUM_JITMEMORYMANAGER_H
