#include <utility>

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

Analyzer::Analyzer()
        :is_quoting_(false),
         in_macro_(false),
         current_ns_("el.user") {
}

shared_ptr<AnalyzerNode> Analyzer::analyze(const shared_ptr<ASTNode>& form, uint64_t depth, EvaluationPhase phase) {
    pushEvaluationPhase(phase);

    auto node = analyzeForm(form);
    runPasses(node, depth);

    popEvaluationPhase();
    assert(evaluation_phases_.empty());

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeForm(const shared_ptr<ASTNode>& form) {
    shared_ptr<AnalyzerNode> node;

    switch (form->tag) {
    case kTypeTagInteger:node = analyzeInteger(form);
        break;
    case kTypeTagFloat:node = analyzeFloat(form);
        break;
    case kTypeTagBoolean:node = analyzeBoolean(form);
        break;
    case kTypeTagString:node = analyzeString(form);
        break;
    case kTypeTagKeyword:node = analyzeKeyword(form);
        break;
    case kTypeTagSymbol:node = analyzeSymbol(form);
        break;
    case kTypeTagList:node = analyzeList(form);
        break;
    case kTypeTagNil:node = analyzeNil(form);
        break;
    }

    assert(node->sourcePosition != nullptr);

    return node;
}

vector<shared_ptr<AnalyzerNode>> Analyzer::collapseTopLevelForms(const shared_ptr<AnalyzerNode>& node) {
    if (node->node_depth > 0) {
        return {node};
    }

    switch (node->nodeType()) {
    case kAnalyzerNodeTypeDo:
    case kAnalyzerNodeTypeEvalWhen: {
        vector<shared_ptr<AnalyzerNode>> nodes;
        for (const auto& c: node->children()) {
            for (const auto& r: collapseTopLevelForms(c)) {
                nodes.push_back(r);
            }
        }

        return nodes;
    }
    default: return {node};
    }
}

vector<string> Analyzer::analyzeClosedOvers(const shared_ptr<AnalyzerNode>& node) {
    if (node->collected_closed_overs) {
        return node->closed_overs;
    }

    vector<string> closed_overs;

    for (const auto& child: node->children()) {
        for (const auto& v: analyzeClosedOvers(child)) {
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
                          // Exclude args and rest args from closed overs
                          for (const auto& n: lambdaNode->arg_names) {
                              if (*n == c) {
                                  return true;
                              }
                          }

                          return lambdaNode->has_rest_arg && *lambdaNode->rest_arg_name == c;
                        }),
                closed_overs.end());
        break;
    }
    case kAnalyzerNodeTypeDefMacro: {
        // If it's a def macro node, remove the macro's args from the collected closed overs
        // of the child nodes.
        auto defMacroNode = std::dynamic_pointer_cast<DefMacroAnalyzerNode>(node);

        closed_overs.erase(
                std::remove_if(closed_overs.begin(),
                        closed_overs.end(),
                        [&defMacroNode](auto c) {
                          for (const auto& n: defMacroNode->arg_names) {
                              if (*n == c) {
                                  return true;
                              }
                          }
                          return defMacroNode->has_rest_arg && *defMacroNode->rest_arg_name == c;
                        }),
                closed_overs.end());
        break;
    }
    case kAnalyzerNodeTypeVarLookup: {
        auto var_node = std::dynamic_pointer_cast<VarLookupNode>(node);
        if (!var_node->is_global) {
            closed_overs.push_back(*var_node->name);
        }
        break;
    }
    default:break;
    }

    node->closed_overs           = closed_overs;
    node->collected_closed_overs = true;

    return closed_overs;
}

void Analyzer::updateDepthForNode(const shared_ptr<AnalyzerNode>& node, uint64_t starting_depth) {
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

    for (const auto& c: node->children()) {
        updateDepthForNode(c, new_depth);
    }
}

