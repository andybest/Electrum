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

#define PARSE_STRING(s) Parser p; auto val = p.readString(s, "")

TEST(Analyzer, analyzesConstantInteger) {
    PARSE_STRING("1234");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeInteger);
    EXPECT_EQ(boost::get<int64_t>(constantNode->value), 1234);
}

TEST(Analyzer, analyzesConstantFloat) {
    PARSE_STRING("1234.5678");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeFloat);
    EXPECT_FLOAT_EQ(boost::get<double>(constantNode->value), 1234.5678);
}

TEST(Analyzer, analyzesConstantBooleanTrue) {
    PARSE_STRING("#t");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeBoolean);
    EXPECT_EQ(boost::get<bool>(constantNode->value), true);
}

TEST(Analyzer, analyzesConstantBooleanFalse) {
    PARSE_STRING("#f");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeBoolean);
    EXPECT_EQ(boost::get<bool>(constantNode->value), false);
}

TEST(Analyzer, analyzesConstantString) {
    PARSE_STRING("\"foo\"");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeString);
    EXPECT_EQ(*boost::get<std::shared_ptr<std::string>>(constantNode->value), "foo");
}

TEST(Analyzer, analyzesConstantKeyword) {
    PARSE_STRING(":foo");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeKeyword);
    EXPECT_EQ(*boost::get<std::shared_ptr<std::string>>(constantNode->value), "foo");
}

TEST(Analyzer, analyzesConstantNil) {
    PARSE_STRING("nil");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstant);
    auto constantNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(node);
    EXPECT_EQ(constantNode->type, kAnalyzerConstantTypeNil);
}

TEST(Analyzer, analyzesDo) {
    PARSE_STRING("(do 123 456 789)");

    Analyzer an;
    auto node = an.analyze(val);

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
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeLambda);
    auto lambdaNode = std::dynamic_pointer_cast<LambdaAnalyzerNode>(node);

    EXPECT_EQ(lambdaNode->arg_names.size(), 1);
    EXPECT_EQ(lambdaNode->arg_name_nodes.at(0)->nodeType(), kAnalyzerNodeTypeConstant);

    EXPECT_EQ(lambdaNode->has_rest_arg, false);

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

TEST(Analyzer, analyzesLambdaWithRestArgs) {
    PARSE_STRING("(lambda (x & y) y)");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeLambda);
    auto lambdaNode = std::dynamic_pointer_cast<LambdaAnalyzerNode>(node);

    EXPECT_EQ(lambdaNode->arg_names.size(), 1);
    EXPECT_EQ(lambdaNode->arg_name_nodes.at(0)->nodeType(), kAnalyzerNodeTypeConstant);

    auto arg1 = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(lambdaNode->arg_name_nodes.at(0));
    EXPECT_EQ(arg1->type, kAnalyzerConstantTypeSymbol);
    EXPECT_EQ(*boost::get<std::shared_ptr<std::string>>(arg1->value), "x");

    EXPECT_EQ(lambdaNode->has_rest_arg, true);
    EXPECT_EQ(*lambdaNode->rest_arg_name, "y");
}

TEST(Analyzer, analyzesInvoke) {
    PARSE_STRING("((lambda (x y) 42) 1 2)");
    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeMaybeInvoke);
    auto invokeNode = std::dynamic_pointer_cast<MaybeInvokeAnalyzerNode>(node);

    EXPECT_EQ(invokeNode->args.size(), 2);

    std::vector<int64_t> expected = {1, 2};
    int i = 0;
    for (auto a: invokeNode->args) {
        EXPECT_EQ(a->nodeType(), kAnalyzerNodeTypeConstant);
        auto cNode = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(a);
        EXPECT_EQ(cNode->type, kAnalyzerConstantTypeInteger);
        EXPECT_EQ(boost::get<int64_t>(cNode->value), expected[i]);
        ++i;
    }
}

