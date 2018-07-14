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

#include "GarbageCollector.h"
#include "stackmap/api.h"
#include "Runtime.h"

namespace electrum {

    GarbageCollector::GarbageCollector(GCMode mode): collector_mode_(mode) {
        switch(mode) {
            case kGCModeCompilerOwned:
                scan_stack_ = true;
                break;
            case kGCModeInterpreterOwned:
                // If the GC is owned by the interpreter, we won't have a
                // stack map to scan
                scan_stack_ = false;
        }
    }

    void GarbageCollector::init_stackmap(void *stackmap) {
        statepoint_table_ = generate_table(stackmap, 0.5);
    }

    /**
     * Maybe perform a garbage collection pass
     * @param stackPointer The stack pointer of the call point
     */
    void GarbageCollector::collect(void *stackPointer) {
        auto return_address = *static_cast<uint64_t *>(stackPointer);

        auto frame_info = lookup_return_address(statepoint_table_,
                                                return_address);
    }

    /**
     * Allocate garbage collected memory
     * @param size The size of the memory block to allocate
     * @return A pointer to the allocated memory
     */
    void *GarbageCollector::malloc(size_t size) {
        // TODO: Place in heap list
        return std::malloc(size);
    }

    /**
     * Explicitly free a garbage collected pointer
     * @param ptr The pointer of the memory block to free
     */
    void GarbageCollector::free(void *ptr) {
        std::free(ptr);
    }

    void GarbageCollector::add_object_root(void *root) {
        assert(object_roots_.count(root) == 0);
        object_roots_.emplace(root);
    }

    bool GarbageCollector::remove_object_root(void *root) {
        object_roots_.erase(root);
        return true;
    }

    std::list<void *> GarbageCollector::pointers_to_collect() {
        return std::list<void *>();
    }

    void GarbageCollector::mark_roots() {

    }

    void GarbageCollector::collect_roots() {

    }


    void GarbageCollector::mark_object_root(void* obj) {
        // Make sure the object tag bit is set
        assert((reinterpret_cast<uintptr_t>(obj) & TAG_MASK) == OBJECT_TAG);
        auto header = TAG_TO_OBJECT(obj);

        // Mark each pointer that the object has access to
        switch(header->tag) {
            default: break;
        }

        header->gc_mark = 1;
    }

    void GarbageCollector::sweep_heap() {
        for(auto it = heap_objects_.begin(); it != heap_objects_.end(); ++it) {
            auto header = TAG_TO_OBJECT(*it);

            if(!header->gc_mark) {
                // Object is not marked, collect it.
                heap_objects_.erase(it);
                this->free(header);
            } else {
                // Object was marked, unmark it
                header->gc_mark = 0;
            }
        }
    }


}

/**
 * Initialize the garbage collector
 */
extern "C" void rt_init_gc(void *stackmap) {
    electrum::main_collector = std::make_shared<electrum::GarbageCollector>(electrum::kGCModeCompilerOwned);
    electrum::main_collector->init_stackmap(stackmap);
}

/**
 * Entry into the garbage collector from a statepoint
 * @param stackPointer The stack pointer, as provided by rt_enter_gc()
 */
extern "C" void rt_enter_gc_impl(void *stackPointer) {
    electrum::main_collector->collect(stackPointer);
}

/**
 * A shim wrapping the actual GC entry function to get the stack pointer
 * as an argument
 */
extern "C" __attribute__((naked)) void rt_enter_gc() {

#if __x86_64__
    asm("mov %rsp, %rdi\n"
        "jmp rt_enter_gc_impl");
#elif __aarch64__
    asm("mov x0, sp\n"
        "jmp rt_enter_gc_impl");
#else
#error Unsupported archetecture for GC
#endif
}