void Analyzer::assertEvalWhenForCompileIsTopLevel(const shared_ptr<AnalyzerNode>& node) {
    if (node->nodeType() == kAnalyzerNodeTypeEvalWhen && node->node_depth > 0) {
        throw CompilerException("eval-when forms can only be used at the top-level.",
                node->sourcePosition);
    }

    for (const auto& c: node->children()) {
        assertEvalWhenForCompileIsTopLevel(c);
    }
}

void Analyzer::updateEvaluationPhase(const shared_ptr<AnalyzerNode>& node, EvaluationPhase phase) {
    EvaluationPhase p = phase;

    if (node->nodeType() == kAnalyzerNodeTypeEvalWhen) {
        auto evalWhenNode = std::dynamic_pointer_cast<EvalWhenAnalyzerNode>(node);
        p = evalWhenNode->phases;
    }

    node->evaluation_phase = p;

    for (const auto& c: node->children()) {
        updateEvaluationPhase(c, p);
    }
}

void Analyzer::runPasses(const std::shared_ptr<electrum::AnalyzerNode>& node, uint64_t depth) {
    analyzeClosedOvers(node);

    // Calculate the depth for each node
    updateDepthForNode(node, depth);

    // If any eval-when forms appear that are not top-level, throw an error.
    assertEvalWhenForCompileIsTopLevel(node);

    // Update the evaluation phase of all of the nodes, with a default of load time evaluation.
    updateEvaluationPhase(node, kEvaluationPhaseLoadTime);
}

shared_ptr<AnalyzerNode> Analyzer::analyzeSymbol(const shared_ptr<ASTNode>& form) {
    auto sym_name = form->stringValue;

    if (is_quoting_ || (!quasi_quote_state_.empty() && quasi_quote_state_.back())) {
        auto node = make_shared<ConstantValueAnalyzerNode>();
        node->sourcePosition = form->sourcePosition;
        node->type           = kAnalyzerConstantTypeSymbol;
        node->value          = sym_name;

        return node;
    }

    if (lookupInLocalEnv(*sym_name) != nullptr) {
        auto node = make_shared<VarLookupNode>();
        node->sourcePosition = form->sourcePosition;
        node->name           = form->stringValue;
        node->is_global      = false;
        node->target_ns      = make_shared<string>(current_ns_);
        return node;
    }

    string   binding = *sym_name;
    string   ns      = current_ns_;
    auto pos = sym_name->find_first_of('/');
    if (pos != std::string::npos && pos != 0) {
        ns = sym_name->substr(0, pos);
        auto b_pos = pos + 1;
        binding = sym_name->substr(b_pos, sym_name->size() - b_pos);
    }

    auto globalResult = ns_manager.lookupSymbolInNS(
            currentNamespace(),
            ns,
            binding);

    if (globalResult.has_value()) {
        if (in_macro_ && !(globalResult->phase & kEvaluationPhaseCompileTime)) {
            throw CompilerException("The symbol " + *sym_name + " is not visible to the compiler",
                    form->sourcePosition);
        }

        auto node = make_shared<VarLookupNode>();
        node->sourcePosition = form->sourcePosition;
        node->name           = make_shared<string>(binding);
        node->is_global      = true;
        node->target_ns      = make_shared<string>(ns);

        return node;
    }

    throw CompilerException("Unbound variable '" + *sym_name + "'",
            form->sourcePosition);
}

