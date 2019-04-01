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
#include <exception>

using namespace electrum;

TEST(Compiler, compilesConstantInteger) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("1234");

    EXPECT_EQ(rt_is_integer(result), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result), 1234);

    rt_deinit_gc();
}

TEST(Compiler, compilesConstantFloat) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("1234.5678");

    EXPECT_EQ(rt_is_float(result), TRUE_PTR);
    EXPECT_FLOAT_EQ(rt_float_value(result), 1234.5678);

    rt_deinit_gc();
}

TEST(Compiler, compilesConstantBoolean) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("#t");

    EXPECT_EQ(rt_is_boolean(result), TRUE_PTR);
    EXPECT_EQ(result, TRUE_PTR);

    auto result2 = c.compileAndEvalString("#f");
    EXPECT_EQ(rt_is_boolean(result2), TRUE_PTR);
    EXPECT_EQ(result2, FALSE_PTR);

    rt_deinit_gc();
}

TEST(Compiler, compilesConstantString) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("\"foo\"");

    EXPECT_EQ(rt_is_string(result), TRUE_PTR);
    EXPECT_STREQ(rt_string_value(result), "foo");

    rt_deinit_gc();
}

TEST(Compiler, compilesConstantNil) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("nil");

    EXPECT_EQ(result, NIL_PTR);

    rt_deinit_gc();
}

TEST(Compiler, compilesIf) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("(if #t 1234 5678)");

    EXPECT_EQ(rt_is_integer(result), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result), 1234);

    auto result2 = c.compileAndEvalString("(if #f 1234 5678)");
    EXPECT_EQ(rt_is_integer(result2), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result2), 5678);

    rt_deinit_gc();
}

TEST(Compiler, compilesDo) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("(do 123 456 789)");

    EXPECT_EQ(rt_is_integer(result), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result), 789);

    rt_deinit_gc();
}

TEST(Compiler, compilesDef) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("(do (def a 1234) a)");

    EXPECT_EQ(rt_is_integer(result), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result), 1234);

    c.compileAndEvalString("(def b \"foo\")");
    auto result2 = c.compileAndEvalString("b");

    EXPECT_EQ(rt_is_string(result2), TRUE_PTR);
    EXPECT_STREQ(rt_string_value(result2), "foo");

    rt_deinit_gc();
}

TEST(Compiler, compilesLambda) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    c.compileAndEvalString("(lambda (x) x)");

    rt_deinit_gc();
}

TEST(Compiler, compilesBasicLambda) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto def = c.compileAndEvalString("(def l (lambda (x) x))");
    auto result1 = c.compileAndEvalString("(l 1234)");
    auto result2 = c.compileAndEvalString("(l 5.678)");

    EXPECT_EQ(rt_is_integer(result1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result1), 1234);

    EXPECT_EQ(rt_is_float(result2), TRUE_PTR);
    EXPECT_FLOAT_EQ(rt_float_value(result2), 5.678);
    rt_deinit_gc();
}

TEST(Compiler, compilesLambdaWithBranch) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto def = c.compileAndEvalString("(def b (lambda (x) (if x 1234 5.678)))");
    auto result1 = c.compileAndEvalString("(b #t)");
    auto result2 = c.compileAndEvalString("(b #f)");

    EXPECT_EQ(rt_is_integer(result1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result1), 1234);

    EXPECT_EQ(rt_is_float(result2), TRUE_PTR);
    EXPECT_FLOAT_EQ(rt_float_value(result2), 5.678);
    rt_deinit_gc();
}

TEST(Compiler, compilesLambdaWithCapturedEnvironment) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("(((lambda (x) (lambda () x)) 1234))");
    EXPECT_EQ(rt_is_integer(result), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result), 1234);

    rt_deinit_gc();
}

TEST(Compiler, compilesDefFFIFn) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    c.compileAndEvalString("(def-ffi-fn* cons rt_make_pair :el (:el :el))");
    auto result = c.compileAndEvalString("(cons 1234 nil)");

    EXPECT_EQ(rt_is_pair(result), TRUE_PTR);

    auto car = rt_car(result);

    EXPECT_EQ(rt_is_integer(car), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(car), 1234);

    c.compileAndEvalString("(def-ffi-fn* car rt_car :el (:el))");
    auto result2 = c.compileAndEvalString("(car (cons 5678 nil))");

    EXPECT_EQ(rt_is_integer(result2), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(result2), 5678);

    rt_deinit_gc();
}

TEST(Compiler, compilesQuotedSymbol) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("'foo");

    EXPECT_EQ(rt_is_symbol(result), TRUE_PTR);
    EXPECT_TRUE(symbol_equal(result, rt_make_symbol("foo")));

    rt_deinit_gc();
}

