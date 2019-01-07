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

#ifndef ELECTRUM_GARBAGECOLLECTOR_H
#define ELECTRUM_GARBAGECOLLECTOR_H


#ifdef __cplusplus

#include <memory>
#include "stackmap/api.h"
#include <vector>
#include <unordered_set>
#include <list>

namespace electrum {

    using std::shared_ptr;
    using std::make_shared;

    /**
     * Decides how the Garbage collector will be run, and specifically
     * if the LLVM Stackmap will be scanned for live references.
     */
    enum GCMode {
        /** Interpreter initialized and owns the GC Instance. */
                kGCModeInterpreterOwned,

        /** Compiler initialized and owns the GC instance. */
                kGCModeCompilerOwned
    };

    class GarbageCollector {
    public:
        explicit GarbageCollector(GCMode mode);
        ~GarbageCollector();

        void init_stackmap(void *stackmap);

        frame_info_t *get_frame_info(uint64_t return_address);

        void collect(void *stackPointer);

        void traverse_object(void *obj);

        void add_object_root(void* root);

        bool remove_object_root(void *root);

        void *malloc(size_t size);

        void *malloc_tagged_object(size_t size);

        void free(void *ptr);

    private:
        std::vector<statepoint_table_t *> statepoint_tables_;
        GCMode collector_mode_;
        bool scan_stack_;
        std::unordered_set<void *> object_roots_;
        std::vector<void*> heap_objects_;
        uint64_t sweep_heap();
    };

    static GarbageCollector *main_collector;
}

extern "C" {
#endif  // __cplusplus

/* Exported functions */
void rt_gc_init_stackmap(void *stackmap);

#ifdef __cplusplus
}
#endif // _cplusplus


#endif //ELECTRUM_GARBAGECOLLECTOR_H
