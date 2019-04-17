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


#ifndef ELECTRUM_ANALYZER_H
#define ELECTRUM_ANALYZER_H

#include "types/Types.h"
#include "EvaluationPhase.h"
#include "Namespace.h"
#include "NamespaceManager.h"
#include <iostream>
#include <memory>
#include <boost/variant.hpp>
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace electrum {

using std::shared_ptr;
using std::make_shared;
using std::string;
using std::unordered_map;

enum AnalyzerNodeType {
  kAnalyzerNodeTypeIf,
  kAnalyzerNodeTypeConstant,
  kAnalyzerNodeTypeDo,
  kAnalyzerNodeTypeLambda,
  kAnalyzerNodeTypeDefMacro,
  kAnalyzerNodeTypeDef,
  kAnalyzerNodeTypeVarLookup,
  kAnalyzerNodeTypeMaybeInvoke,
  kAnalyzerNodeTypeMacroExpand,
  kAnalyzerNodeTypeDefFFIFunction,
  kAnalyzerNodeTypeConstantList,
  kAnalyzerNodeTypeEvalWhen,
  kAnalyzerNodeTypeTry,
  kAnalyzerNodeTypeCatch
};

class AnalyzerNode {
public:
    virtual ~AnalyzerNode() = default;

    /// A reference to the original source position of the form
    shared_ptr<SourcePosition> sourcePosition;

    /// The closed overs generated by the `collectClosedOvers` pass
    vector<string> closed_overs;

    /// A flag indicating whether the closed overs have already been collected
    bool collected_closed_overs;

    /// The depth of this node from the top level
    int64_t node_depth = -1;

    /// The phases in which the node will be evaluated
    EvaluationPhase evaluation_phase = kEvaluationPhaseNone;

    /// The namespace that the node is evaluated in
    string ns;

    /// The node's children
    virtual vector<shared_ptr<AnalyzerNode>> children() { return {}; };

    /// The type of the node
    virtual AnalyzerNodeType nodeType() = 0;

    /// Serialize to YAML
    virtual YAML::Node serialize() = 0;

    /// Print the node to stdout
    void printNode() {
        YAML::Emitter e;
        e << serialize();
        std::cout << e.c_str() << std::endl;
    }
};

/**
 * Node that represents an 'if' expression.
 * Condition should be a bool
 * If alternative is nil, that branch should return nil.
 */
class IfAnalyzerNode : public AnalyzerNode {
public:
    shared_ptr<AnalyzerNode> condition;
    shared_ptr<AnalyzerNode> consequent;
    shared_ptr<AnalyzerNode> alternative;

    vector<shared_ptr<AnalyzerNode>> children() override {
        return {condition, consequent, alternative};
    }

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeIf;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"]        = "if";
        node["condition"]   = condition->serialize();
        node["consequent"]  = consequent->serialize();
        node["alternative"] = alternative->serialize();
        return node;
    }
};

/**
 * The type of constant that the constant node represents.
 */
enum AnalyzerConstantType {
  kAnalyzerConstantTypeInteger,
  kAnalyzerConstantTypeFloat,
  kAnalyzerConstantTypeBoolean,
  kAnalyzerConstantTypeString,
  kAnalyzerConstantTypeSymbol,
  kAnalyzerConstantTypeKeyword,
  kAnalyzerConstantTypeNil
};

/*
 * Node that represents a constant value.
 */
class ConstantValueAnalyzerNode : public AnalyzerNode {
public:
    /// Type of the constant
    AnalyzerConstantType type;

    /// Value of the constant
    boost::variant<int64_t,
                   double,
                   bool,
                   shared_ptr<string>> value;

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeConstant;
    }

    string typeString() {
        switch (type) {
        case kAnalyzerConstantTypeSymbol: return "symbol";
        case kAnalyzerConstantTypeNil: return "nil";
        case kAnalyzerConstantTypeBoolean: return "boolean";
        case kAnalyzerConstantTypeFloat: return "float";
        case kAnalyzerConstantTypeInteger: return "integer";
        case kAnalyzerConstantTypeKeyword: return "keyword";
        case kAnalyzerConstantTypeString: return "string";
        }

        return "";
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"]       = "constant";
        node["const-type"] = typeString();

        switch (type) {
        case kAnalyzerConstantTypeSymbol: node["value"] = *boost::get<shared_ptr<string>>(value);
            break;
        case kAnalyzerConstantTypeNil: node["value"] = "nil";
            break;
        case kAnalyzerConstantTypeBoolean: node["value"] = boost::get<bool>(value);
            break;
        case kAnalyzerConstantTypeFloat: node["value"] = boost::get<double>(value);
            break;
        case kAnalyzerConstantTypeInteger: node["value"] = boost::get<int64_t>(value);
            break;
        case kAnalyzerConstantTypeKeyword: node["value"] = *boost::get<shared_ptr<string>>(value);
            break;
        case kAnalyzerConstantTypeString: node["value"] = *boost::get<shared_ptr<string>>(value);
            break;
        }
        return node;
    }
};

