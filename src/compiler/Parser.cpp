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

#include "Parser.h"
#include <lex.yy.h>

namespace electrum {

    std::shared_ptr<ASTNode> Parser::readString(const string input) const {
        auto lexer = Lexer(input);
        auto tokens = lexer.getTokens();

        return readTokens(tokens, tokens.begin()).first;
    }

    pair<shared_ptr<ASTNode>, vector<Token>::iterator> Parser::readTokens(vector<Token> tokens,
                                                                          vector<Token>::iterator it) const {
        while (it != tokens.end()) {
            auto t = *it;

            switch (t.type) {
                case kTokenTypeInteger: return make_pair(parseInteger(t), it);
                case kTokenTypeFloat: return make_pair(parseFloat(t), it);
                case kTokenTypeBoolean: return make_pair(parseBoolean(t), it);
                case kTokenTypeSymbol: return make_pair(parseSymbol(t), it);
                case kTokenTypeKeyword: return make_pair(parseKeyword(t), it);
                case kTokenTypeString: return make_pair(parseString(t), it);
                case kTokenTypeNil: return make_pair(parseNil(t), it);
                case kTokenTypeQuote: {
                    return parseQuote(tokens, ++it, kQuoteTypeQuote);
                }
                case kTokenTypeQuasiQuote: {
                    return parseQuote(tokens, ++it, kQuoteTypeQuasiQuote);
                }
                case kTokenTypeUnquote: {
                    return parseQuote(tokens, ++it, kQuoteTypeUnquote);
                }
                case kTokenTypeLParen: {
                    return parseList(tokens, ++it);
                }

                default:break;
            }

            ++it;
        }

        throw std::exception();
    }

    shared_ptr<ASTNode> Parser::parseInteger(const Token &t) const {
        auto val = make_shared<electrum::ASTNode>();
        val->tag = kTypeTagInteger;
        val->integerValue = stoll(t.text);

        val->sourcePosition = make_shared<SourcePosition>();
        val->sourcePosition->line = t.line;
        val->sourcePosition->column = t.column;
        val->sourcePosition->filename = t.filename;
        return val;
    }

    shared_ptr<ASTNode> Parser::parseFloat(const Token &t) const {
        auto val = make_shared<ASTNode>();
        val->tag = kTypeTagFloat;
        val->floatValue = std::stod(t.text);

        val->sourcePosition = make_shared<SourcePosition>();
        val->sourcePosition->line = t.line;
        val->sourcePosition->column = t.column;
        val->sourcePosition->filename = t.filename;
        return val;
    }

    shared_ptr<ASTNode> Parser::parseNil(const Token &t) const {
        auto val = make_shared<ASTNode>();
        val->tag = kTypeTagNil;

        val->sourcePosition = make_shared<SourcePosition>();
        val->sourcePosition->line = t.line;
        val->sourcePosition->column = t.column;
        val->sourcePosition->filename = t.filename;
        return val;
    }

    shared_ptr<ASTNode> Parser::parseBoolean(const Token &t) const {
        auto val = make_shared<ASTNode>();
        val->tag = kTypeTagBoolean;

        val->booleanValue = t.text == "#t" || t.text == "#true";

        val->sourcePosition = make_shared<SourcePosition>();
        val->sourcePosition->line = t.line;
        val->sourcePosition->column = t.column;
        val->sourcePosition->filename = t.filename;
        return val;
    }

    shared_ptr<ASTNode> Parser::parseSymbol(const Token &t) const {
        auto val = make_shared<ASTNode>();
        val->tag = kTypeTagSymbol;
        val->stringValue = make_shared<string>(t.text);

        val->sourcePosition = make_shared<SourcePosition>();
        val->sourcePosition->line = t.line;
        val->sourcePosition->column = t.column;
        val->sourcePosition->filename = t.filename;
        return val;
    }

