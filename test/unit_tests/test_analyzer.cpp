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
#include "compiler/Analyzer.h"

using namespace electrum;

#define PARSE_STRING(s) Parser p; auto val = p.readString((s))

TEST(Analyzer, analyzesConstantInteger) {
    PARSE_STRING("1234");

    Analyzer an;
    auto node = an.analyzeForm(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeInteger);
    EXPECT_EQ(boost::get<int64_t>(constantNode->value), 1234);
}

TEST(Analyzer, analyzesConstantFloat) {
    PARSE_STRING("1234.5678");

    Analyzer an;
    auto node = an.analyzeForm(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeFloat);
    EXPECT_FLOAT_EQ(boost::get<double>(constantNode->value), 1234.5678);
}

TEST(Analyzer, analyzesConstantBooleanTrue) {
    PARSE_STRING("#t");

    Analyzer an;
    auto node = an.analyzeForm(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeBoolean);
    EXPECT_EQ(boost::get<bool>(constantNode->value), true);
}

TEST(Analyzer, analyzesConstantBooleanFalse) {
    PARSE_STRING("#f");

    Analyzer an;
    auto node = an.analyzeForm(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeBoolean);
    EXPECT_EQ(boost::get<bool>(constantNode->value), false);
}

TEST(Analyzer, analyzesConstantString) {
    PARSE_STRING("\"foo\"");

    Analyzer an;
    auto node = an.analyzeForm(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeString);
    EXPECT_EQ(*boost::get<std::shared_ptr<std::string>>(constantNode->value), "foo");
}

TEST(Analyzer, analyzesConstantKeyword) {
    PARSE_STRING(":foo");

    Analyzer an;
    auto node = an.analyzeForm(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeKeyword);
    EXPECT_EQ(*boost::get<std::shared_ptr<std::string>>(constantNode->value), "foo");
}

TEST(Analyzer, analyzesConstantNil) {
    PARSE_STRING("nil");

    Analyzer an;
    auto node = an.analyzeForm(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeNil);
}

TEST(Analyzer, analyzesDo) {
    PARSE_STRING("(do 123 456 789)");

    Analyzer an;
    auto node = an.analyzeForm(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeDo);
    auto doNode = std::dynamic_pointer_cast<DoAnalyzerNode>(node);

    EXPECT_EQ(doNode->children().size(), 3);

    EXPECT_EQ(doNode->statements.size(), 2);
    EXPECT_EQ(doNode->statements[0]->nodeType(), kAnalyzerNodeTypeConstant);
    EXPECT_EQ(doNode->statements[1]->nodeType(), kAnalyzerNodeTypeConstant);
    EXPECT_EQ(doNode->returnValue->nodeType(), kAnalyzerNodeTypeConstant);
}

TEST(Analyzer, analyzesLambda) {
    PARSE_STRING("(lambda (x) 1234)");

    Analyzer an;
    auto node = an.analyzeForm(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeLambda);
    auto lambdaNode = std::dynamic_pointer_cast<LambdaAnalyzerNode>(node);

    EXPECT_EQ(lambdaNode->arg_names.size(), 1);
    EXPECT_EQ(lambdaNode->arg_name_nodes.at(0)->nodeType(), kAnalyzerNodeTypeConstant);

    auto arg1 = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(lambdaNode->arg_name_nodes.at(0));
    EXPECT_EQ(arg1->type, kAnalyzerConstantTypeSymbol);
    EXPECT_EQ(*boost::get<std::shared_ptr<std::string>>(arg1->value), "x");

    EXPECT_EQ(lambdaNode->body->nodeType(), kAnalyzerNodeTypeDo);

    auto body = std::dynamic_pointer_cast<DoAnalyzerNode>(lambdaNode->body);
    EXPECT_EQ(body->returnValue->nodeType(), kAnalyzerNodeTypeConstant);

    auto rv = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(body->returnValue);

    EXPECT_EQ(rv->type, kAnalyzerConstantTypeInteger);
    EXPECT_EQ(boost::get<int64_t>(rv->value), 1234);
}

