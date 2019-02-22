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
#include <algorithm>

namespace electrum {

    Analyzer::Analyzer() : is_quoting_(false),
                           is_quasi_quoting_(false),
                           in_macro_(false) {
    }

    shared_ptr<AnalyzerNode> Analyzer::analyze(shared_ptr<ASTNode> form, uint64_t depth, EvaluationPhase phase) {
        push_evaluation_phase(phase);

        auto node = analyzeForm(form);
        run_passes(node, depth);

        pop_evaluation_phase();
        assert(evaluation_phases_.empty());

        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeForm(const shared_ptr<ASTNode> form) {

        switch (form->tag) {
            case kTypeTagInteger:return analyzeInteger(form);
            case kTypeTagFloat:return analyzeFloat(form);
            case kTypeTagBoolean:return analyzeBoolean(form);
            case kTypeTagString:return analyzeString(form);
            case kTypeTagKeyword:return analyzeKeyword(form);
            case kTypeTagSymbol:return analyzeSymbol(form);
            case kTypeTagList:return analyzeList(form);
            case kTypeTagNil:return analyzeNil(form);
        }

        return shared_ptr<AnalyzerNode>();
    }

    vector<shared_ptr<AnalyzerNode>> Analyzer::collapse_top_level_forms(const shared_ptr<AnalyzerNode> node) {
        if (node->node_depth > 0) {
            return {node};
        }

        switch (node->nodeType()) {
            case kAnalyzerNodeTypeDo:
            case kAnalyzerNodeTypeEvalWhen:
            {
                vector<shared_ptr<AnalyzerNode>> nodes;
                for(auto c: node->children()) {
                    for(auto r: collapse_top_level_forms(c)) {
                        nodes.push_back(r);
                    }
                }

                return nodes;
            }
            default: return {node};
        }
    }

    vector<string> Analyzer::analyze_closed_overs(const shared_ptr<AnalyzerNode> node) {
        if (node->collected_closed_overs) {
            return node->closed_overs;
        }

        vector<string> closed_overs;

        for (auto child: node->children()) {
            for (auto v: analyze_closed_overs(child)) {
                closed_overs.push_back(v);
            }
        }

        switch (node->nodeType()) {
            case kAnalyzerNodeTypeLambda: {
                // If it's a lambda node, remove the lambda's args from the collected closed overs
                // of the child nodes.
                auto lambdaNode = std::dynamic_pointer_cast<LambdaAnalyzerNode>(node);

                closed_overs.erase(
                        std::remove_if(closed_overs.begin(),
                                       closed_overs.end(),
                                       [&lambdaNode](auto c) {
                                           for (auto n: lambdaNode->arg_names) {
                                               if (*n == c) {
                                                   return true;
                                               }
                                           }
                                           return false;
                                       }),
                        closed_overs.end());
                break;
            }
            case kAnalyzerNodeTypeVarLookup: {
                auto varNode = std::dynamic_pointer_cast<VarLookupNode>(node);
                if (!varNode->is_global) {
                    closed_overs.push_back(*varNode->name);
                }
                break;
            }
            default:break;
        }

        node->closed_overs = closed_overs;
        node->collected_closed_overs = true;

        return closed_overs;
    }

    void Analyzer::update_depth_for_node(const shared_ptr<AnalyzerNode> node, uint64_t starting_depth) {
        if (node->node_depth >= 0) {
            return;
        }

        node->node_depth = starting_depth;

        uint64_t new_depth;

        switch (node->nodeType()) {
            case kAnalyzerNodeTypeEvalWhen:
            case kAnalyzerNodeTypeDo:new_depth = starting_depth;
                break;
            default:new_depth = starting_depth + 1;
        }

        for (const auto &c: node->children()) {
            update_depth_for_node(c, new_depth);
        }
    }

    void Analyzer::assert_eval_when_for_compile_is_top_level(shared_ptr<AnalyzerNode> node) {
        if (node->nodeType() == kAnalyzerNodeTypeEvalWhen && node->node_depth > 0) {
            throw CompilerException("eval-when forms can only be used at the top-level.",
                                    node->sourcePosition);
        }

        for (auto c: node->children()) {
            assert_eval_when_for_compile_is_top_level(c);
        }
    }

    void Analyzer::update_evaluation_phase(shared_ptr<AnalyzerNode> node, EvaluationPhase phase) {
        EvaluationPhase p = phase;

        if(node->nodeType() == kAnalyzerNodeTypeEvalWhen) {
            auto evalWhenNode = std::dynamic_pointer_cast<EvalWhenAnalyzerNode>(node);
            p = evalWhenNode->phases;
        }

        node->evaluation_phase = p;

        for(auto c: node->children()) {
            update_evaluation_phase(c, p);
        }
    }

    void Analyzer::run_passes(std::shared_ptr<electrum::AnalyzerNode> node, uint64_t depth) {
        analyze_closed_overs(node);

        // Calculate the depth for each node
        update_depth_for_node(node, depth);

        // If any eval-when forms appear that are not top-level, throw an error.
        assert_eval_when_for_compile_is_top_level(node);

        // Update the evaluation phase of all of the nodes, with a default of load time evaluation.
        update_evaluation_phase(node, kEvaluationPhaseLoadTime);
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeSymbol(shared_ptr<ASTNode> form) {
        auto symName = form->stringValue;

        if (is_quoting_) {
            auto node = make_shared<ConstantValueAnalyzerNode>();
            node->sourcePosition = form->sourcePosition;
            node->type = kAnalyzerConstantTypeSymbol;
            node->value = symName;

            return node;
        }

        if (lookup_in_local_env(*symName) != nullptr) {
            auto node = make_shared<VarLookupNode>();
            node->sourcePosition = form->sourcePosition;
            node->name = form->stringValue;
            node->is_global = false;
            return node;
        }

        auto globalResult = global_env_.find(*symName);
        if (globalResult != global_env_.end()) {
            auto def = globalResult->second;

            if (in_macro_ && !(def.phase & kEvaluationPhaseCompileTime)) {
                throw CompilerException("The symbol " + *symName + " is not visible to the compiler",
                                        form->sourcePosition);
            }

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

    shared_ptr<AnalyzerNode> Analyzer::analyzeNil(const shared_ptr<ASTNode> form) {
        auto node = make_shared<ConstantValueAnalyzerNode>();
        node->type = kAnalyzerConstantTypeNil;
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

        if (is_quoting_) {
            auto node = make_shared<ConstantListAnalyzerNode>();
            node->sourcePosition = form->sourcePosition;

            for (auto item: *listPtr) {
                node->values.push_back(analyzeForm(item));
            }

            return node;
        }

        if (listSize > 0) {
            auto firstItemForm = listPtr->at(0);

            // Check for special form
            if (firstItemForm->tag == kTypeTagSymbol) {
                // Check for special form
                auto maybeSpecial = maybeAnalyzeSpecialForm(firstItemForm->stringValue, form);
                if (maybeSpecial) {
                    return maybeSpecial;
                }

                // Check for macro
                auto result = global_macros_.find(*firstItemForm->stringValue);
                if (result != global_macros_.end()) {
                    return analyzeMacroExpand(form);
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
        for (const auto &a: *listPtr) {
            if (first) {
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
            // Lambda arguments must be a list
            throw CompilerException("Lambda arguments must be a list",
                                    listPtr->at(1)->sourcePosition);
        }

        auto argList = listPtr->at(1)->listValue;

        std::vector<shared_ptr<AnalyzerNode>>
                argNameNodes;
        std::vector<shared_ptr<std::string>>
                argNames;

        for (auto arg: *argList) {
            if (arg->tag != kTypeTagSymbol) {
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

        push_local_env();

        for (int i = 0; i < argNames.size(); ++i) {
            store_in_local_env(*argNames[i], std::make_shared<ConstantValueAnalyzerNode>());
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

        pop_local_env();

        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeMacro(const shared_ptr<ASTNode> form) {
        assert(form->tag == kTypeTagList);
        auto listPtr = form->listValue;
        assert(!listPtr->empty());

        // (defmacro binding (args) body)

        if (listPtr->size() < 2) {
            throw CompilerException("Defmacro forms must have a binding",
                                    form->sourcePosition);
        }

        if (listPtr->at(1)->tag != kTypeTagSymbol) {
            throw CompilerException("Defmacro bindings must be symbols",
                                    listPtr->at(1)->sourcePosition);
        }

        if (listPtr->size() < 3) {
            throw CompilerException("Defmacro forms must have an argument list",
                                    form->sourcePosition);
        }

        if (listPtr->at(2)->tag != kTypeTagList) {
            // Macro arguments must be a list
            throw CompilerException("Defmacro arguments must be a list",
                                    listPtr->at(1)->sourcePosition);
        }

        auto binding = listPtr->at(1)->stringValue;

        auto argList = listPtr->at(2)->listValue;

        std::vector<shared_ptr<AnalyzerNode>>
                argNameNodes;
        std::vector<shared_ptr<std::string>>
                argNames;

        for (const auto &arg: *argList) {
            if (arg->tag != kTypeTagSymbol) {
                throw CompilerException("Defmacro arguments must be symbols",
                                        arg->sourcePosition);
            }

            auto sym = std::make_shared<ConstantValueAnalyzerNode>();
            sym->sourcePosition = arg->sourcePosition;
            sym->value = arg->stringValue;
            sym->type = kAnalyzerConstantTypeSymbol;
            argNameNodes.push_back(sym);

            argNames.push_back(arg->stringValue);
        }

        push_local_env();

        for (auto &argName : argNames) {
            store_in_local_env(*argName, std::make_shared<ConstantValueAnalyzerNode>());
        }

        if (listPtr->size() < 4) {
            throw CompilerException("Defmacro forms must have at least one body expression",
                                    form->sourcePosition);
        }

        auto body = std::make_shared<DoAnalyzerNode>();
        body->sourcePosition = listPtr->at(3)->sourcePosition;

        auto last_macro_val = in_macro_;
        in_macro_ = true;

        // Add all but the last statements to 'statements'
        for (auto it = listPtr->begin() + 3; it < listPtr->end() - 1; ++it) {
            body->statements.push_back(analyzeForm(*it));
        }

        // Add the last statement to 'returnValue'
        body->returnValue = analyzeForm(*(listPtr->end() - 1));

        in_macro_ = last_macro_val;

        auto node = std::make_shared<DefMacroAnalyzerNode>();

        node->sourcePosition = form->sourcePosition;
        node->name = binding;
        node->arg_names = argNames;
        node->arg_name_nodes = argNameNodes;
        node->body = body;

        global_macros_[*binding] = node;

        pop_local_env();

        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeMacroExpand(shared_ptr<ASTNode> form) {
        assert(form->tag == kTypeTagList);
        auto listPtr = form->listValue;
        assert(!listPtr->empty());
        assert(listPtr->at(0)->tag == kTypeTagSymbol);

        auto macroName = listPtr->at(0)->stringValue;

        auto result = global_macros_.find(*macroName);
        if (result == global_macros_.end()) {
            // This shouldn't be able to happen, as the macro has already been looked up before
            // this method was invoked.
            throw CompilerException("Fatal error, could not find macro!",
                                    form->sourcePosition);
        }

        auto macro = result->second;

        auto node = make_shared<MacroExpandAnalyzerNode>();
        node->macro = macro;
        node->args.reserve(listPtr->size() - 1);

        bool first = true;
        for (const auto &a: *listPtr) {
            if (first) {
                first = false;
            } else {
                is_quoting_ = true;
                node->args.push_back(analyzeForm(a));
                is_quoting_ = false;
            }
        }

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

        AnalyzerDefinition d;
        d.phase = current_evaluation_phase();
        d.node = valueNode;

        global_env_[*name] = d;

        auto node = std::make_shared<DefAnalyzerNode>();
        node->sourcePosition = form->sourcePosition;
        node->name = name;
        node->value = valueNode;

        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeDefFFIFn(shared_ptr<ASTNode> form) {
        assert(form->tag == kTypeTagList);
        auto listPtr = form->listValue;
        assert(!listPtr->empty());

        if (listPtr->size() < 2) {
            throw CompilerException("def-ffi-fn* forms must have a binding name",
                                    form->sourcePosition);
        }

        if (listPtr->size() < 3) {
            throw CompilerException("def-ffi-fn* forms must have a external function name",
                                    form->sourcePosition);
        }

        if (listPtr->size() < 4) {
            throw CompilerException("def-ffi-fn* forms must have a return type",
                                    form->sourcePosition);
        }

        if (listPtr->size() < 5) {
            throw CompilerException("def-ffi-fn* forms must have a list of argument types",
                                    form->sourcePosition);
        }

        if (listPtr->at(1)->tag != kTypeTagSymbol) {
            throw CompilerException("def-ffi-fn* binding must be a symbol",
                                    listPtr->at(1)->sourcePosition);
        }
        auto binding = listPtr->at(1)->stringValue;

        if (listPtr->at(2)->tag != kTypeTagSymbol) {
            throw CompilerException("def-ffi-fn* function name must be a symbol",
                                    listPtr->at(2)->sourcePosition);
        }
        auto fn_name = listPtr->at(2)->stringValue;

        if (listPtr->at(3)->tag != kTypeTagKeyword) {
            throw CompilerException("def-ffi-fn* return type must be a keyword",
                                    listPtr->at(3)->sourcePosition);
        }
        auto ret_type = ffi_type_from_keyword(*listPtr->at(3)->stringValue);

        if (ret_type == kFFITypeUnknown) {
            throw CompilerException("Unknown FFI type",
                                    listPtr->at(3)->sourcePosition);
        }

        if (listPtr->at(4)->tag != kTypeTagList) {
            throw CompilerException("def-ffi-fn* argument types must be a list",
                                    listPtr->at(4)->sourcePosition);
        }
        auto arg_types = listPtr->at(4)->listValue;

        vector<FFIType> args;
        args.reserve(arg_types->size());

        for (auto argPtr: *arg_types) {
            if (argPtr->tag != kTypeTagKeyword) {
                throw CompilerException("def-ffi-fn* arg type must be a keyword",
                                        argPtr->sourcePosition);
            }

            auto argType = ffi_type_from_keyword(*argPtr->stringValue);
            if (argType == kFFITypeUnknown) {
                throw CompilerException("Unknown FFI type",
                                        argPtr->sourcePosition);
            }
            args.push_back(argType);
        }

        auto node = make_shared<DefFFIFunctionNode>();
        node->binding = binding;
        node->func_name = fn_name;
        node->return_type = ret_type;
        node->arg_types = args;

        AnalyzerDefinition d;
        d.phase = current_evaluation_phase();
        d.node = node;

        global_env_[*binding] = d;

        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeQuote(shared_ptr<ASTNode> form) {
        assert(form->tag == kTypeTagList);
        auto listPtr = form->listValue;
        assert(!listPtr->empty());

        if (listPtr->size() < 2) {
            throw CompilerException("Quote forms must have one argument",
                                    form->sourcePosition);
        }

        if (listPtr->size() > 2) {
            throw CompilerException("Quote forms must not have more than one argument",
                                    form->sourcePosition);
        }

        auto quotedForm = listPtr->at(1);

        is_quoting_ = true;
        auto node = analyzeForm(quotedForm);
        is_quoting_ = false;

        return node;
    }

    shared_ptr<AnalyzerNode> Analyzer::analyzeEvalWhen(shared_ptr<ASTNode> form) {
        assert(form->tag == kTypeTagList);
        auto listPtr = form->listValue;
        assert(!listPtr->empty());

        // (eval-when (:compile :load) ...)

        if (listPtr->size() < 2) {
            throw CompilerException("eval-when forms must have a list of evaluation environments",
                                    form->sourcePosition);
        }

        if (listPtr->at(1)->tag != kTypeTagList) {
            throw CompilerException("Expected a list",
                                    listPtr->at(1)->sourcePosition);
        }

        EvaluationPhase phases = kEvaluationPhaseNone;

        for (auto p: *listPtr->at(1)->listValue) {
            if (p->tag != kTypeTagKeyword) {
                throw CompilerException("eval-when phase must be a keyword",
                                        p->sourcePosition);
            }

            if (*p->stringValue == "compile") {
                phases |= kEvaluationPhaseCompileTime;
            } else if (*p->stringValue == "load") {
                phases |= kEvaluationPhaseLoadTime;
            } else {
                throw CompilerException("Unknown eval-when phase", p->sourcePosition);
            }
        }

        auto node = make_shared<EvalWhenAnalyzerNode>();
        node->sourcePosition = form->sourcePosition;
        node->phases = phases;

        push_evaluation_phase(phases);

        // Add all but the last form to body
        for (auto it = listPtr->begin() + 2; it < listPtr->end() - 1; ++it) {
            node->body.push_back(analyzeForm(*it));
        }

        // Add the last form to 'last'
        node->last = analyzeForm(*(listPtr->end() - 1));

        pop_evaluation_phase();

        return node;
    }


    shared_ptr<AnalyzerNode> Analyzer::initialBindingWithName(const std::string &name) {
        auto result = global_env_.find(name);

        if (result != global_env_.end()) {
            return result->second.node;
        }

        return nullptr;
    }

    void Analyzer::push_local_env() {
        local_envs_.push_back({});
    }

    void Analyzer::pop_local_env() {
        local_envs_.pop_back();
    }

    shared_ptr<AnalyzerNode> Analyzer::lookup_in_local_env(std::string name) {
        for (auto it = local_envs_.rbegin(); it != local_envs_.rend(); ++it) {
            auto env = *it;

            auto result = env.find(name);

            if (result != env.end()) {
                return result->second;
            }
        }

        return nullptr;
    }

    void Analyzer::store_in_local_env(std::string name, shared_ptr<AnalyzerNode> initialValue) {
        local_envs_.at(local_envs_.size() - 1)[name] = initialValue;
    }

    void Analyzer::push_evaluation_phase(EvaluationPhase phase) {
        evaluation_phases_.push_back(phase);
    }

    EvaluationPhase Analyzer::pop_evaluation_phase() {
        assert(!evaluation_phases_.empty());
        auto p = evaluation_phases_.back();
        evaluation_phases_.pop_back();
        return p;
    }

    EvaluationPhase Analyzer::current_evaluation_phase() {
        return evaluation_phases_.back();
    }


}
