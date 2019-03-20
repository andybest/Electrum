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

#include <iostream>
#include <stack>
#include "GarbageCollector.h"
#include "stackmap/api.h"
#include "Runtime.h"
#include <cassert>

namespace electrum {

GarbageCollector::GarbageCollector(GCMode mode)
        :collector_mode_(mode) {
    switch (mode) {
    case kGCModeCompilerOwned:scan_stack_ = true;
        break;
    case kGCModeInterpreterOwned:
        // If the GC is owned by the interpreter, we won't have a
        // stack map to scan
        scan_stack_ = false;
    }
}

GarbageCollector::~GarbageCollector() {
    // Free all heap objects
    for (auto ptr: heap_objects_) {
        auto header = TAG_TO_OBJECT(ptr);
        this->free(header);
    }
}

void GarbageCollector::init_stackmap(void* stackmap) {
    statepoint_tables_.push_back(generate_table(stackmap, 0.5));
}

frame_info_t* GarbageCollector::get_frame_info(uint64_t return_address) {
    for (auto statepoint_table: statepoint_tables_) {
        auto f_info = lookup_return_address(statepoint_table, return_address);
        if (f_info) {
            return f_info;
        }
    }

    return nullptr;
}

/**
 * Maybe perform a garbage collection pass
 * @param stackPointer The stack pointer of the call point
 */
void GarbageCollector::collect(void* stackPointer) {

    auto return_address = *static_cast<uint64_t*>(stackPointer);

    auto frame_info = get_frame_info(return_address);

    auto stackIndex = reinterpret_cast<uintptr_t>(stackPointer);
    stackIndex += sizeof(void*);

    while (frame_info!=nullptr) {
        for (uint16_t i = 0; i<frame_info->numSlots; i++) {
            auto pointerSlot = frame_info->slots[i];
            if (pointerSlot.kind>=0) {
                continue;
            }

            auto ptr = reinterpret_cast<void**>(stackIndex+pointerSlot.offset);

            // Mark if it is an object
            if (is_object(*ptr)) {
                traverse_object(*ptr);
            }
        }

        // Move to next frame
        stackIndex = stackIndex+frame_info->frameSize;
        return_address = *reinterpret_cast<uint64_t*>(stackIndex);
        stackIndex += sizeof(void*);
        frame_info = get_frame_info(return_address);
    };
    // Mark roots
    for (auto it: object_roots_) {
        if (is_object(it)) {
            traverse_object(it);
        }
    }

    sweep_heap();
}

void GarbageCollector::traverse_object(void* vobj) {
    auto obj = TAG_TO_OBJECT(vobj);

    // Skip if the object has already been seen
    if (obj->gc_mark) {
        return;
    }

    std::vector<EObjectHeader*> st;
    st.push_back(obj);

    while (!st.empty()) {
        obj = st.back();
        st.pop_back();

        if (obj->gc_mark) {
            return;
        }

        // Mark this object
        obj->gc_mark = 1;

        switch (obj->tag) {
        case kETypeTagFloat:break;
        case kETypeTagKeyword:break;
        case kETypeTagString:break;
        case kETypeTagSymbol:break;

        case kETypeTagPair: {
            auto pair = reinterpret_cast<EPair*>(reinterpret_cast<void*>(obj));

            if (is_object(pair->value)) {
                st.push_back(TAG_TO_OBJECT(pair->value));
            }

            if (is_object(pair->next)) {
                st.push_back(TAG_TO_OBJECT(pair->next));
            }

            break;
        }

        case kETypeTagVar: {
            auto var = reinterpret_cast<EVar*>(reinterpret_cast<void*>(obj));

            if (is_object(var->sym)) {
                st.push_back(TAG_TO_OBJECT(var->sym));
            }

            if (is_object(var->val)) {
                st.push_back(TAG_TO_OBJECT(var->val));
            }

            break;
        }

        case kETypeTagFunction: {
            auto fn = reinterpret_cast<ECompiledFunction*>(reinterpret_cast<void*>(obj));

            for (uint64_t i = 0; i<fn->env_size; i++) {
                auto e = fn->env[i];
                if (is_object(e)) {
                    st.push_back(TAG_TO_OBJECT(e));
                }
            }

            break;
        }

        case kETypeTagInterpretedFunction: {
            auto fn = reinterpret_cast<EInterpretedFunction*>(reinterpret_cast<void*>(obj));

            break;
        }

        default:break;
        }
    }

}

/**
 * Allocate garbage collected memory
 * @param size The size of the memory block to allocate
 * @return A pointer to the allocated memory
 */
void* GarbageCollector::malloc(size_t size) {
    // TODO: Place in heap list
    return std::malloc(size);
}

/**
 * Allocate garbage collected memory. Assumes that the pointer will
 * be tagged.
 * @param size The size of the memory block to allocate
 * @return A pointer to the allocated memory
 */
void* GarbageCollector::malloc_tagged_object(size_t size) {
    auto ptr = std::malloc(size);
    /* The object will be tagged by the runtime, so add the tag to the
     * pointer for faster lookup later.
     */
    heap_objects_.push_back(OBJECT_TO_TAG(ptr));
    return ptr;
}

/**
 * Explicitly free a garbage collected pointer
 * @param ptr The pointer of the memory block to free
 */
void GarbageCollector::free(void* ptr) {
    std::free(ptr);
}

void GarbageCollector::add_object_root(void* root) {
    if (!is_object(root)) {
        return;
    }

    assert(object_roots_.count(root)==0);
    object_roots_.emplace(root);
}

bool GarbageCollector::remove_object_root(void* root) {
    object_roots_.erase(root);
    return true;
}

uint64_t GarbageCollector::sweep_heap() {
    uint64_t numCollected = 0;

    auto it = heap_objects_.begin();
    while (it!=heap_objects_.end()) {
        auto header = TAG_TO_OBJECT(*it);

        if (!header->gc_mark) {
            // Object is not marked, collect it.
            it = heap_objects_.erase(it);
            this->free(header);
            ++numCollected;
        }
        else {
            // Object was marked, unmark it
            header->gc_mark = 0;
            ++it;
        }
    }

    return numCollected;
}
}

