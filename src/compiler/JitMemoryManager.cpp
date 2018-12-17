#include <utility>

//
// Created by Andy Best on 2018-12-13.
//

#include "JitMemoryManager.h"
#include <iostream>

namespace electrum {
    uint8_t *JitMemoryManager::allocateCodeSection(uintptr_t Size,
                                                   unsigned Alignment,
                                                   unsigned SectionID,
                                                   llvm::StringRef SectionName) {

        std::cout << SectionName.str() << std::endl;
        return SectionMemoryManager::allocateCodeSection(Size, Alignment, SectionID, SectionName);
    }

    uint8_t *JitMemoryManager::allocateDataSection(uintptr_t Size,
                                                   unsigned Alignment,
                                                   unsigned SectionID,
                                                   StringRef SectionName,
                                                   bool isReadOnly) {
        std::cout << SectionName.str() << std::endl;
        auto section_ptr =  SectionMemoryManager::allocateDataSection(Size, Alignment, SectionID, SectionName, isReadOnly);

        if(SectionName == ".llvm_stackmaps" || SectionName == "__llvm_stackmaps") {
            stackMapPtr_ = section_ptr;
            stackMapCB(stackMapPtr_);
        }

        return section_ptr;
    }

    JitMemoryManager::JitMemoryManager(std::function<void(void *)> stackmap_cb) : SectionMemoryManager(nullptr) {
        this->stackMapCB = std::move(stackmap_cb);
    }
}