TEST(Compiler, compilesQuotedList) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("'(1 2 a)");
    EXPECT_EQ(rt_is_pair(result), TRUE_PTR);

    auto i1 = rt_car(result);
    auto i2 = rt_car(rt_cdr(result));
    auto i3 = rt_car(rt_cdr(rt_cdr(result)));

    EXPECT_EQ(rt_is_integer(i1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(i1), 1);

    EXPECT_EQ(rt_is_integer(i2), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(i2), 2);

    EXPECT_EQ(rt_is_symbol(i3), TRUE_PTR);
    EXPECT_TRUE(symbol_equal(i3, rt_make_symbol("a")));

    rt_deinit_gc();
}

TEST(Compiler, compilesQuotedEmptyList) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("'()");
    EXPECT_EQ(result, NIL_PTR);

    rt_deinit_gc();
}

TEST(Compiler, compilesQuotedQuote) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("'('a)");
    EXPECT_EQ(rt_is_pair(result), TRUE_PTR);

    auto car = rt_car(result);
    EXPECT_EQ(rt_is_pair(car), TRUE_PTR);

    auto q = rt_car(car);
    EXPECT_EQ(rt_is_symbol(q), TRUE_PTR);
    EXPECT_TRUE(symbol_equal(q, rt_make_symbol("quote")));

    rt_deinit_gc();
}

TEST(Compiler, compilesQuoteInLambda) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("((lambda (a b c) `(,a ,b ,c)) 1 2 3)");

    // Expected result: '(1 2 3)
    EXPECT_EQ(rt_is_pair(result), TRUE_PTR);

    auto e1 = rt_car(result);
    EXPECT_EQ(rt_is_integer(e1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(e1), 1);

    auto e2 = rt_car(rt_cdr(result));
    EXPECT_EQ(rt_is_integer(e2), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(e2), 2);

    auto e3 = rt_car(rt_cdr(rt_cdr(result)));
    EXPECT_EQ(rt_is_integer(e3), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(e3), 3);

    rt_deinit_gc();
}

TEST(Compiler, compilesSimpleMacro) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto result = c.compileAndEvalString("(do "
                                            "  (def-ffi-fn* cons rt_make_pair :el (:el :el))"
                                            "  (defmacro make-list (x y z) `(cons ,x (cons ,y (cons ,z nil))))"
                                            "  (make-list 1 2 3))");

    // Should result in (1 2 3)

    EXPECT_EQ(rt_is_pair(result), TRUE_PTR);

    auto e1 = rt_car(result);
    auto e2 = rt_car(rt_cdr(result));
    auto e3 = rt_car(rt_cdr(rt_cdr(result)));

    EXPECT_EQ(rt_is_integer(e1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(e1), 1);

    EXPECT_EQ(rt_is_integer(e2), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(e2), 2);

    EXPECT_EQ(rt_is_integer(e3), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(e3), 3);

    rt_deinit_gc();
}

TEST(Compiler, compilesLambdaWithRestArgs) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    c.compileAndEvalString("(do "
                              "  (def-ffi-fn* car rt_car :el (:el))"
                              "  (def-ffi-fn* cdr rt_cdr :el (:el)))");

    auto r1 = c.compileAndEvalString("((lambda (x & rest) x) 1 2 3)");
    auto r2 = c.compileAndEvalString("((lambda (x & rest) (car rest)) 1 2 3)");
    auto r3 = c.compileAndEvalString("((lambda (x & rest) (car (cdr rest))) 1 2 3)");

    EXPECT_EQ(rt_is_integer(r1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r1), 1);

    EXPECT_EQ(rt_is_integer(r2), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r2), 2);

    EXPECT_EQ(rt_is_integer(r3), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r3), 3);

    rt_deinit_gc();
}

TEST(Compiler, compilesArithmeticAdd) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    c.compileAndEvalString("(def-ffi-fn* + rt_add :el (:el :el))");

    auto r1 = c.compileAndEvalString("(+ 1   2  )");
    auto r2 = c.compileAndEvalString("(+ 1   2.0)");
    auto r3 = c.compileAndEvalString("(+ 1.0 2  )");

    EXPECT_EQ(rt_is_integer(r1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r1), 3);

    EXPECT_EQ(rt_is_float(r2), TRUE_PTR);
    EXPECT_DOUBLE_EQ(rt_float_value(r2), 3.0);

    EXPECT_EQ(rt_is_float(r3), TRUE_PTR);
    EXPECT_DOUBLE_EQ(rt_float_value(r3), 3.0);

    rt_deinit_gc();
}