TEST(Analyzer, collectsClosedOversInLambda) {
    PARSE_STRING("(lambda (a & b) (lambda () a))");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeLambda);
    auto outerLambda = std::dynamic_pointer_cast<LambdaAnalyzerNode>(node);

    EXPECT_EQ(outerLambda->closed_overs.size(), 0);

    EXPECT_EQ(outerLambda->body->nodeType(), kAnalyzerNodeTypeDo);

    auto body = std::dynamic_pointer_cast<DoAnalyzerNode>(outerLambda->body);
    EXPECT_EQ(body->returnValue->nodeType(), kAnalyzerNodeTypeLambda);
    auto innerLambda = std::dynamic_pointer_cast<LambdaAnalyzerNode>(body->returnValue);

    EXPECT_EQ(innerLambda->closed_overs.size(), 1);
    EXPECT_EQ(innerLambda->closed_overs[0], *outerLambda->arg_names[0]);
}

TEST(Analyzer, doesNotTreatRestArgAsClosedOver) {
    PARSE_STRING("(lambda (a & b) b)");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeLambda);
    auto lambda = std::dynamic_pointer_cast<LambdaAnalyzerNode>(node);

    EXPECT_EQ(lambda->closed_overs.size(), 0);
}

TEST(Analyzer, doesNotTreatGlobalVarAsClosedOver) {
    PARSE_STRING("(do (def a 1) (lambda () a))");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeDo);
    auto doNode = std::dynamic_pointer_cast<DoAnalyzerNode>(node);

    EXPECT_EQ(doNode->returnValue->nodeType(), kAnalyzerNodeTypeLambda);
    auto lambdaNode = std::dynamic_pointer_cast<LambdaAnalyzerNode>(doNode->returnValue);

    EXPECT_EQ(lambdaNode->closed_overs.size(), 0);
}

TEST(Analyzer, analyzesDefFFIFn) {
    PARSE_STRING("(def-ffi-fn* binding c_func :el (:el :el))");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeDefFFIFunction);
    auto ffiNode = std::dynamic_pointer_cast<DefFFIFunctionNode>(node);

    EXPECT_EQ(*ffiNode->binding, "binding");
    EXPECT_EQ(*ffiNode->func_name, "c_func");

    EXPECT_EQ(ffiNode->return_type, kFFITypeElectrumValue);

    EXPECT_EQ(ffiNode->arg_types.size(), 2);
    EXPECT_EQ(ffiNode->arg_types[0], kFFITypeElectrumValue);
    EXPECT_EQ(ffiNode->arg_types[1], kFFITypeElectrumValue);
}

TEST(Analyzer, analyzesQuotedList) {
    PARSE_STRING("'(1 2 a)");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeConstantList);
    auto listNode = std::dynamic_pointer_cast<ConstantListAnalyzerNode>(node);
    EXPECT_EQ(listNode->values.size(), 3);

    EXPECT_EQ(listNode->values[0]->nodeType(), kAnalyzerNodeTypeConstant);
    auto v1 = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(listNode->values[0]);
    EXPECT_EQ(v1->type, kAnalyzerConstantTypeInteger);
    EXPECT_EQ(boost::get<int64_t>(v1->value), 1);

    EXPECT_EQ(listNode->values[1]->nodeType(), kAnalyzerNodeTypeConstant);
    auto v2 = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(listNode->values[1]);
    EXPECT_EQ(v2->type, kAnalyzerConstantTypeInteger);
    EXPECT_EQ(boost::get<int64_t>(v2->value), 2);

    EXPECT_EQ(listNode->values[2]->nodeType(), kAnalyzerNodeTypeConstant);
    auto v3 = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(listNode->values[2]);
    EXPECT_EQ(v3->type, kAnalyzerConstantTypeSymbol);
    EXPECT_EQ(*boost::get<shared_ptr<std::string>>(v3->value), "a");
}

TEST(Analyzer, analyzesQuasiQuoteInLambda) {
    PARSE_STRING("(lambda (a b c) `(,a ,b ,c))");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeLambda);
    auto lambdaNode = std::dynamic_pointer_cast<LambdaAnalyzerNode>(node);

    EXPECT_EQ(lambdaNode->body->nodeType(), kAnalyzerNodeTypeDo);
    auto doBodyNode = std::dynamic_pointer_cast<DoAnalyzerNode>(lambdaNode->body);

    EXPECT_EQ(doBodyNode->returnValue->nodeType(), kAnalyzerNodeTypeConstantList);

    auto listNode = std::dynamic_pointer_cast<ConstantListAnalyzerNode>(doBodyNode->returnValue);
    EXPECT_EQ(listNode->values.size(), 3);

    EXPECT_EQ(listNode->values[0]->nodeType(), kAnalyzerNodeTypeVarLookup);
    EXPECT_EQ(listNode->values[1]->nodeType(), kAnalyzerNodeTypeVarLookup);
    EXPECT_EQ(listNode->values[2]->nodeType(), kAnalyzerNodeTypeVarLookup);
}