TEST(Analyzer, analyzesInvoke) {
    PARSE_STRING("((lambda (x y) 42) 1 2)");
    Analyzer an;
    auto node = an.analyzeForm(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeMaybeInvoke);
    auto invokeNode = std::dynamic_pointer_cast<MaybeInvokeAnalyzerNode>(node);

    EXPECT_EQ(invokeNode->args.size(), 2);

    std::vector<int64_t> expected = { 1, 2 };
    int i = 0;
    for(auto a: invokeNode->args) {
        EXPECT_EQ(a->nodeType(), kAnalyzerNodeTypeConstant);
        auto cNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(a);
        EXPECT_EQ(cNode->type, kAnalyzerConstantTypeInteger);
        EXPECT_EQ(boost::get<int64_t>(cNode->value), expected[i]);
        ++i;
    }
}

TEST(Analyzer, analyzesDefAndSavesToAnalyzerEnvironment) {
    PARSE_STRING("(def a 1234)");

    Analyzer an;
    auto node = an.analyzeForm(val);

    auto envVal = an.initialBindingWithName("a");
    EXPECT_NE(envVal, nullptr);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeDef);

    auto defNode = std::dynamic_pointer_cast<DefAnalyzerNode>(node);
    auto defVal = defNode->value;

    EXPECT_EQ(defVal, envVal);

    auto constNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(envVal);
    EXPECT_EQ(constNode->type, kAnalyzerConstantTypeInteger);
    EXPECT_EQ(boost::get<int64_t>(constNode->value), 1234);
}

TEST(Analyzer, collectsClosedOversInLambda) {
    PARSE_STRING("(lambda (a) (lambda () a))");

    Analyzer an;
    auto node = an.analyzeForm(val);
    an.closedOversForNode(node);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeLambda);
    auto outerLambda = std::dynamic_pointer_cast<LambdaAnalyzerNode>(node);

    EXPECT_EQ(outerLambda->arg_name_nodes.size(), 1);
    EXPECT_EQ(outerLambda->closed_overs.size(), 0);
    
    EXPECT_EQ(outerLambda->body->nodeType(), kAnalyzerNodeTypeDo);
    
    auto body = std::dynamic_pointer_cast<DoAnalyzerNode>(outerLambda->body);
    EXPECT_EQ(body->returnValue->nodeType(), kAnalyzerNodeTypeLambda);
    auto innerLambda = std::dynamic_pointer_cast<LambdaAnalyzerNode>(body->returnValue);

    EXPECT_EQ(innerLambda->closed_overs.size(), 1);
    EXPECT_EQ(innerLambda->closed_overs[0], *outerLambda->arg_names[0]);
}

TEST(Analyzer, doesNotTreatGlobalVarAsClosedOver) {
    PARSE_STRING("(do (def a 1) (lambda () a))");

    Analyzer an;
    auto node = an.analyzeForm(val);
    an.closedOversForNode(node);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeDo);
    auto doNode = std::dynamic_pointer_cast<DoAnalyzerNode>(node);

    EXPECT_EQ(doNode->returnValue->nodeType(), kAnalyzerNodeTypeLambda);
    auto lambdaNode = std::dynamic_pointer_cast<LambdaAnalyzerNode>(doNode->returnValue);

    EXPECT_EQ(lambdaNode->closed_overs.size(), 0);
}

TEST(Analyzer, analyzesDefFFIFn) {
    PARSE_STRING("(def-ffi-fn* binding c_func :el (:el :el))");
    
    Analyzer an;
    auto node = an.analyzeForm(val);
    
    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeDefFFIFunction);
    auto ffiNode = std::dynamic_pointer_cast<DefFFIFunctionNode>(node);

    EXPECT_EQ(*ffiNode->binding, "binding");
    EXPECT_EQ(*ffiNode->func_name, "c_func");

    EXPECT_EQ(ffiNode->return_type, kFFITypeElectrumValue);

    EXPECT_EQ(ffiNode->arg_types.size(), 2);
    EXPECT_EQ(ffiNode->arg_types[0], kFFITypeElectrumValue);
    EXPECT_EQ(ffiNode->arg_types[1], kFFITypeElectrumValue);
}
