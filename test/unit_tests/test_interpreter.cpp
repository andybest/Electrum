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
#include "interpreter/Interpreter.h"
#include "types/Types.h"
#include <memory>
#include <interpreter/InterpreterExceptions.h>
#include <runtime/Runtime.h>

using namespace electrum;
using std::shared_ptr;
using std::make_shared;


TEST(Interpreter, if_evaluates_correct_expressions) {
    rt_init_gc(kGCModeInterpreterOwned);

    auto cond1 = rt_make_boolean(static_cast<int8_t>(true));
    auto cond2 = rt_make_boolean(static_cast<int8_t>(false));

    auto subsequent = rt_make_integer(1234);
    auto alternative = rt_make_integer(5678);

    auto if1 = rt_make_pair(rt_make_symbol("if"),
                            rt_make_pair(cond1,
                                         rt_make_pair(subsequent,
                                                      rt_make_pair(alternative, NIL_PTR))));

    auto if2 = rt_make_pair(rt_make_symbol("if"),
                            rt_make_pair(cond2,
                                         rt_make_pair(subsequent,
                                                      rt_make_pair(alternative, NIL_PTR))));

    auto interp = Interpreter();

    try {
        auto result1 = interp.evalExpr(if1, nullptr);
        auto result2 = interp.evalExpr(if2, nullptr);
        EXPECT_TRUE(is_integer(result1));
        EXPECT_EQ(TAG_TO_INTEGER(result1), 1234);

        EXPECT_TRUE(is_integer(result2));
        EXPECT_EQ(TAG_TO_INTEGER(result2), 5678);

    } catch (InterpreterException &e) {
        GTEST_FATAL_FAILURE_(e.what());
    }

    rt_deinit_gc();
}

TEST(Interpreter, begin_returns_last_form) {
    rt_init_gc(kGCModeInterpreterOwned);

    auto form = rt_make_pair(rt_make_symbol("begin"),
                             rt_make_pair(rt_make_integer(1234),
                                          rt_make_pair(rt_make_integer(5678), NIL_PTR)));

    auto interp = Interpreter();

    try {
        auto result = interp.evalExpr(form, nullptr);
        EXPECT_TRUE(is_integer(result));
        EXPECT_EQ(TAG_TO_INTEGER(result), 5678);
    } catch (InterpreterException &e) {
        GTEST_FATAL_FAILURE_(e.what());
    }

    rt_deinit_gc();
}

TEST(Interpreter, runs_simple_lambda) {
    rt_init_gc(kGCModeInterpreterOwned);

    auto form = rt_make_pair(rt_make_pair(rt_make_symbol("lambda"),
                                          rt_make_pair(rt_make_pair(rt_make_symbol("x"), NIL_PTR),
                                                       rt_make_pair(rt_make_symbol("x"), NIL_PTR))),
                             rt_make_pair(rt_make_integer(1234), NIL_PTR));

    auto interp = Interpreter();
    try {
        auto result = interp.evalExpr(form);
        EXPECT_TRUE(is_integer(result));
        EXPECT_EQ(TAG_TO_INTEGER(result), 1234);
    } catch (InterpreterException &e) {
        GTEST_FATAL_FAILURE_(e.what());
    }

    rt_deinit_gc();
}


TEST(Interpreter, define_stores_value) {
    rt_init_gc(kGCModeInterpreterOwned);

    auto def = rt_make_pair(rt_make_symbol("define"),
            rt_make_pair(rt_make_symbol("a"),
            rt_make_pair(rt_make_integer(1234), NIL_PTR)));

    auto interp = Interpreter();

    try {
        interp.evalExpr(def);

        auto result = interp.evalExpr(rt_make_symbol("a"));
        EXPECT_TRUE(is_integer(result));
        EXPECT_EQ(TAG_TO_INTEGER(result), 1234);
    } catch (InterpreterException &e) {
        GTEST_FATAL_FAILURE_(e.what());
    }

    rt_deinit_gc();
}

/*
TEST(Interpreter, unbound_symbol_lookup_throws_exception) {
    auto interp = Interpreter();

    EXPECT_THROW(interp.evalExpr(make_symbol("a")), InterpreterException);
}

TEST(Interpreter, do_executes_body_returns_last) {
    auto expr1 = make_list({make_symbol("define"),
                            make_symbol("a"),
                            make_integer(1234)});
    auto expr2 = make_integer(2345);
    auto doForm = make_list({make_symbol("do"), expr1, expr2});

    auto interp = Interpreter();
    auto result = interp.evalExpr(doForm);

    EXPECT_EQ(result->tag, kTypeTagInteger);
    EXPECT_EQ(result->integerValue, 2345);

    auto defResult = interp.evalExpr(make_symbol("a"));
    EXPECT_EQ(defResult->tag, kTypeTagInteger);
    EXPECT_EQ(defResult->integerValue, 1234);
}
*/