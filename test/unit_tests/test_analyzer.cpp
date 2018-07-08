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