    shared_ptr<ASTNode> Parser::parseKeyword(const Token &t) const {
        auto val = make_shared<ASTNode>();
        val->tag = kTypeTagKeyword;

        // Remove the colon
        val->stringValue = make_shared<string>(t.text.substr(1, t.text.size() - 1));

        val->sourcePosition = make_shared<SourcePosition>();
        val->sourcePosition->line = t.line;
        val->sourcePosition->column = t.column;
        val->sourcePosition->filename = t.filename;
        return val;
    }

    shared_ptr<ASTNode> Parser::parseString(const Token &t) const {
        auto val = make_shared<ASTNode>();

        // TODO: Parse escapes
        val->tag = kTypeTagString;

        // Remove the opening and closing quotes
        val->stringValue = make_shared<string>(t.text.substr(1, t.text.size() - 2));

        val->sourcePosition = make_shared<SourcePosition>();
        val->sourcePosition->line = t.line;
        val->sourcePosition->column = t.column;
        val->sourcePosition->filename = t.filename;
        return val;
    }

    pair<shared_ptr<ASTNode>, vector<Token>::iterator> Parser::parseList(const vector<Token> tokens,
                                                                         vector<Token>::iterator it) const {
        auto list = make_shared<vector<shared_ptr<ASTNode>>>();

        while (it != tokens.end()) {
            auto t = *it;

            switch (t.type) {
                case kTokenTypeLParen: {
                    auto l = parseList(tokens, ++it);
                    it = l.second;
                    list->push_back(l.first);
                    break;
                }
                case kTokenTypeRParen: {
                    auto val = make_shared<ASTNode>();
                    val->tag = kTypeTagList;
                    val->listValue = list;

                    val->sourcePosition = make_shared<SourcePosition>();
                    val->sourcePosition->line = t.line;
                    val->sourcePosition->column = t.column;
                    val->sourcePosition->filename = t.filename;
                    return std::make_pair(val, it);
                }
                default: {
                    auto v = readTokens(tokens, it);
                    list->push_back(v.first);
                    it = v.second;
                    break;
                }
            }

            ++it;
        }

        // Error, list not closed- expected a right paren.
        throw std::exception();
    }

    pair<shared_ptr<ASTNode>, vector<Token>::iterator> Parser::parseQuote(const vector<Token> tokens,
                                                                          vector<Token>::iterator it,
                                                                          QuoteType quote_type) const {
        auto list = make_shared<vector<shared_ptr<ASTNode>>>();

        auto t = *it;

        // Add 'quote' symbol to head of list
        auto sym = make_shared<ASTNode>();
        sym->tag = kTypeTagSymbol;

        switch(quote_type) {
            case kQuoteTypeQuote:
                sym->stringValue = make_shared<string>("quote");
                break;

            case kQuoteTypeQuasiQuote:
                sym->stringValue = make_shared<string>("quasiquote");
                break;

            case kQuoteTypeUnquote:
                sym->stringValue = make_shared<string>("unquote");
                break;
        }


        sym->sourcePosition = make_shared<SourcePosition>();
        sym->sourcePosition->line = t.line;
        sym->sourcePosition->column = t.column;
        sym->sourcePosition->filename = t.filename;

        list->push_back(sym);

        t = *it;

        switch (t.type) {
            case kTokenTypeLParen: {
                auto l = parseList(tokens, ++it);
                it = l.second;
                list->push_back(l.first);
                break;
            }
            case kTokenTypeRParen: {
                // Unexpected close paren
                throw std::exception();
            }
            default: {
                auto v = readTokens(tokens, it);
                list->push_back(v.first);
                it = v.second;
                break;
            }
        }

        //it;

        auto val = make_shared<ASTNode>();
        val->tag = kTypeTagList;
        val->listValue = list;

        val->sourcePosition = make_shared<SourcePosition>();
        val->sourcePosition->line = t.line;
        val->sourcePosition->column = t.column;
        val->sourcePosition->filename = t.filename;
        return std::make_pair(val, it);
    }
}