TEST(Analyzer, analyzesDefMacro) {
    PARSE_STRING("(defmacro x (y) 'y)");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeDefMacro);
    auto defMacroNode = std::dynamic_pointer_cast<DefMacroAnalyzerNode>(node);

    EXPECT_EQ(*defMacroNode->name, "x");

    EXPECT_EQ(defMacroNode->arg_names.size(), 1);
    EXPECT_EQ(defMacroNode->arg_name_nodes.at(0)->nodeType(), kAnalyzerNodeTypeConstant);

    auto arg1 = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(defMacroNode->arg_name_nodes.at(0));
    EXPECT_EQ(arg1->type, kAnalyzerConstantTypeSymbol);
    EXPECT_EQ(*boost::get<std::shared_ptr<std::string>>(arg1->value), "y");

    EXPECT_EQ(defMacroNode->body->nodeType(), kAnalyzerNodeTypeDo);

    auto body = std::dynamic_pointer_cast<DoAnalyzerNode>(defMacroNode->body);
    EXPECT_EQ(body->returnValue->nodeType(), kAnalyzerNodeTypeConstant);

    auto sym = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(body->returnValue);
    EXPECT_EQ(*boost::get<shared_ptr<std::string>>(sym->value), "y");
}

TEST(Analyzer, analyzesMacroExpansion) {
    PARSE_STRING("(do (defmacro x (y) 'y) (x 1))");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeDo);
    auto doNode = std::dynamic_pointer_cast<DoAnalyzerNode>(node);

    EXPECT_EQ(doNode->returnValue->nodeType(), kAnalyzerNodeTypeMacroExpand);

    auto expandNode = std::dynamic_pointer_cast<MacroExpandAnalyzerNode>(doNode->returnValue);
    EXPECT_EQ(expandNode->args.size(), 1);

    EXPECT_EQ(expandNode->args[0]->nodeType(), kAnalyzerNodeTypeConstant);
    auto arg1 = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(expandNode->args[0]);

    EXPECT_EQ(arg1->type, kAnalyzerConstantTypeInteger);
    EXPECT_EQ(boost::get<int64_t>(arg1->value), 1);

    EXPECT_EQ(expandNode->macro->nodeType(), kAnalyzerNodeTypeDefMacro);
    auto macroNode = std::dynamic_pointer_cast<DefMacroAnalyzerNode>(expandNode->macro);

    EXPECT_EQ(*macroNode->name, "x");
    EXPECT_EQ(macroNode->arg_names.size(), 1);
}

TEST(Analyzer, doesNotEvaluateArguments) {
    PARSE_STRING("(do (defmacro infix (x op y) `(,op ,x ,y)) (infix 1 + 2))");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeDo);
    auto doNode = std::dynamic_pointer_cast<DoAnalyzerNode>(node);

    EXPECT_EQ(doNode->returnValue->nodeType(), kAnalyzerNodeTypeMacroExpand);

    auto expandNode = std::dynamic_pointer_cast<MacroExpandAnalyzerNode>(doNode->returnValue);
    EXPECT_EQ(expandNode->args.size(), 3);

    EXPECT_EQ(expandNode->args[0]->nodeType(), kAnalyzerNodeTypeConstant);
    auto arg1 = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(expandNode->args[0]);

    EXPECT_EQ(arg1->type, kAnalyzerConstantTypeInteger);
    EXPECT_EQ(boost::get<int64_t>(arg1->value), 1);

    EXPECT_EQ(expandNode->args[1]->nodeType(), kAnalyzerNodeTypeConstant);
    auto arg2 = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(expandNode->args[1]);

    EXPECT_EQ(arg2->type, kAnalyzerConstantTypeSymbol);
    EXPECT_EQ(*boost::get<shared_ptr<std::string>>(arg2->value), "+");

    EXPECT_EQ(expandNode->args[2]->nodeType(), kAnalyzerNodeTypeConstant);
    auto arg3 = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(expandNode->args[2]);

    EXPECT_EQ(arg3->type, kAnalyzerConstantTypeInteger);
    EXPECT_EQ(boost::get<int64_t>(arg3->value), 2);

    EXPECT_EQ(expandNode->macro->nodeType(), kAnalyzerNodeTypeDefMacro);
    auto macroNode = std::dynamic_pointer_cast<DefMacroAnalyzerNode>(expandNode->macro);

    EXPECT_EQ(*macroNode->name, "infix");
    EXPECT_EQ(macroNode->arg_names.size(), 3);
}

