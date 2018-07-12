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

namespace electrum {

    GarbageCollector::GarbageCollector(void *stackmap) {
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
        return nullptr;
    }

    /**
     * Explicitly free a garbage collected pointer
     * @param ptr The pointer of the memory block to free
     */
    void GarbageCollector::free(void *ptr) {
    }


}

/**
 * Initialize the garbage collector
 */
extern "C" void rt_init_gc(void *stackmap) {
    electrum::main_collector = std::make_shared<electrum::GarbageCollector>(
            stackmap);
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