/*
 * Node that represents a list
 */
class ConstantListAnalyzerNode : public AnalyzerNode {
public:
    /// The values in the list
    vector<shared_ptr<AnalyzerNode>> values;

    vector<shared_ptr<AnalyzerNode>> children() override {
        return values;
    }

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeConstantList;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"] = "constant-list";

        vector<YAML::Node> vals;
        for (const auto& n: values) {
            vals.push_back(n->serialize());
        }

        node["values"] = vals;

        return node;
    }
};

/*
 * Node that represents a 'do' form
 */
class DoAnalyzerNode : public AnalyzerNode {
public:
    /// A vector of all statements in the do form, except the return value
    vector<shared_ptr<AnalyzerNode>> statements;
    /// The last value in the do form
    shared_ptr<AnalyzerNode>         returnValue;

    vector<shared_ptr<AnalyzerNode>> children() override {
        auto rv = vector<shared_ptr<AnalyzerNode>>(statements);
        rv.push_back(returnValue);
        return rv;
    }

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeDo;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"]         = "do";
        node["return-value"] = returnValue->serialize();

        vector<YAML::Node> s;
        for (const auto& n: statements) { s.push_back(n->serialize()); }
        node["statements"] = s;

        return node;
    }
};

class LambdaAnalyzerNode : public AnalyzerNode {
public:
    /// A vector of the argument names
    vector<shared_ptr<AnalyzerNode>> arg_name_nodes;

    vector<shared_ptr<std::string>> arg_names;

    /// Whether the func has a rest arg
    bool has_rest_arg;

    /// The name of the rest arg
    shared_ptr<std::string> rest_arg_name;

    /// A do node representing the body
    shared_ptr<AnalyzerNode> body;

    vector<shared_ptr<AnalyzerNode>> children() override {
        return body->children();
    }

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeLambda;
    }

    YAML::Node serialize() override {
        YAML::Node node;

        node["type"]         = "lambda";
        node["has-rest-arg"] = has_rest_arg;
        if (rest_arg_name) { node["rest-arg-name"] = *rest_arg_name; }

        node["body"] = body->serialize();

        vector<string> an;
        for (const auto& n: arg_names) { an.push_back(*n); }
        node["arg-names"] = an;

        return node;
    }
};

class DefMacroAnalyzerNode : public AnalyzerNode {
public:
    /// The binding name
    shared_ptr<std::string> name;

    /// A vector of the argument names
    vector<shared_ptr<AnalyzerNode>> arg_name_nodes;
    vector<shared_ptr<std::string>>  arg_names;

    /// Whether the func has a rest arg
    bool has_rest_arg;

    /// The name of the rest arg
    shared_ptr<std::string> rest_arg_name;

    /// A do node representing the body
    shared_ptr<AnalyzerNode> body;

    vector<shared_ptr<AnalyzerNode>> children() override {
        return body->children();
    }

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeDefMacro;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"] = "defmacro";
        node["name"] = *name;
        node["body"] = body->serialize();

        vector<string> an;
        for (const auto& n: arg_names) { an.push_back(*n); }
        node["arg-names"] = an;

        return node;
    }
};

class DefAnalyzerNode : public AnalyzerNode {
public:
    /// The binding name
    shared_ptr<std::string> name;

    /// The binding value
    shared_ptr<AnalyzerNode> value;

    vector<shared_ptr<AnalyzerNode>> children() override {
        return {value};
    }

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeDef;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"]  = "def";
        node["name"]  = "name";
        node["value"] = value->serialize();
        return node;
    }
};

class VarLookupNode : public AnalyzerNode {
public:

    /// The binding namespace
    shared_ptr<std::string> target_ns;

    /// The binding name
    shared_ptr<std::string> name;

    /// Is it a global variable?
    bool is_global;

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeVarLookup;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"]      = "var-lookup";
        if (target_ns) { node["target-ns"] = *target_ns; }
        node["name"]      = *name;
        node["is_global"] = is_global;

        return node;
    }
};

class MacroExpandAnalyzerNode : public AnalyzerNode {
public:
    /// Expander to call
    shared_ptr<AnalyzerNode> macro;

    /// Macro arguments
    std::vector<shared_ptr<AnalyzerNode>> args;

    /// Specifies whether the result be evaluated after expansion
    bool do_evaluate;

