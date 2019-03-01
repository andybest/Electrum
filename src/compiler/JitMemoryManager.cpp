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


#include <utility>
#include "JitMemoryManager.h"
#include <iostream>

namespace electrum {
uint8_t* JitMemoryManager::allocateCodeSection(uintptr_t Size,
        unsigned Alignment,
        unsigned SectionID,
        llvm::StringRef SectionName) {
    return SectionMemoryManager::allocateCodeSection(Size, Alignment, SectionID, SectionName);
}

uint8_t* JitMemoryManager::allocateDataSection(uintptr_t Size,
        unsigned Alignment,
        unsigned SectionID,
        StringRef SectionName,
        bool isReadOnly) {
    auto section_ptr = SectionMemoryManager::allocateDataSection(Size, Alignment, SectionID, SectionName, isReadOnly);

    if (SectionName==".llvm_stackmaps" || SectionName=="__llvm_stackmaps") {
        stackMapPtr_ = section_ptr;
        stackMapCB(stackMapPtr_);
    }

    return section_ptr;
}

JitMemoryManager::JitMemoryManager(std::function<void(void*)> stackmap_cb)
        :SectionMemoryManager(nullptr) {
    this->stackMapCB = std::move(stackmap_cb);
}
}
