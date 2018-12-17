//
// Created by Andy Best on 2018-12-13.
//

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

        uint8_t *allocateCodeSection(uintptr_t Size, unsigned Alignment,
                                     unsigned SectionID,
                                     StringRef SectionName) override;

        uint8_t *allocateDataSection(uintptr_t Size, unsigned Alignment,
                                     unsigned SectionID, StringRef SectionName,
                                     bool isReadOnly) override;

        void *getStackMapPtr() {
            return stackMapPtr_;
        }

    private:
        void *stackMapPtr_;
        std::function<void(void *)> stackMapCB;
    };
}


#endif //ELECTRUM_JITMEMORYMANAGER_H
