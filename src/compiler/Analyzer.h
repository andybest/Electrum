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
#include <memory>
#include <boost/variant.hpp>
#include <string>
#include <unordered_map>

namespace electrum {

    using std::shared_ptr;
    using std::make_shared;
    using std::string;

    enum AnalyzerNodeType {
        kAnalyzerNodeTypeNone = -1,
        kAnalyzerNodeTypeIf,
        kAnalyzerNodeTypeConstant,
        kAnalyzerNodeTypeDo
    };

    class AnalyzerNode {
    public:
        shared_ptr<SourcePosition> sourcePosition;
        virtual vector<shared_ptr<AnalyzerNode>> children() { return {}; };
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

        vector<shared_ptr<AnalyzerNode>> children() override {
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
        kAnalyzerConstantTypeString
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
     * Node that represents a 'do' form
     */
    class DoAnalyzerNode : public AnalyzerNode {
    public:
        /// A vector of all statements in the do form, except the return value
        vector<shared_ptr<AnalyzerNode>> statements;
        /// The last value in the do form
        shared_ptr<AnalyzerNode> returnValue;

        vector<shared_ptr<AnalyzerNode>> children() override {
            auto rv = vector<shared_ptr<AnalyzerNode>>(statements);
            rv.push_back(returnValue);
            return rv;
        }

        AnalyzerNodeType nodeType() override {
            return kAnalyzerNodeTypeDo;
        }
    };

    class Analyzer {
    public:
        Analyzer();

        shared_ptr<AnalyzerNode> analyzeForm(shared_ptr<ASTNode> form);

    private:
        shared_ptr<AnalyzerNode> analyzeInteger(shared_ptr<ASTNode> shared_ptr);

        shared_ptr<AnalyzerNode> analyzeFloat(shared_ptr<ASTNode> form);

        shared_ptr<AnalyzerNode> analyzeString(shared_ptr<ASTNode> form);

        shared_ptr <AnalyzerNode> analyzeBoolean(shared_ptr <ASTNode> form);

        shared_ptr <AnalyzerNode> analyzeList(shared_ptr <ASTNode> form);

        shared_ptr <AnalyzerNode> analyzeIf(shared_ptr <ASTNode> form);

        shared_ptr <AnalyzerNode> analyzeDo(shared_ptr <ASTNode> form);

        shared_ptr <AnalyzerNode> analyzeMaybeFunctionCall(shared_ptr <ASTNode> form);

        shared_ptr <AnalyzerNode> maybeAnalyzeSpecialForm(shared_ptr <string> symbolName, shared_ptr <ASTNode> form);

        typedef shared_ptr<AnalyzerNode> (Analyzer::*AnalyzerFunc)(shared_ptr<ASTNode>);

        /// Analysis functions for special forms
        const std::unordered_map<std::string, AnalyzerFunc> specialForms  {
                { "if", &Analyzer::analyzeIf },
                { "do", &Analyzer::analyzeDo }
        };
    };
}


#endif //ELECTRUM_ANALYZER_H
