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


#ifndef ELECTRUM_PARSER_H
#define ELECTRUM_PARSER_H

#include "types/Types.h"
#include <memory>
#include <string>
#include "LexerDefs.h"

namespace electrum {
using std::shared_ptr;
using std::make_shared;
using std::string;
using std::pair;
using std::make_pair;

class Parser {

    enum QuoteType {
      kQuoteTypeQuote,
      kQuoteTypeQuasiQuote,
      kQuoteTypeUnquote,
      kQuoteTypeUnquoteSplice
    };

    pair<shared_ptr<ASTNode>, vector<Token>::iterator> readTokens(vector<Token, std::allocator<Token>>* tokens,
            vector<Token>::iterator it) const;

    shared_ptr<ASTNode> parseInteger(const electrum::Token& t) const;
    shared_ptr<ASTNode> parseFloat(const Token& t) const;
    shared_ptr<ASTNode> parseSymbol(const Token& t) const;
    std::pair<shared_ptr<ASTNode>, vector<Token>::iterator> parseList(vector<Token>* tokens,
            vector<Token>::iterator it) const;
    pair<shared_ptr<ASTNode>, vector<Token>::iterator> parseQuote(vector<Token>* const tokens,
            vector<Token>::iterator it,
            QuoteType quote_type) const;

public:
    shared_ptr<ASTNode> readString(const string& input, const string& filename) const;
    shared_ptr<ASTNode> parseString(const Token& t) const;
    shared_ptr<ASTNode> parseBoolean(const Token& t) const;
    shared_ptr<ASTNode> parseKeyword(const Token& t) const;
    shared_ptr<ASTNode> parseNil(const Token& t) const;
    shared_ptr<ASTNode> readLispValue(void* val, const shared_ptr<SourcePosition>& ptr);
};
}

#endif //ELECTRUM_PARSER_H
