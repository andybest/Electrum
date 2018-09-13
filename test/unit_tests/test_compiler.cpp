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
#include "compiler/Compiler.h"
#include "runtime/Runtime.h"

using namespace electrum;

TEST(Compiler, compilesConstantInteger) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compile_and_eval_string("1234");

    EXPECT_TRUE(rt_is_integer(result));
    EXPECT_EQ(rt_integer_value(result), 1234);

    rt_deinit_gc();
}

TEST(Compiler, compilesConstantFloat) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compile_and_eval_string("1234.5678");

    EXPECT_TRUE(rt_is_float(result));
    EXPECT_FLOAT_EQ(rt_float_value(result), 1234.5678);

    rt_deinit_gc();
}

