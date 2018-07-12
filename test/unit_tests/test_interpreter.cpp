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

using namespace electrum;
using std::shared_ptr;
using std::make_shared;

#define MAKE_BOOLEAN(b) make_shared

shared_ptr<ASTNode> make_boolean(bool value) {
    auto b = make_shared<ASTNode>();
    b->tag = kTypeTagBoolean;
    b->booleanValue = value;
    return b;
}

shared_ptr<ASTNode> make_symbol(std::string name) {
    auto s = make_shared<ASTNode>();
    s->tag = kTypeTagSymbol;
    s->stringValue = make_shared<std::string>(name);
    return s;
}

shared_ptr<ASTNode> make_integer(uint64_t val) {
    auto i = make_shared<ASTNode>();
    i->tag = kTypeTagInteger;
    i->integerValue = val;
    return i;
}

shared_ptr<ASTNode> make_list(std::vector<shared_ptr<ASTNode>> listVal) {
    auto l = make_shared<ASTNode>();
    l->tag = kTypeTagList;
    l->listValue = make_shared<std::vector<shared_ptr<ASTNode>>>(listVal);
    return l;
}

TEST(Interpreter, if_evaluates_correct_expressions) {
    auto cond1 = make_boolean(true);
    auto cond2 = make_boolean(false);

    auto subsequent = make_integer(1234);
    auto alternative = make_integer(5678);

    auto if1 = make_list({make_symbol("if"), cond1, subsequent, alternative});
    auto if2 = make_list({make_symbol("if"), cond2, subsequent, alternative});

    auto interp = Interpreter();

    auto result1 = interp.evalExpr(if1);
    auto result2 = interp.evalExpr(if2);

    EXPECT_EQ(result1->tag, kTypeTagInteger);
    EXPECT_EQ(result1->integerValue, 1234);

    EXPECT_EQ(result2->tag, kTypeTagInteger);
    EXPECT_EQ(result2->integerValue, 5678);
}

TEST(Interpreter, define_stores_value) {
    auto val = make_integer(1234);
    auto def = make_list({make_symbol("define"), make_symbol("a"), val});

    auto interp = Interpreter();
    interp.evalExpr(def);

    auto result = interp.evalExpr(make_symbol("a"));
    EXPECT_EQ(result->tag, kTypeTagInteger);
    EXPECT_EQ(result->integerValue, 1234);
}


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
