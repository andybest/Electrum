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
                case kTokenTypeString: return make_pair(parseString(t), it);
                case kTokenTypeLParen: {
                    return parseList(tokens, ++it);
                }

                default:break;
            }

            ++it;
        }
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

    shared_ptr<ASTNode> Parser::parseBoolean(const Token &t) const {
        auto val = make_shared<ASTNode>();
        val->tag = kTypeTagBoolean;

        if(t.text == "#t" || t.text == "#true") {
            val->booleanValue = true;
        } else {
            val->booleanValue = false;
        }

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

    shared_ptr<ASTNode> Parser::parseString(const Token &t) const {
        auto val = make_shared<ASTNode>();

        // TODO: Parse escapes
        val->tag = kTypeTagString;
        val->stringValue = make_shared<string>(t.text);

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


}