    vector<shared_ptr<AnalyzerNode>> children() override {
        vector<shared_ptr<AnalyzerNode>> c = {macro};
        for (const auto& a: args) { c.push_back(a); }
        return c;
    }

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeMacroExpand;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"] = "macroexpand";

        vector<YAML::Node> a;
        for (const auto& n: args) { a.push_back(n->serialize()); }
        node["args"] = a;

        return node;
    }
};

class MaybeInvokeAnalyzerNode : public AnalyzerNode {
public:
    /// Function to call
    shared_ptr<AnalyzerNode> fn;

    /// Function call arguments
    std::vector<shared_ptr<AnalyzerNode>> args;

    vector<shared_ptr<AnalyzerNode>> children() override {
        vector<shared_ptr<AnalyzerNode>> c = {fn};
        for (const auto& a: args) { c.push_back(a); }
        return c;
    }

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeMaybeInvoke;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"] = "maybe-invoke";
        node["fn"]   = fn->serialize();

        vector<YAML::Node> a;
        for (const auto& n: args) { a.push_back(n->serialize()); }
        node["args"] = a;

        return node;
    }
};

enum FFIType : int64_t {
  kFFITypeUnknown = -1,
  kFFITypeElectrumValue
};

static FFIType ffi_type_from_keyword(const string& input) {
    if (input == "el") {
        return kFFITypeElectrumValue;
    }

    return kFFITypeUnknown;
}

static string string_from_ffi_type(FFIType t) {
    switch (t) {
    case kFFITypeElectrumValue: return "el";
    default: return "unknown";
    }
}

class DefFFIFunctionNode : public AnalyzerNode {
public:
    /// Binding name
    shared_ptr<string> binding;

    /// Function name
    shared_ptr<string> func_name;

    /// Return type
    FFIType return_type;

    vector<FFIType> arg_types;

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeDefFFIFunction;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"]        = "def-ffi-fn";
        node["binding"]     = "binding";
        node["func-name"]   = *func_name;
        node["return-type"] = string_from_ffi_type(return_type);

        vector<string> at;
        for (auto      a: arg_types) { at.push_back(string_from_ffi_type(a)); }
        node["arg_types"] = at;

        return node;
    }
};

class EvalWhenAnalyzerNode : public AnalyzerNode {
public:
    /// Which evaluation phases the body will be evaluated in
    EvaluationPhase phases;

    /// All but the last body form
    vector<shared_ptr<AnalyzerNode>> body;

    /// The last body form (returned value)
    shared_ptr<AnalyzerNode> last;

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeEvalWhen;
    }

    vector<shared_ptr<AnalyzerNode>> children() override {
        vector<shared_ptr<AnalyzerNode>> c;

        for (const auto& b: body) {
            c.push_back(b);
        }

        c.push_back(last);

        return c;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"] = "eval-when";

        vector<string> p;

        if (phases & kEvaluationPhaseCompileTime) {
            p.emplace_back("compile");
        }

        if (phases & kEvaluationPhaseLoadTime) {
            p.emplace_back("load");
        }

        node["phases"] = p;
        node["last"]   = last->serialize();

        vector<YAML::Node> b;
        for (const auto& n: body) {
            b.push_back(n->serialize());
        }
        node["body"] = b;

        return node;
    }
};

class CatchAnalyzerNode : public AnalyzerNode {
public:
    shared_ptr<string> exception_type;

    shared_ptr<string> exception_binding;

    vector<shared_ptr<AnalyzerNode>> body;

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeCatch;
    }

    vector<shared_ptr<AnalyzerNode>> children() override {
        return body;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"]              = "catch";
        node["exception-type"]    = *exception_type;
        node["exception_binding"] = *exception_binding;

        vector<YAML::Node> b;
        for (const auto& n: body) {
            b.push_back(n->serialize());
        }
        node["body"] = b;

        return node;
    }
};

class TryAnalyzerNode : public AnalyzerNode {
public:
    vector<shared_ptr<AnalyzerNode>> body;

    vector<shared_ptr<CatchAnalyzerNode>> catch_nodes;

    AnalyzerNodeType nodeType() override {
        return kAnalyzerNodeTypeTry;
    }

    vector<shared_ptr<AnalyzerNode>> children() override {
        vector<shared_ptr<AnalyzerNode>> c;
        c.reserve(body.size() + catch_nodes.size());
        c.insert(c.end(), body.begin(), body.end());
        c.insert(c.end(), catch_nodes.begin(), catch_nodes.end());

        return c;
    }