TEST(Compiler, compilesArithmeticSub) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    c.compileAndEvalString("(def-ffi-fn* - rt_sub :el (:el :el))");

    auto r1 = c.compileAndEvalString("(- 2 1)");
    auto r2 = c.compileAndEvalString("(- 2.0 1)");
    auto r3 = c.compileAndEvalString("(- 2 1.0)");

    EXPECT_EQ(rt_is_integer(r1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r1), 1);

    EXPECT_EQ(rt_is_float(r2), TRUE_PTR);
    EXPECT_DOUBLE_EQ(rt_float_value(r2), 1.0);

    EXPECT_EQ(rt_is_float(r3), TRUE_PTR);
    EXPECT_DOUBLE_EQ(rt_float_value(r3), 1.0);

    rt_deinit_gc();
}

TEST(Compiler, compilesArithmeticMul) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    c.compileAndEvalString("(def-ffi-fn* * rt_mul :el (:el :el))");

    auto r1 = c.compileAndEvalString("(* 2 3)");
    auto r2 = c.compileAndEvalString("(* 2.0 3)");
    auto r3 = c.compileAndEvalString("(* 2 3.0)");

    EXPECT_EQ(rt_is_integer(r1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r1), 6);

    EXPECT_EQ(rt_is_float(r2), TRUE_PTR);
    EXPECT_DOUBLE_EQ(rt_float_value(r2), 6.0);

    EXPECT_EQ(rt_is_float(r3), TRUE_PTR);
    EXPECT_DOUBLE_EQ(rt_float_value(r3), 6.0);

    rt_deinit_gc();
}

TEST(Compiler, compilesArithmeticDiv) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    c.compileAndEvalString("(def-ffi-fn* / rt_div :el (:el :el))");

    auto r1 = c.compileAndEvalString("(/ 8 2)");
    auto r2 = c.compileAndEvalString("(/ 3.0 2)");
    auto r3 = c.compileAndEvalString("(/ 3 2.0)");

    EXPECT_EQ(rt_is_integer(r1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r1), 4);

    EXPECT_EQ(rt_is_float(r2), TRUE_PTR);
    EXPECT_DOUBLE_EQ(rt_float_value(r2), 1.5);

    EXPECT_EQ(rt_is_float(r3), TRUE_PTR);
    EXPECT_DOUBLE_EQ(rt_float_value(r3), 1.5);

    rt_deinit_gc();
}

TEST(Compiler, tryNoThrowReturnsResult) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto r1 = c.compileAndEvalString("  (try"
                                     "    ((lambda () 1234))"
                                     "    (catch (fakeerror e)"
                                     "      5678))");

    EXPECT_EQ(rt_is_integer(r1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r1), 1234);

    rt_deinit_gc();
}

TEST(Compiler, tryThrowReturnsCatchResult) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    c.compileAndEvalString("(def-ffi-fn* throw el_rt_throw :el (:el))");
    c.compileAndEvalString("(def-ffi-fn* exception el_rt_make_exception :el (:el :el :el))");
    auto r1 = c.compileAndEvalString("  (try"
                                     "    (throw (exception 'fooerror \"A foo error\" nil))"
                                     "    1234"
                                     "    (catch (fooerror e)"
                                     "      5678))");

    EXPECT_EQ(rt_is_integer(r1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r1), 5678);

    rt_deinit_gc();
}

TEST(Compiler, tryThrowRunsCorrectCatchBlock) {
    rt_init_gc(kGCModeInterpreterOwned);

    Compiler c;
    auto r1 = c.compileAndEvalString("  (try"
                                     "    (throw 'a)"
                                     "    1234"
                                     "    (catch (a e)"
                                     "      1)"
                                     "    (catch (b e)"
                                     "      2)"
                                     "    (catch (c e)"
                                     "      3))");

    auto r2 = c.compileAndEvalString("  (try"
                                     "    (throw 'b)"
                                     "    1234"
                                     "    (catch (a e)"
                                     "      1)"
                                     "    (catch (b e)"
                                     "      2)"
                                     "    (catch (c e)"
                                     "      3))");

    auto r3 = c.compileAndEvalString("  (try"
                                     "    (throw 'c)"
                                     "    1234"
                                     "    (catch (a e)"
                                     "      1)"
                                     "    (catch (b e)"
                                     "      2)"
                                     "    (catch (c e)"
                                     "      3))");

    EXPECT_EQ(rt_is_integer(r1), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r1), 1);

    EXPECT_EQ(rt_is_integer(r2), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r2), 2);

    EXPECT_EQ(rt_is_integer(r3), TRUE_PTR);
    EXPECT_EQ(rt_integer_value(r3), 3);
    rt_deinit_gc();
}