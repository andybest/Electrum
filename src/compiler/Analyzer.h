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
#include <memory>
#include <boost/variant.hpp>
#include <string>
#include <unordered_map>

namespace electrum {

    using std::shared_ptr;
    using std::make_shared;
    using std::string;
    using std::unordered_map;

    enum AnalyzerNodeType {
        kAnalyzerNodeTypeNone = -1,
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
        kAnalyzerNodeTypeEvalWhen
    };



    class AnalyzerNode {
    public:
        virtual ~AnalyzerNode() {};

        shared_ptr<SourcePosition> sourcePosition;

        vector <string> closed_overs;

        bool collected_closed_overs;

        int64_t node_depth = -1;

        EvaluationPhase evaluation_phase = kEvaluationPhaseNone;

        virtual vector <shared_ptr<AnalyzerNode>> children() { return {}; };

        virtual AnalyzerNodeType nodeType() = 0;
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

        vector <shared_ptr<AnalyzerNode>> children() override {
            return {condition, consequent, alternative};
        }

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeIf;
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
    };

    /*
     * Node that represents a list
     */
    class ConstantListAnalyzerNode : public AnalyzerNode {
    public:
        /// The values in the list
        vector <shared_ptr<AnalyzerNode>> values;

        vector <shared_ptr<AnalyzerNode>> children() override {
            return values;
        }

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeConstantList;
        }
    };

    /*
     * Node that represents a 'do' form
     */
    class DoAnalyzerNode : public AnalyzerNode {
    public:
        /// A vector of all statements in the do form, except the return value
        vector <shared_ptr<AnalyzerNode>> statements;
        /// The last value in the do form
        shared_ptr<AnalyzerNode> returnValue;

        vector <shared_ptr<AnalyzerNode>> children() override {
            auto rv = vector<shared_ptr<AnalyzerNode>>(statements);
            rv.push_back(returnValue);
            return rv;
        }

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeDo;
        }
    };

    class LambdaAnalyzerNode : public AnalyzerNode {
    public:
        /// A vector of the argument names
        vector <shared_ptr<AnalyzerNode>> arg_name_nodes;

        vector <shared_ptr<std::string>> arg_names;

        /// A do node representing the body
        shared_ptr<AnalyzerNode> body;

        vector <shared_ptr<AnalyzerNode>> children() override {
            return body->children();
        }

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeLambda;
        }
    };

    class DefMacroAnalyzerNode : public AnalyzerNode {
    public:
        /// The binding name
        shared_ptr<std::string> name;

        /// A vector of the argument names
        vector <shared_ptr<AnalyzerNode>> arg_name_nodes;
        vector <shared_ptr<std::string>> arg_names;

        /// A do node representing the body
        shared_ptr<AnalyzerNode> body;

        vector <shared_ptr<AnalyzerNode>> children() override {
            return body->children();
        }

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeDefMacro;
        }
    };

    class DefAnalyzerNode : public AnalyzerNode {
    public:
        /// The binding name
        shared_ptr<std::string> name;

        /// The binding value
        shared_ptr<AnalyzerNode> value;

        vector <shared_ptr<AnalyzerNode>> children() override {
            return {value};
        }

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeDef;
        }
    };

    class VarLookupNode : public AnalyzerNode {
    public:
        /// The binding name
        shared_ptr<std::string> name;

        /// Is it a global variable?
        bool is_global;

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeVarLookup;
        }
    };

    class MacroExpandAnalyzerNode : public AnalyzerNode {
    public:
        /// Expander to call
        shared_ptr<AnalyzerNode> macro;

        /// Macro arguments
        std::vector<shared_ptr<AnalyzerNode>> args;

        vector <shared_ptr<AnalyzerNode>> children() override {
            vector<shared_ptr<AnalyzerNode>> c = {macro};

            for (auto a: args) {
                c.push_back(a);
            }

            return c;
        }

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeMacroExpand;
        }
    };

    class MaybeInvokeAnalyzerNode : public AnalyzerNode {
    public:
        /// Function to call
        shared_ptr<AnalyzerNode> fn;

        /// Function call arguments
        std::vector<shared_ptr<AnalyzerNode>> args;

        vector <shared_ptr<AnalyzerNode>> children() override {
            vector<shared_ptr<AnalyzerNode>> c = {fn};

            for (auto a: args) {
                c.push_back(a);
            }

            return c;
        }

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeMaybeInvoke;
        }
    };

    enum FFIType : int64_t {
        kFFITypeUnknown = -1,
        kFFITypeElectrumValue
    };

    static FFIType ffi_type_from_keyword(string input) {
        if (input == "el") {
            return kFFITypeElectrumValue;
        }

        return kFFITypeUnknown;
    }

    class DefFFIFunctionNode : public AnalyzerNode {
    public:
        /// Binding name
        shared_ptr<string> binding;

        /// Function name
        shared_ptr<string> func_name;

        /// Return type
        FFIType return_type;

        vector <FFIType> arg_types;

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeDefFFIFunction;
        }
    };

    class EvalWhenAnalyzerNode : public AnalyzerNode {
    public:
        /// Which evaluation phases the body will be evaluated in
        EvaluationPhase phases;

        /// All but the last body form
        vector <shared_ptr<AnalyzerNode>> body;

        /// The last body form (returned value)
        shared_ptr<AnalyzerNode> last;

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeEvalWhen;
        }

        vector <shared_ptr<AnalyzerNode>> children() override {
            vector<shared_ptr<AnalyzerNode>> c;

            for (const auto &b: body) {
                c.push_back(b);
            }

            c.push_back(last);

            return c;
        }
    };

    class Analyzer {
    public:
        Analyzer();

        shared_ptr<AnalyzerNode> analyze(shared_ptr<ASTNode> form, uint64_t depth = 0, EvaluationPhase phase = kEvaluationPhaseLoadTime);

        shared_ptr<AnalyzerNode> initialBindingWithName(const std::string &name);

        vector<shared_ptr<AnalyzerNode>> collapse_top_level_forms(shared_ptr<AnalyzerNode> node);
    private:

        shared_ptr<AnalyzerNode> analyzeForm(shared_ptr<ASTNode> form);

        vector <string> analyze_closed_overs(shared_ptr <AnalyzerNode> node);

        void run_passes(shared_ptr<AnalyzerNode> node, uint64_t depth = 0);

        void update_depth_for_node(shared_ptr<AnalyzerNode> node, uint64_t starting_depth = 0);

        void assert_eval_when_for_compile_is_top_level(shared_ptr<AnalyzerNode> node);

        void update_evaluation_phase(shared_ptr<AnalyzerNode> node, EvaluationPhase phase);

        shared_ptr<AnalyzerNode> analyzeSymbol(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeInteger(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeFloat(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeString(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeNil(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeKeyword(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeBoolean(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeList(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeIf(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeDo(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeLambda(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeMacro(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeMacroExpand(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeDef(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeMaybeInvoke(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeDefFFIFn(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeQuote(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeEvalWhen(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> maybeAnalyzeSpecialForm(shared_ptr<string> symbolName, shared_ptr<ASTNode> form);

        typedef shared_ptr<AnalyzerNode> (Analyzer::*AnalyzerFunc)(shared_ptr<ASTNode>);

        void push_local_env();

        void pop_local_env();

        shared_ptr<AnalyzerNode> lookup_in_local_env(std::string name);

        void store_in_local_env(std::string name, shared_ptr<AnalyzerNode> initialValue);


        void push_evaluation_phase(EvaluationPhase phase);

        EvaluationPhase pop_evaluation_phase();

        EvaluationPhase current_evaluation_phase();

        /// Analysis functions for special forms
        const std::unordered_map<std::string, AnalyzerFunc> specialForms{
                {"if",          &Analyzer::analyzeIf},
                {"do",          &Analyzer::analyzeDo},
                {"lambda",      &Analyzer::analyzeLambda},
                {"defmacro",    &Analyzer::analyzeMacro},
                {"def",         &Analyzer::analyzeDef},
                {"def-ffi-fn*", &Analyzer::analyzeDefFFIFn},
                {"quote",       &Analyzer::analyzeQuote},
                {"eval-when",   &Analyzer::analyzeEvalWhen}
        };

        struct AnalyzerDefinition {
            EvaluationPhase phase;
            shared_ptr<AnalyzerNode> node;
        };

        /// Holds global macros
        std::unordered_map<std::string, shared_ptr<AnalyzerNode>> global_macros_;

        /// Holds already defined globals
        std::unordered_map<std::string, AnalyzerDefinition> global_env_;
        vector <unordered_map<string, shared_ptr<AnalyzerNode>>> local_envs_;

        /// Flag to specify whether the analyzer is inside a quoted form
        bool is_quoting_;

        /// Flag to specify whether the analyzer is currently inside a quasiquoted form
        bool is_quasi_quoting_;

        /// Flag to specify whether the analyzer is currently analyzing a macro expander
        bool in_macro_;

        /// Stack to hold evaluation environments
        vector<EvaluationPhase> evaluation_phases_;
    };
}


#endif //ELECTRUM_ANALYZER_H