TEST(Analyzer, calculatesNodeDepth) {
    PARSE_STRING("(do (if #t (if #f 1 2) 3))");

    Analyzer an;
    auto node = an.analyze(val);

    auto doNode = std::dynamic_pointer_cast<DoAnalyzerNode>(node);
    EXPECT_EQ(doNode->node_depth, 0);

    auto ifNode1 = std::dynamic_pointer_cast<IfAnalyzerNode>(doNode->returnValue);
    // Do node should not affect node depth
    EXPECT_EQ(ifNode1->node_depth, 0);

    EXPECT_EQ(ifNode1->condition->node_depth, 1);
    EXPECT_EQ(ifNode1->consequent->node_depth, 1);
    EXPECT_EQ(ifNode1->alternative->node_depth, 1);

    auto ifNode2 = std::dynamic_pointer_cast<IfAnalyzerNode>(ifNode1->consequent);

    EXPECT_EQ(ifNode2->condition->node_depth, 2);
    EXPECT_EQ(ifNode2->consequent->node_depth, 2);
    EXPECT_EQ(ifNode2->alternative->node_depth, 2);
}

TEST(Analyzer, analyzesEvalWhen) {
    PARSE_STRING("(eval-when (:compile :load) (if #t 123 456))");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeEvalWhen);

    auto evalWhenNode = std::dynamic_pointer_cast<EvalWhenAnalyzerNode>(node);
    EXPECT_TRUE(evalWhenNode->phases & kEvaluationPhaseCompileTime);
    EXPECT_TRUE(evalWhenNode->phases & kEvaluationPhaseLoadTime);
}

TEST(Analyzer, analyzesEvalWhenCompile) {
    PARSE_STRING("(eval-when (:compile) (if #t 123 456))");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeEvalWhen);

    auto evalWhenNode = std::dynamic_pointer_cast<EvalWhenAnalyzerNode>(node);
    EXPECT_TRUE(evalWhenNode->phases & kEvaluationPhaseCompileTime);
    EXPECT_FALSE(evalWhenNode->phases & kEvaluationPhaseLoadTime);
}

TEST(Analyzer, analyzesEvalWhenLoad) {
    PARSE_STRING("(eval-when (:load) (if #t 123 456))");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeEvalWhen);

    auto evalWhenNode = std::dynamic_pointer_cast<EvalWhenAnalyzerNode>(node);
    EXPECT_FALSE(evalWhenNode->phases & kEvaluationPhaseCompileTime);
    EXPECT_TRUE(evalWhenNode->phases & kEvaluationPhaseLoadTime);
}

TEST(Analyzer, rejectsMacroReferringToGlobalSymbolNotVisibleToCompilationPhase) {
    PARSE_STRING("(do (eval-when (:load)"
                 "  (def test 1))"
                 "(defmacro foo () test))");

    Analyzer an;
    EXPECT_ANY_THROW(an.analyze(val));
}

TEST(Analyzer, doesNotRejectMacroReferringToGlobalSymbolWhenVisibleToCompilationPhase) {
    PARSE_STRING("(do (eval-when (:load :compile)"
                 "  (def test 1))"
                 "(defmacro foo () test))");

    Analyzer an;
    EXPECT_NO_THROW(an.analyze(val));
}

TEST(Analyzer, updatesEvaluationPhasesOfAllNodes) {
    PARSE_STRING("(eval-when (:load :compile)"
                 "  1234"
                 "  5.678)");

    Analyzer an;

    auto node = an.analyze(val);

    for (const auto& c: node->children()) {
        EXPECT_EQ(c->evaluation_phase, kEvaluationPhaseLoadTime | kEvaluationPhaseCompileTime);
    }
}

