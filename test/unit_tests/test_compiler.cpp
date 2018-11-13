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

    EXPECT_EQ(rt_is_integer(result), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result), 1234);

    rt_deinit_gc();
}

TEST(Compiler, compilesConstantFloat) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compile_and_eval_string("1234.5678");

    EXPECT_EQ(rt_is_float(result), TRUE_PTR);
    EXPECT_FLOAT_EQ(rt_float_value(result), 1234.5678);

    rt_deinit_gc();
}

TEST(Compiler, compilesConstantBoolean) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compile_and_eval_string("#t");

    EXPECT_EQ(rt_is_boolean(result), TRUE_PTR);
    EXPECT_EQ(result, TRUE_PTR);

    auto result2 = c.compile_and_eval_string("#f");
    EXPECT_EQ(rt_is_boolean(result2), TRUE_PTR);
    EXPECT_EQ(result2, FALSE_PTR);

    rt_deinit_gc();
}

TEST(Compiler, compilesConstantString) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compile_and_eval_string("\"foo\"");

    EXPECT_EQ(rt_is_string(result), TRUE_PTR);
    EXPECT_STREQ(rt_string_value(result), "foo");

    rt_deinit_gc();
}

TEST(Compiler, compilesIf) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compile_and_eval_string("(if #t 1234 5678)");

    EXPECT_EQ(rt_is_integer(result), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result), 1234);

    auto result2 = c.compile_and_eval_string("(if #f 1234 5678)");
    EXPECT_EQ(rt_is_integer(result2), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result2), 5678);

    rt_deinit_gc();
}

TEST(Compiler, compilesDo) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compile_and_eval_string("(do 123 456 789)");

    EXPECT_EQ(rt_is_integer(result), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result), 789);

    rt_deinit_gc();
}

TEST(Compiler, compilesDef) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compile_and_eval_string("(do (def a 1234) a)");

    EXPECT_EQ(rt_is_integer(result), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result), 1234);

    c.compile_and_eval_string("(def b \"foo\")");
    auto result2 = c.compile_and_eval_string("b");

    EXPECT_EQ(rt_is_string(result2), TRUE_PTR);
    EXPECT_STREQ(rt_string_value(result2), "foo");

    rt_deinit_gc();
}

TEST(Compiler, compilesBasicLambda)  {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto def = c.compile_and_eval_string("(def l (lambda (x) x))");
    auto result1 = c.compile_and_eval_string("(l 1234)");
    auto result2 = c.compile_and_eval_string("(l 5.678)");

    EXPECT_EQ(rt_is_integer(result1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result1), 1234);

    EXPECT_EQ(rt_is_float(result2), TRUE_PTR);
    EXPECT_FLOAT_EQ(rt_float_value(result2), 5.678);
    rt_deinit_gc();
}

TEST(Compiler, compilesLambdaWithBranch)  {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto def = c.compile_and_eval_string("(def b (lambda (x) (if x 1234 5.678)))");
    auto result1 = c.compile_and_eval_string("(b #t)");
    auto result2 = c.compile_and_eval_string("(b #f)");

    EXPECT_EQ(rt_is_integer(result1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result1), 1234);

    EXPECT_EQ(rt_is_float(result2), TRUE_PTR);
    EXPECT_FLOAT_EQ(rt_float_value(result2), 5.678);
    rt_deinit_gc();
}