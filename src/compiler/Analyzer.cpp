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
            case kTypeTagBoolean: return analyzeBoolean(form);
            case kTypeTagString: return analyzeString(form);
            case kTypeTagKeyword: return analyzeKeyword(form);
            case kTypeTagSymbol: return analyzeSymbol(form);
            case kTypeTagList: return analyzeList(form);
        }

        return shared_ptr<AnalyzerNode>();
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeSymbol(shared_ptr<ASTNode> form) {
        auto symName = form->stringValue;

        auto globalResult = global_env_.find(*symName);
        if(globalResult != global_env_.end()) {
            auto node = make_shared<VarLookupNode>();
            node->sourcePosition = form->sourcePosition;
            node->name = form->stringValue;
            node->is_global = true;
            return node;
        }

        throw CompilerException("Unbound variable '" + *symName + "'",
                form->sourcePosition);
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
        node->value = form->booleanValue;
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

    shared_ptr<AnalyzerNode> Analyzer::analyzeKeyword(const shared_ptr<ASTNode> form) {
        auto node = make_shared<ConstantValueAnalyzerNode>();
        node->type = kAnalyzerConstantTypeKeyword;
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
                }
            }

            // The node isn't a special form, so it might be a function call.
            return analyzeMaybeInvoke(form);
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

    shared_ptr<AnalyzerNode> Analyzer::analyzeMaybeInvoke(const shared_ptr<ASTNode> form) {
        assert(form->tag == kTypeTagList);
        auto listPtr = form->listValue;
        assert(!listPtr->empty());

        auto node = std::make_shared<MaybeInvokeAnalyzerNode>();
        node->args.reserve(listPtr->size() - 1);

        bool first = true;
        for(auto a: *listPtr) {
            if(first) {
                node->fn = analyzeForm(listPtr->at(0));
                first = false;
            } else {
                node->args.push_back(analyzeForm(a));
            }
        }

        return node;
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

    shared_ptr<AnalyzerNode> Analyzer::analyzeLambda(const shared_ptr<ASTNode> form) {
        assert(form->tag == kTypeTagList);
        auto listPtr = form->listValue;
        assert(!listPtr->empty());

        if (listPtr->size() < 2) {
            // Lambda forms must contain an argument list
            throw CompilerException("Lambda forms must have an argument list",
                    form->sourcePosition);
        }

        if (listPtr->at(1)->tag != kTypeTagList) {
            // Lamda arguments must be a list
            throw CompilerException("Lambda arguments must be a list",
                    listPtr->at(1)->sourcePosition);
        }

        auto argList = listPtr->at(1)->listValue;

        std::vector<shared_ptr<AnalyzerNode>> argNameNodes;
        std::vector<shared_ptr<std::string>> argNames;

        for(auto arg: *argList) {
            if(arg->tag != kTypeTagSymbol) {
                // Lambda arguments must be symbols
                throw CompilerException("Lambda arguments must be symbols",
                        arg->sourcePosition);
            }

            auto sym = std::make_shared<ConstantValueAnalyzerNode>();
            sym->sourcePosition = arg->sourcePosition;
            sym->value = arg->stringValue;
            sym->type = kAnalyzerConstantTypeSymbol;
            argNameNodes.push_back(sym);

            argNames.push_back(arg->stringValue);
        }

        if (listPtr->size() < 3) {
            // Lambda forms must have at least one body expression
            throw CompilerException("Lambda forms must have at least one body expression",
                    form->sourcePosition);
        }

        auto body = std::make_shared<DoAnalyzerNode>();
        body->sourcePosition = listPtr->at(2)->sourcePosition;

        // Add all but the last statements to 'statements'
        for (auto it = listPtr->begin() + 2; it < listPtr->end() - 1; ++it) {
            body->statements.push_back(analyzeForm(*it));
        }

        // Add the last statement to 'returnValue'
        body->returnValue = analyzeForm(*(listPtr->end() - 1));


        auto node = std::make_shared<LambdaAnalyzerNode>();

        node->sourcePosition = form->sourcePosition;
        node->arg_names = argNames;
        node->arg_name_nodes = argNameNodes;
        node->body = body;

        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeDef(shared_ptr<ASTNode> form) {
        assert(form->tag == kTypeTagList);
        auto listPtr = form->listValue;
        assert(!listPtr->empty());

        if (listPtr->size() < 2) {
            // Def forms must contain a var name
            throw CompilerException("Def forms must have a var name",
                                    form->sourcePosition);
        }

        if (listPtr->size() < 3) {
            // Def forms must contain a binding value
            throw CompilerException("Def forms must have binding value",
                                    form->sourcePosition);
        }

        if (listPtr->size() > 3) {
            throw CompilerException("Unexpected arguments in def form",
                                    listPtr->at(3)->sourcePosition);
        }

        if (listPtr->at(1)->tag != kTypeTagSymbol) {
            throw CompilerException("Def forms var name must be a symbol",
                                    listPtr->at(1)->sourcePosition);
        }

        auto name = listPtr->at(1)->stringValue;
        auto valueNode = analyzeForm(listPtr->at(2));
        global_env_[*name] = valueNode;

        auto node = std::make_shared<DefAnalyzerNode>();
        node->sourcePosition = form->sourcePosition;
        node->name = name;
        node->value = valueNode;

        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::initialBindingWithName(const std::string &name) {
        auto result = global_env_.find(name);

        if(result != global_env_.end()) {
            return result->second;
        }

        return nullptr;
    }

}