TEST(Analyzer, collapsesTopLevelForms) {
    PARSE_STRING("(do 1234 (do 5678 1.234) (eval-when (:compile) (def a 1)))");

    Analyzer an;

    auto node = an.analyze(val);
    auto nodes = an.collapseTopLevelForms(node);

    EXPECT_EQ(nodes.size(), 4);

    EXPECT_EQ(nodes[0]->nodeType(), kAnalyzerNodeTypeConstant);
    EXPECT_EQ(nodes[1]->nodeType(), kAnalyzerNodeTypeConstant);
    EXPECT_EQ(nodes[2]->nodeType(), kAnalyzerNodeTypeConstant);
    EXPECT_EQ(nodes[3]->nodeType(), kAnalyzerNodeTypeDef);

    EXPECT_EQ(nodes[0]->evaluation_phase, kEvaluationPhaseLoadTime);
    EXPECT_EQ(nodes[1]->evaluation_phase, kEvaluationPhaseLoadTime);
    EXPECT_EQ(nodes[2]->evaluation_phase, kEvaluationPhaseLoadTime);
    // Evaluation phase should have been added by another pass before
    // eval-when nodes are collapsed
    EXPECT_EQ(nodes[3]->evaluation_phase, kEvaluationPhaseCompileTime);
}

TEST(Analyzer, analyzesTryCatch) {
    PARSE_STRING("(try"
                 "  1234"
                 "  5678"
                 "  (catch (fooerror e)"
                 "    nil)"
                 "  (catch (barerror e)"
                 "    nil"
                 "    e))");

    Analyzer an;

    auto node = an.analyze(val);

    ASSERT_EQ(node->nodeType(), kAnalyzerNodeTypeTry);

    auto tryNode = std::dynamic_pointer_cast<TryAnalyzerNode>(node);

    ASSERT_EQ(tryNode->body.size(), 2);
    EXPECT_EQ(tryNode->body[0]->nodeType(), kAnalyzerNodeTypeConstant);
    EXPECT_EQ(tryNode->body[1]->nodeType(), kAnalyzerNodeTypeConstant);

    ASSERT_EQ(tryNode->catch_nodes.size(), 2);

    auto catch1 = tryNode->catch_nodes[0];
    auto catch2 = tryNode->catch_nodes[1];

    EXPECT_EQ(*catch1->exception_type, "fooerror");
    EXPECT_EQ(*catch2->exception_type, "barerror");

    EXPECT_EQ(*catch1->exception_binding, "e");
    EXPECT_EQ(*catch2->exception_binding, "e");

    EXPECT_EQ(catch1->body.size(), 1);
    EXPECT_EQ(catch2->body.size(), 2);
}

TEST(Analyzer, analyzesInNS) {
    PARSE_STRING("(in-ns 'foo)");

    Analyzer an;

    EXPECT_EQ(an.currentNamespace()->name, "el.user");

    an.analyze(val);

    EXPECT_EQ(an.currentNamespace()->name, "foo");
}

TEST(Analyzer, definitionFromOtherNSVisible) {
    PARSE_STRING("(do (in-ns 'foo)"
                 "    (def baz 1)"
                 "    (in-ns 'bar)"
                 "    foo/baz)");

    Analyzer an;

    EXPECT_NO_THROW(an.analyze(val));
}

TEST(Analyzer, analyzesMacroWithRestArgs) {
    PARSE_STRING("(defmacro foo (x & y) y)");

    Analyzer an;
    auto node = an.analyze(val);

    EXPECT_EQ(node->nodeType(), kAnalyzerNodeTypeDefMacro);
    auto macroNode = std::dynamic_pointer_cast<DefMacroAnalyzerNode>(node);

    EXPECT_EQ(macroNode->arg_names.size(), 1);
    EXPECT_EQ(macroNode->arg_name_nodes.at(0)->nodeType(), kAnalyzerNodeTypeConstant);

    auto arg1 = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(macroNode->arg_name_nodes.at(0));
    EXPECT_EQ(arg1->type, kAnalyzerConstantTypeSymbol);
    EXPECT_EQ(*boost::get<std::shared_ptr<std::string>>(arg1->value), "x");

    EXPECT_EQ(macroNode->has_rest_arg, true);
    EXPECT_EQ(*macroNode->rest_arg_name, "y");
}
