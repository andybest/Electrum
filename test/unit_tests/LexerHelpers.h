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


#ifndef ELECTRUM_LEXERHELPERS_H
#define ELECTRUM_LEXERHELPERS_H

#include "gtest/gtest.h"
#include "lexer/LexerDefs.h"

using namespace electrum;

std::string tokenTypeName(TokenType type) {
    switch (type) {
        case kTokenTypeNoToken: return "No token";
        case kTokenTypeLParen: return "kTokenTypeLParen";
        case kTokenTypeRParen: return "kTokenTypeRParen";
        case kTokenTypeQuote: return "kTokenTypeQuote";
        case kTokenTypeQuasiQuote: return "kTokenTypeQuasiQuote";
        case kTokenTypeSpliceUnquote: return "kTokenTypeSpliceUnquote";
        case kTokenTypeUnquote: return "kTokenTypeUnquote";
        case kTokenTypeSymbol: return "kTokenTypeSymbol";
        case kTokenTypeFloat: return "kTokenTypeFloat";
        case kTokenTypeInteger: return "kTokenTypeInteger";
        case kTokenTypeEOF: return "kTokenTypeEOF";
        case kTokenTypeKeyword: return "kTokenTypeKeyword";
        case kTokenTypeString: return "kTokenTypeString";
        case kTokenTypeBoolean: return "kTokenTypeBoolen";
    }
}

#define ASSERT_TOKEN(t, type, text) EXPECT_TRUE(AssertToken((t), (type), (text)));

::testing::AssertionResult AssertToken(Token t, TokenType type, std::string text) {
    auto failure = ::testing::AssertionFailure();
    auto fail = false;

    if(t.type != type) {
        failure << "Expected token of type " << tokenTypeName(type) << ", but got " << tokenTypeName(t.type);
        fail = true;
    }

    if(t.text != text) {
        if (strlen(failure.message()) > 0) {
            failure << std::endl;
        }

        failure << "Expected token text to be '" << t.text << "' but got '" << text << "'";
        fail = true;
    }

    if(fail) {
        return failure;
    }

    return ::testing::AssertionSuccess();
}

#endif //ELECTRUM_LEXERHELPERS_H