/**
 * Initialize the garbage collector
 */
extern "C" void rt_init_gc(void* stackmap) {
    //electrum::main_collector = std::make_shared<electrum::GarbageCollector>(electrum::kGCModeCompilerOwned);
    electrum::main_collector = new electrum::GarbageCollector(electrum::kGCModeCompilerOwned);
    electrum::main_collector->init_stackmap(stackmap);
}

extern "C" void rt_gc_init_stackmap(void* stackmap) {
    if(stackmap != nullptr) {
        auto collector = rt_get_gc();
        collector->init_stackmap(stackmap);
    }
}

extern "C" void rt_gc_add_root(void* obj) {
    auto collector = rt_get_gc();
    collector->add_object_root(obj);
}

extern "C" void rt_gc_remove_root(void* obj) {
    auto collector = rt_get_gc();
    collector->remove_object_root(obj);
}

/**
 * Entry into the garbage collector from a statepoint
 * @param stackPointer The stack pointer, as provided by rt_enter_gc()
 */
extern "C" void rt_enter_gc_impl(void* stackPointer) {
    auto collector = rt_get_gc();
    collector->collect(stackPointer);
}

/**
 * A shim wrapping the actual GC entry function to get the stack pointer
 * as an argument
 */
extern "C" __attribute__((naked)) void rt_enter_gc() {
#if __x86_64__
#if __APPLE__
    asm("mov %rsp, %rdi\n"
        "jmp _rt_enter_gc_impl");
#else
    asm("mov %rsp, %rdi\n"
        "jmp rt_enter_gc_impl");
#endif
#elif __aarch64__
    asm("mov x0, sp\n"
        "jmp _rt_enter_gc_impl");
#else
#error Unsupported archetecture for GC
#endif
}