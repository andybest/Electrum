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


#include <memory>
#include "Analyzer.h"
#include "CompilerExceptions.h"

namespace electrum {

    Analyzer::Analyzer() {
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeForm(const shared_ptr<ASTNode> form) {

        switch (form->tag) {
            case kTypeTagInteger: return analyzeInteger(form);
            case kTypeTagFloat: return analyzeFloat(form);
            case kTypeTagString: return analyzeString(form);
            case kTypeTagList: return analyzeList(form);


        }

        return shared_ptr<AnalyzerNode>();
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeInteger(const shared_ptr<ASTNode> form) {
        auto node = make_shared<ConstantValueAnalyzerNode>();
        node->type = kAnalyzerConstantTypeInteger;
        node->value = form->integerValue;
        node->sourcePosition = form->sourcePosition;
        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeFloat(const shared_ptr<ASTNode> form) {
        auto node = make_shared<ConstantValueAnalyzerNode>();
        node->type = kAnalyzerConstantTypeFloat;
        node->value = form->floatValue;
        node->sourcePosition = form->sourcePosition;
        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeBoolean(const shared_ptr<ASTNode> form) {
        auto node = make_shared<ConstantValueAnalyzerNode>();
        node->type = kAnalyzerConstantTypeBoolean;
        // TODO: Implement boolean
        node->sourcePosition = form->sourcePosition;
        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeString(const shared_ptr<ASTNode> form) {
        auto node = make_shared<ConstantValueAnalyzerNode>();
        node->type = kAnalyzerConstantTypeString;
        node->value = form->stringValue;
        node->sourcePosition = form->sourcePosition;
        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeList(const shared_ptr<ASTNode> form) {
        auto listPtr = form->listValue;
        auto listSize = listPtr->size();

        if (listSize > 0) {
            auto firstItemForm = listPtr->at(0);

            // Check for special form
            if (firstItemForm->tag == kTypeTagSymbol) {
                // Check for special form
                auto maybeSpecial = maybeAnalyzeSpecialForm(firstItemForm->stringValue, form);
                if (maybeSpecial) {
                    return maybeSpecial;
                } else {
                    // The node isn't a special form, so it might be a function call.
                    return analyzeMaybeFunctionCall(form);
                }
            }
        }

        // The list isn't a special form or a function call.
        throw std::exception(); // TODO: Replace with proper exception
    }

    shared_ptr<AnalyzerNode> Analyzer::maybeAnalyzeSpecialForm(const shared_ptr<string> symbolName,
                                                               const shared_ptr<ASTNode> form) {
        assert(form->tag == kTypeTagList);
        auto listPtr = form->listValue;
        assert(!listPtr->empty());

        // If an analysis function exists for the symbol, return the result
        auto analyseFunc = specialForms.find(*symbolName);
        if (analyseFunc != specialForms.end()) {
            auto func = analyseFunc->second;
            return (this->*func)(form);
        }

        return nullptr;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeMaybeFunctionCall(const shared_ptr<ASTNode> form) {
        return nullptr;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeIf(const shared_ptr<ASTNode> form) {
        assert(form->tag == kTypeTagList);
        auto listPtr = form->listValue;
        assert(!listPtr->empty());

        // Check list has at least a condition and consequent
        if (listPtr->size() < 3) {
            throw CompilerException("if form requires at least a condition and a consequent",
                                    form->sourcePosition);
        }

        auto conditionForm = listPtr->at(1);
        auto consequentForm = listPtr->at(2);

        auto node = make_shared<IfAnalyzerNode>();
        node->condition = analyzeForm(conditionForm);
        node->consequent = analyzeForm(consequentForm);

        if (listPtr->size() > 4) {
            // The list has more elements than is expected.
            throw CompilerException("if form must have a maximum of 3 statements",
                                    listPtr->at(4)->sourcePosition);
        } else if (listPtr->size() > 3) {
            auto alternativeForm = listPtr->at(3);
            node->alternative = analyzeForm(alternativeForm);
        }

        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeDo(const shared_ptr<ASTNode> form) {
        assert(form->tag == kTypeTagList);
        auto listPtr = form->listValue;
        assert(!listPtr->empty());

        if (listPtr->size() < 2) {
            // Do forms must have at least one form in the body
            throw CompilerException("Do forms must have at least one body statement",
                                    form->sourcePosition);
        }

        auto node = make_shared<DoAnalyzerNode>();

        // Add all but the last statements to 'statements'
        for (auto it = listPtr->begin() + 1; it < listPtr->end() - 1; ++it) {
            node->statements.push_back(analyzeForm(*it));
        }

        // Add the last statement to 'returnValue'
        node->returnValue = analyzeForm(*(listPtr->end() - 1));

        return node;
    }

}