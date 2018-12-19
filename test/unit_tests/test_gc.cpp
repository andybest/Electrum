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

#include "gtest/gtest.h"
#include "compiler/Parser.h"
#include "types/Types.h"
#include "runtime/Runtime.h"
#include "runtime/GarbageCollector.h"

using namespace electrum;

/*
TEST(GC, does_not_collect_root_object) {
    rt_init_gc(kGCModeInterpreterOwned);

    auto f = rt_make_float(1.234);
    rt_get_gc()->add_object_root(f);
    auto numCollected = rt_get_gc()->collect_roots();

    EXPECT_EQ(numCollected, 0);
    rt_deinit_gc();
}

TEST(GC, collects_orphaned_object) {
    rt_init_gc(kGCModeInterpreterOwned);

    rt_make_float(1.234);
    rt_make_float(2.345);
    auto numCollected = rt_get_gc()->collect_roots();

    EXPECT_EQ(numCollected, 2);
    rt_deinit_gc();
}

TEST(GC, collects_correct_object) {
    rt_init_gc(kGCModeInterpreterOwned);

    auto f1 = rt_make_float(1.234);
    rt_make_float(2.345);

    rt_get_gc()->add_object_root(f1);
    auto numCollected = rt_get_gc()->collect_roots();
    EXPECT_EQ(numCollected, 1);

    // This should not crash
    EXPECT_FLOAT_EQ(rt_float_value(f1), 1.234);
}*/