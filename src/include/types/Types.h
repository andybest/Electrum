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


#ifndef ELECTRUM_TYPES_H
#define ELECTRUM_TYPES_H

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace electrum {

    using std::shared_ptr;
    using std::vector;
    using std::string;
    using std::unordered_map;

    enum TypeTag {
        kTypeTagInteger,
        kTypeTagFloat,
        kTypeTagBoolean,
        kTypeTagList,
        kTypeTagString,
        kTypeTagNil,
        kTypeTagSymbol,
        kTypeTagKeyword
    };

    struct SourcePosition {
        size_t line;
        size_t column;
        shared_ptr<string> filename;
    };

    struct ASTNode;

    struct LispFunction {
        /// List of binding names
        vector<string> bindings;

        /// If the function has a variable argument count
        bool isVararg;

        /// The binding for the rest of the arguments
        std::string varargBinding;

        /// The body of the function
        shared_ptr<ASTNode> bodyForm;
    };

    struct ASTNode {
        TypeTag tag;
        shared_ptr<SourcePosition> sourcePosition;

        union {
            int64_t integerValue;
            double floatValue;
            bool booleanValue;
        };

        shared_ptr<LispFunction> functionValue;
        shared_ptr<vector<shared_ptr<ASTNode>>> listValue;
        shared_ptr<string> stringValue;     // Used for string, symbol and keyword
    };

    struct Environment {
        shared_ptr<Environment> parent;
        unordered_map<std::string, shared_ptr<ASTNode>> bindings;
    };
}

#endif //ELECTRUM_TYPES_H