    YAML::Node serialize() override {
        YAML::Node node;
        node["type"] = "try";

        vector<YAML::Node> b;
        for (const auto& n: body) {
            b.push_back(n->serialize());
        }
        node["body"] = b;

        vector<YAML::Node> c;
        for (const auto& n: catch_nodes) {
            c.push_back(n->serialize());
        }
        node["catch-nodes"] = c;

        return node;
    }
};

class Analyzer {
public:
    Analyzer();

    shared_ptr<AnalyzerNode> analyze(const shared_ptr<ASTNode>& form, uint64_t depth = 0,
            EvaluationPhase phase = kEvaluationPhaseLoadTime);

    /// Collapses the given node into a vector of top level forms
    vector<shared_ptr<AnalyzerNode>> collapseTopLevelForms(const shared_ptr<AnalyzerNode>& node);

    shared_ptr<Namespace> currentNamespace();
private:

    /* Passes */
    /// Run all analyzer passes
    void runPasses(const shared_ptr<AnalyzerNode>& node, uint64_t depth = 0);

    /// Recursively updates the node's `node_depth` value, starting at `starting_depth`
    void updateDepthForNode(const shared_ptr<AnalyzerNode>& node, uint64_t starting_depth = 0);

    /// Asserts that all `eval-when` forms are at the top level
    void assertEvalWhenForCompileIsTopLevel(const shared_ptr<AnalyzerNode>& node);

    /// Recursively updates the evaluation phase of all nodes that are inside eval-when forms
    void updateEvaluationPhase(const shared_ptr<AnalyzerNode>& node, EvaluationPhase phase);

    /// Recursively walks the node tree and generates a list of closed overs for each node
    vector<string> analyzeClosedOvers(const shared_ptr<AnalyzerNode>& node);

    /* Analyzers */
    shared_ptr<AnalyzerNode> analyzeForm(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeSymbol(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeInteger(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeFloat(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeString(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeNil(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeKeyword(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeBoolean(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeList(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeIf(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeDo(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeLambda(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeMacro(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeMacroExpand(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeDef(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeMaybeInvoke(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeDefFFIFn(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeQuote(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeQuasiQuote(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeUnquote(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeEvalWhen(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeTry(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeCatch(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode> analyzeInNS(const shared_ptr<ASTNode>& form);
    shared_ptr<AnalyzerNode>
    maybeAnalyzeSpecialForm(const shared_ptr<string>& symbol_name, const shared_ptr<ASTNode>& form);

    typedef shared_ptr<AnalyzerNode> (Analyzer::*AnalyzerFunc)(const shared_ptr<ASTNode>&);

    /* Environment */
    void pushLocalEnv();
    void popLocalEnv();
    shared_ptr<AnalyzerNode> lookupInLocalEnv(const std::string& name);
    void storeInLocalEnv(const std::string& name, shared_ptr<AnalyzerNode> initial_value);

    /* Evaluation Phase */
    void pushEvaluationPhase(EvaluationPhase phase);
    EvaluationPhase popEvaluationPhase();
    EvaluationPhase currentEvaluationPhase();

    /// Analysis functions for special forms
    const std::unordered_map<std::string, AnalyzerFunc> specialForms{
            {"if", &Analyzer::analyzeIf},
            {"do", &Analyzer::analyzeDo},
            {"lambda", &Analyzer::analyzeLambda},
            {"defmacro", &Analyzer::analyzeMacro},
            {"def", &Analyzer::analyzeDef},
            {"def-ffi-fn*", &Analyzer::analyzeDefFFIFn},
            {"quote", &Analyzer::analyzeQuote},
            {"quasiquote", &Analyzer::analyzeQuasiQuote},
            {"unquote", &Analyzer::analyzeUnquote},
            {"eval-when", &Analyzer::analyzeEvalWhen},
            {"try", &Analyzer::analyzeTry},
            {"catch", &Analyzer::analyzeCatch},
            {"in-ns", &Analyzer::analyzeInNS}
    };

    struct AnalyzerDefinition {
      EvaluationPhase          phase;
      shared_ptr<AnalyzerNode> node;
    };

    /// Holds global macros
    std::unordered_map<std::string, shared_ptr<AnalyzerNode>> global_macros_;

    vector<unordered_map<string, shared_ptr<AnalyzerNode>>> local_envs_;

    NamespaceManager ns_manager;

    /// The namespace of the currently analyzed form
    string current_ns_;

    /// Flag to specify whether the analyzer is inside a quoted form
    bool is_quoting_;

    /// The current quasiquote state
    std::vector<bool> quasi_quote_state_;

    /// Flag to specify whether the analyzer is currently analyzing a macro expander
    bool in_macro_;

    /// Stack to hold evaluation environments
    vector<EvaluationPhase> evaluation_phases_;
};
}

#endif //ELECTRUM_ANALYZER_H