shared_ptr<AnalyzerNode> Analyzer::analyzeInteger(const shared_ptr<ASTNode>& form) {
    auto node = make_shared<ConstantValueAnalyzerNode>();
    node->type           = kAnalyzerConstantTypeInteger;
    node->value          = form->integerValue;
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeFloat(const shared_ptr<ASTNode>& form) {
    auto node = make_shared<ConstantValueAnalyzerNode>();
    node->type           = kAnalyzerConstantTypeFloat;
    node->value          = form->floatValue;
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeBoolean(const shared_ptr<ASTNode>& form) {
    auto node = make_shared<ConstantValueAnalyzerNode>();
    node->type           = kAnalyzerConstantTypeBoolean;
    node->value          = form->booleanValue;
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeString(const shared_ptr<ASTNode>& form) {
    auto node = make_shared<ConstantValueAnalyzerNode>();
    node->type           = kAnalyzerConstantTypeString;
    node->value          = form->stringValue;
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeNil(const shared_ptr<ASTNode>& form) {
    auto node = make_shared<ConstantValueAnalyzerNode>();
    node->type           = kAnalyzerConstantTypeNil;
    node->value          = form->stringValue;
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeKeyword(const shared_ptr<ASTNode>& form) {
    auto node = make_shared<ConstantValueAnalyzerNode>();
    node->type           = kAnalyzerConstantTypeKeyword;
    node->value          = form->stringValue;
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeList(const shared_ptr<ASTNode>& form) {
    auto listPtr  = form->listValue;
    auto listSize = listPtr->size();

    if (is_quoting_ || (!quasi_quote_state_.empty() && quasi_quote_state_.back())) {
        // Special case for unquote
        if (listSize > 0 && listPtr->at(0)->tag == kTypeTagSymbol && *listPtr->at(0)->stringValue == "unquote") {
            return analyzeUnquote(form);
        }

        auto node = make_shared<ConstantListAnalyzerNode>();
        node->sourcePosition = form->sourcePosition;
        node->ns             = current_ns_;

        for (const auto& item: *listPtr) {
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

shared_ptr<AnalyzerNode> Analyzer::maybeAnalyzeSpecialForm(const shared_ptr<string>& symbol_name,
        const shared_ptr<ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto listPtr = form->listValue;
    assert(!listPtr->empty());

    // If an analysis function exists for the symbol, return the result
    auto analyseFunc = specialForms.find(*symbol_name);
    if (analyseFunc != specialForms.end()) {
        auto func = analyseFunc->second;
        return (this->*func)(form);
    }

    return nullptr;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeMaybeInvoke(const shared_ptr<ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto listPtr = form->listValue;
    assert(!listPtr->empty());

    auto node = std::make_shared<MaybeInvokeAnalyzerNode>();
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    node->args.reserve(listPtr->size() - 1);

    bool first = true;
    for (const auto& a: *listPtr) {
        if (first) {
            node->fn = analyzeForm(listPtr->at(0));
            first = false;
        }
        else {
            node->args.push_back(analyzeForm(a));
        }
    }

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeIf(const shared_ptr<ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto listPtr = form->listValue;
    assert(!listPtr->empty());

    // Check list has at least a condition and consequent
    if (listPtr->size() < 3) {
        throw CompilerException("if form requires at least a condition and a consequent",
                form->sourcePosition);
    }

    auto conditionForm  = listPtr->at(1);
    auto consequentForm = listPtr->at(2);

    auto node = make_shared<IfAnalyzerNode>();
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    node->condition      = analyzeForm(conditionForm);
    node->consequent     = analyzeForm(consequentForm);

    if (listPtr->size() > 4) {
        // The list has more elements than is expected.
        throw CompilerException("if form must have a maximum of 3 statements",
                listPtr->at(4)->sourcePosition);
    }
    else if (listPtr->size() > 3) {
        auto alternativeForm = listPtr->at(3);
        node->alternative = analyzeForm(alternativeForm);
    }

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeDo(const shared_ptr<ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto listPtr = form->listValue;
    assert(!listPtr->empty());

    if (listPtr->size() < 2) {
        // Do forms must have at least one form in the body
        throw CompilerException("Do forms must have at least one body statement",
                form->sourcePosition);
    }

    auto node = make_shared<DoAnalyzerNode>();
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;

    // Add all but the last statements to 'statements'
    for (auto it = listPtr->begin() + 1; it < listPtr->end() - 1; ++it) {
        node->statements.push_back(analyzeForm(*it));
    }

    // Add the last statement to 'returnValue'
    node->returnValue = analyzeForm(*(listPtr->end() - 1));

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeLambda(const shared_ptr<ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto list_ptr = form->listValue;
    assert(!list_ptr->empty());

    if (list_ptr->size() < 2) {
        // Lambda forms must contain an argument list
        throw CompilerException("Lambda forms must have an argument list",
                form->sourcePosition);
    }

    if (list_ptr->at(1)->tag != kTypeTagList) {
        // Lambda arguments must be a list
        throw CompilerException("Lambda arguments must be a list",
                list_ptr->at(1)->sourcePosition);
    }

    auto arg_list = list_ptr->at(1)->listValue;

    std::vector<shared_ptr<AnalyzerNode>> arg_name_nodes;
    std::vector<shared_ptr<std::string>>  arg_names;

    bool               has_rest_arg = false;
    shared_ptr<string> rest_arg_name;
    int                rest_count   = 0;

    for (const auto& arg: *arg_list) {
        if (arg->tag != kTypeTagSymbol) {
            // Lambda arguments must be symbols
            throw CompilerException("Lambda arguments must be symbols",
                    arg->sourcePosition);
        }

        if (has_rest_arg) {
            if (rest_count > 1) {
                throw CompilerException("Unexpected argument after rest arg", arg->sourcePosition);
            }

            rest_arg_name = arg->stringValue;
            ++rest_count;
            continue;
        }
        else if (*arg->stringValue == "&") {
            has_rest_arg = true;
            continue;
        }

        auto sym = std::make_shared<ConstantValueAnalyzerNode>();
        sym->sourcePosition = arg->sourcePosition;
        sym->ns             = current_ns_;
        sym->value          = arg->stringValue;
        sym->type           = kAnalyzerConstantTypeSymbol;
        arg_name_nodes.push_back(sym);

        arg_names.push_back(arg->stringValue);
    }

    pushLocalEnv();

    for (auto& arg_name : arg_names) {
        storeInLocalEnv(*arg_name, std::make_shared<ConstantValueAnalyzerNode>());
    }

    if (has_rest_arg) {
        storeInLocalEnv(*rest_arg_name, std::make_shared<ConstantValueAnalyzerNode>());
    }

    if (list_ptr->size() < 3) {
        // Lambda forms must have at least one body expression
        throw CompilerException("Lambda forms must have at least one body expression",
                form->sourcePosition);
    }

    auto body = std::make_shared<DoAnalyzerNode>();
    body->sourcePosition = list_ptr->at(2)->sourcePosition;

    // Add all but the last statements to 'statements'
    for (auto it = list_ptr->begin() + 2; it < list_ptr->end() - 1; ++it) {
        body->statements.push_back(analyzeForm(*it));
    }

    // Add the last statement to 'returnValue'
    body->returnValue = analyzeForm(*(list_ptr->end() - 1));

    auto node = std::make_shared<LambdaAnalyzerNode>();

    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    node->arg_names      = arg_names;
    node->arg_name_nodes = arg_name_nodes;
    node->body           = body;

    if (has_rest_arg) {
        node->has_rest_arg  = true;
        node->rest_arg_name = rest_arg_name;
    }

    popLocalEnv();

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeMacro(const shared_ptr<ASTNode>& form) {
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

    auto arg_list = listPtr->at(2)->listValue;

    std::vector<shared_ptr<AnalyzerNode>> arg_name_nodes;
    std::vector<shared_ptr<std::string>>  arg_names;

    bool               has_rest_arg = false;
    shared_ptr<string> rest_arg_name;
    int                rest_count   = 0;
    
    for (const auto& arg: *arg_list) {
        if (arg->tag != kTypeTagSymbol) {
            throw CompilerException("Defmacro arguments must be symbols",
                    arg->sourcePosition);
        }

        if (has_rest_arg) {
            if (rest_count > 1) {
                throw CompilerException("Unexpected argument after rest arg", arg->sourcePosition);
            }

            rest_arg_name = arg->stringValue;
            ++rest_count;
            continue;
        }
        else if (*arg->stringValue == "&") {
            has_rest_arg = true;
            continue;
        }
        
        auto sym = std::make_shared<ConstantValueAnalyzerNode>();
        sym->sourcePosition = arg->sourcePosition;
        sym->ns             = current_ns_;
        sym->value          = arg->stringValue;
        sym->type           = kAnalyzerConstantTypeSymbol;
        arg_name_nodes.push_back(sym);

        arg_names.push_back(arg->stringValue);
    }

    pushLocalEnv();

    for (auto& arg_name : arg_names) {
        storeInLocalEnv(*arg_name, std::make_shared<ConstantValueAnalyzerNode>());
    }

    if (has_rest_arg) {
        storeInLocalEnv(*rest_arg_name, std::make_shared<ConstantValueAnalyzerNode>());
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
    node->ns             = current_ns_;
    node->name           = binding;
    node->arg_names      = arg_names;
    node->arg_name_nodes = arg_name_nodes;
    node->body           = body;

    if (has_rest_arg) {
        node->has_rest_arg  = true;
        node->rest_arg_name = rest_arg_name;
    }
    
    global_macros_[*binding] = node;

    popLocalEnv();

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeMacroExpand(const shared_ptr<ASTNode>& form) {
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
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    node->macro          = macro;
    node->args.reserve(listPtr->size() - 1);
    node->do_evaluate = true;

    bool first = true;
    for (const auto& a: *listPtr) {
        if (first) {
            first = false;
        }
        else {
            is_quoting_ = true;
            node->args.push_back(analyzeForm(a));
            is_quoting_ = false;
        }
    }

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeDef(const shared_ptr<ASTNode>& form) {
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

    auto name      = listPtr->at(1)->stringValue;
    auto valueNode = analyzeForm(listPtr->at(2));

    AnalyzerDefinition d;
    d.phase = currentEvaluationPhase();
    d.node  = valueNode;

    ns_manager.addGlobalDefinition(currentNamespace(),
            *name,
            kDefinitionTypeVariable,
            currentEvaluationPhase());

    auto node = std::make_shared<DefAnalyzerNode>();
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    node->name           = name;
    node->value          = valueNode;

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeDefFFIFn(const shared_ptr<ASTNode>& form) {
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

    for (const auto& argPtr: *arg_types) {
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
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    node->binding        = binding;
    node->func_name      = fn_name;
    node->return_type    = ret_type;
    node->arg_types      = args;

    ns_manager.addGlobalDefinition(currentNamespace(),
            *binding,
            kDefinitionTypeFunction,
            currentEvaluationPhase());

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeQuote(const shared_ptr<ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto list_ptr = form->listValue;
    assert(!list_ptr->empty());

    if (list_ptr->size() < 2) {
        throw CompilerException("Quote forms must have one argument",
                form->sourcePosition);
    }

    if (list_ptr->size() > 2) {
        throw CompilerException("Quote forms must not have more than one argument",
                form->sourcePosition);
    }

    auto quoted_form = list_ptr->at(1);

    is_quoting_ = true;
    auto node = analyzeForm(quoted_form);
    is_quoting_ = false;

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeQuasiQuote(const shared_ptr<ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto list_ptr = form->listValue;
    assert(!list_ptr->empty());

    if (list_ptr->size() < 2) {
        throw CompilerException("Quasiquote forms must have one argument",
                form->sourcePosition);
    }

    if (list_ptr->size() > 2) {
        throw CompilerException("Quasiquote forms must not have more than one argument",
                form->sourcePosition);
    }

    auto quoted_form = list_ptr->at(1);

    quasi_quote_state_.push_back(true);
    auto node = analyzeForm(quoted_form);
    quasi_quote_state_.pop_back();

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeUnquote(const shared_ptr<ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto list_ptr = form->listValue;
    assert(!list_ptr->empty());

    if (list_ptr->size() < 2) {
        throw CompilerException("Unquote forms must have one argument",
                form->sourcePosition);
    }

    if (list_ptr->size() > 2) {
        throw CompilerException("Unquote forms must not have more than one argument",
                form->sourcePosition);
    }

    auto unquoted_form = list_ptr->at(1);

    if (quasi_quote_state_.empty() || !quasi_quote_state_.back()) {
        throw CompilerException("Unquote not valid: not in quasiquote.", form->sourcePosition);
    }

    quasi_quote_state_.pop_back();
    quasi_quote_state_.push_back(false);
    auto node = analyzeForm(unquoted_form);
    quasi_quote_state_.pop_back();
    quasi_quote_state_.push_back(true);

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeEvalWhen(const shared_ptr<ASTNode>& form) {
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

    for (const auto& p: *listPtr->at(1)->listValue) {
        if (p->tag != kTypeTagKeyword) {
            throw CompilerException("eval-when phase must be a keyword",
                    p->sourcePosition);
        }

        if (*p->stringValue == "compile") {
            phases |= kEvaluationPhaseCompileTime;
        }
        else if (*p->stringValue == "load") {
            phases |= kEvaluationPhaseLoadTime;
        }
        else {
            throw CompilerException("Unknown eval-when phase", p->sourcePosition);
        }
    }

    auto node = make_shared<EvalWhenAnalyzerNode>();
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    node->phases         = phases;

    pushEvaluationPhase(phases);

    // Add all but the last form to body
    for (auto it = listPtr->begin() + 2; it < listPtr->end() - 1; ++it) {
        node->body.push_back(analyzeForm(*it));
    }

    // Add the last form to 'last'
    node->last = analyzeForm(*(listPtr->end() - 1));

    popEvaluationPhase();

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeTry(const shared_ptr<ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto listPtr = form->listValue;
    assert(!listPtr->empty());

    vector<shared_ptr<AnalyzerNode>> b;

    for (int i = 1; i < listPtr->size(); i++) {
        auto f = listPtr->at(i);
        b.push_back(analyzeForm(f));
    }

    vector<shared_ptr<AnalyzerNode>> body_nodes;
    while (!b.empty() && b[0]->nodeType() != kAnalyzerNodeTypeCatch) {
        body_nodes.push_back(*b.begin());
        b.erase(b.begin());
    }

    vector<shared_ptr<CatchAnalyzerNode>> catch_nodes;
    for (auto                             n: b) {
        if (n->nodeType() != kAnalyzerNodeTypeCatch) {
            throw CompilerException("Expected catch form", n->sourcePosition);
        }

        catch_nodes.push_back(std::dynamic_pointer_cast<CatchAnalyzerNode>(n));
    }

    if (catch_nodes.empty()) {
        throw CompilerException("Expected at least one catch form", form->sourcePosition);
    }

    auto node = make_shared<TryAnalyzerNode>();
    node->sourcePosition = form->sourcePosition;
    node->ns             = current_ns_;
    node->body           = body_nodes;
    node->catch_nodes    = catch_nodes;

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeCatch(const shared_ptr<ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto listPtr = form->listValue;
    assert(!listPtr->empty());

    if (listPtr->size() < 3) {
        throw CompilerException("Catch form must have a binding and a body", form->sourcePosition);
    }

    if (listPtr->at(1)->tag != kTypeTagList) {
        throw CompilerException("Catch: expected a list", listPtr->at(1)->sourcePosition);
    }

    auto bindingList = listPtr->at(1)->listValue;

    if (bindingList->size() != 2) {
        throw CompilerException("Catch binding must contain an exception type and a binding name",
                listPtr->at(1)->sourcePosition);
    }

    if (bindingList->at(0)->tag != kTypeTagSymbol) {
        throw CompilerException("Catch: expected an exception type symbol", bindingList->at(0)->sourcePosition);
    }

    if (bindingList->at(1)->tag != kTypeTagSymbol) {
        throw CompilerException("Catch: expected a binding symbol", bindingList->at(1)->sourcePosition);
    }

    auto exception_type    = bindingList->at(0)->stringValue;
    auto exception_binding = bindingList->at(1)->stringValue;

    vector<shared_ptr<AnalyzerNode>> body;

    pushLocalEnv();
    storeInLocalEnv(*exception_binding, std::make_shared<ConstantValueAnalyzerNode>());

    for (int i = 2; i < listPtr->size(); i++) {
        body.push_back(analyzeForm(listPtr->at(i)));
    }

    popLocalEnv();

    auto node = make_shared<CatchAnalyzerNode>();
    node->sourcePosition    = form->sourcePosition;
    node->ns                = current_ns_;
    node->exception_type    = exception_type;
    node->exception_binding = exception_binding;
    node->body              = body;

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeMakeList(const std::shared_ptr<ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto listPtr = form->listValue;
    assert(!listPtr->empty());

    auto node = make_shared<ConstantListAnalyzerNode>();
    node->sourcePosition = form->sourcePosition;
    node->ns = current_ns_;

    for(int i = 1; i < listPtr->size(); ++i) {
        auto item = listPtr->at(i);
        node->values.push_back(analyzeForm(item));
    }

    return node;
}

shared_ptr<AnalyzerNode> Analyzer::analyzeInNS(const std::shared_ptr<electrum::ASTNode>& form) {
    assert(form->tag == kTypeTagList);
    auto listPtr = form->listValue;
    assert(!listPtr->empty());

    if (listPtr->size() > 2) {
        throw CompilerException("in-ns: Unexpected argument(s)", listPtr->at(2)->sourcePosition);
    }

    auto ns_node = analyzeForm(listPtr->at(1));

    if (ns_node->nodeType() != kAnalyzerNodeTypeConstant) {
        throw CompilerException("in-ns: Namespace should be a symbol", listPtr->at(1)->sourcePosition);
    }

    auto c_node = std::dynamic_pointer_cast<ConstantValueAnalyzerNode>(ns_node);

    if (c_node->type != kAnalyzerConstantTypeSymbol) {
        throw CompilerException("in-ns: Namespace should be a symbol", listPtr->at(1)->sourcePosition);
    }

    auto ns_val = boost::get<shared_ptr<string>>(c_node->value);
    current_ns_ = *ns_val;

    auto nil_node = make_shared<ConstantValueAnalyzerNode>();
    nil_node->type           = kAnalyzerConstantTypeNil;
    nil_node->sourcePosition = form->sourcePosition;
    nil_node->ns             = current_ns_;
    return nil_node;
}

void Analyzer::pushLocalEnv() {
    local_envs_.emplace_back();
}

void Analyzer::popLocalEnv() {
    local_envs_.pop_back();
}

shared_ptr<AnalyzerNode> Analyzer::lookupInLocalEnv(const std::string& name) {
    for (auto it = local_envs_.rbegin(); it != local_envs_.rend(); ++it) {
        auto env = *it;

        auto result = env.find(name);

        if (result != env.end()) {
            return result->second;
        }
    }

    return nullptr;
}

void Analyzer::storeInLocalEnv(const std::string& name, shared_ptr<AnalyzerNode> initial_value) {
    local_envs_.at(local_envs_.size() - 1)[name] = std::move(initial_value);
}

void Analyzer::pushEvaluationPhase(EvaluationPhase phase) {
    evaluation_phases_.push_back(phase);
}

EvaluationPhase Analyzer::popEvaluationPhase() {
    assert(!evaluation_phases_.empty());
    auto p = evaluation_phases_.back();
    evaluation_phases_.pop_back();
    return p;
}

EvaluationPhase Analyzer::currentEvaluationPhase() {
    return evaluation_phases_.back();
}

std::shared_ptr<Namespace> Analyzer::currentNamespace() {
    return ns_manager.getOrCreateNamespace(current_ns_);
}

void AnalyzerNode::printNode() {
    YAML::Emitter e;
    e << serialize();
    std::cout << e.c_str() << std::endl;
}
}
