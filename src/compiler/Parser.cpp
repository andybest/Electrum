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
#include "CompilerExceptions.h"
#include <lex.yy.h>
#include <Runtime.h>

namespace electrum {

shared_ptr<ASTNode> Parser::readString(const string& input, const string& filename) const {
    auto lexer = Lexer(input);
    lexer.filename = std::make_shared<string>(filename);
    auto tokens = lexer.getTokens();

    auto result = readTokens(&tokens, tokens.begin());

    // TODO: Handle multiple expressions

    return result.first;
}

pair<shared_ptr<ASTNode>, vector<Token>::iterator> Parser::readTokens(vector<Token>* tokens,
        vector<Token>::iterator it) const {
    while (it != tokens->end()) {
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
        case kTokenTypeUnquoteSplice: {
            return parseQuote(tokens, ++it, kQuoteTypeUnquoteSplice);
        }
        case kTokenTypeLParen: {
            return parseList(tokens, ++it);
        }
        case kTokenTypeRParen: {
            auto pos = make_shared<SourcePosition>();
            pos->line     = t.line;
            pos->column   = t.column;
            pos->filename = t.filename;

            throw ParserException(kParserExceptionUnexpectedRParen, "Unexpected right paren", pos);
        }

        default:break;
        }

        ++it;
    }

    throw std::exception();
}

shared_ptr<ASTNode> Parser::parseInteger(const Token& t) const {
    auto val = make_shared<electrum::ASTNode>();
    val->tag          = kTypeTagInteger;
    val->integerValue = stoll(t.text);

    val->sourcePosition           = make_shared<SourcePosition>();
    val->sourcePosition->line     = t.line;
    val->sourcePosition->column   = t.column;
    val->sourcePosition->filename = t.filename;
    return val;
}

shared_ptr<ASTNode> Parser::parseFloat(const Token& t) const {
    auto val = make_shared<ASTNode>();
    val->tag        = kTypeTagFloat;
    val->floatValue = std::stod(t.text);

    val->sourcePosition           = make_shared<SourcePosition>();
    val->sourcePosition->line     = t.line;
    val->sourcePosition->column   = t.column;
    val->sourcePosition->filename = t.filename;
    return val;
}

shared_ptr<ASTNode> Parser::parseNil(const Token& t) const {
    auto val = make_shared<ASTNode>();
    val->tag = kTypeTagNil;

    val->sourcePosition           = make_shared<SourcePosition>();
    val->sourcePosition->line     = t.line;
    val->sourcePosition->column   = t.column;
    val->sourcePosition->filename = t.filename;
    return val;
}

shared_ptr<ASTNode> Parser::parseBoolean(const Token& t) const {
    auto val = make_shared<ASTNode>();
    val->tag = kTypeTagBoolean;

    val->booleanValue = t.text == "#t" || t.text == "#true";

    val->sourcePosition           = make_shared<SourcePosition>();
    val->sourcePosition->line     = t.line;
    val->sourcePosition->column   = t.column;
    val->sourcePosition->filename = t.filename;
    return val;
}

shared_ptr<ASTNode> Parser::parseSymbol(const Token& t) const {
    auto val = make_shared<ASTNode>();
    val->tag         = kTypeTagSymbol;
    val->stringValue = make_shared<string>(t.text);

    val->sourcePosition           = make_shared<SourcePosition>();
    val->sourcePosition->line     = t.line;
    val->sourcePosition->column   = t.column;
    val->sourcePosition->filename = t.filename;
    return val;
}

shared_ptr<ASTNode> Parser::parseKeyword(const Token& t) const {
    auto val = make_shared<ASTNode>();
    val->tag = kTypeTagKeyword;

    // Remove the colon
    val->stringValue = make_shared<string>(t.text.substr(1, t.text.size() - 1));

    val->sourcePosition           = make_shared<SourcePosition>();
    val->sourcePosition->line     = t.line;
    val->sourcePosition->column   = t.column;
    val->sourcePosition->filename = t.filename;
    return val;
}

shared_ptr<ASTNode> Parser::parseString(const Token& t) const {
    auto val = make_shared<ASTNode>();

    // TODO: Parse escapes
    val->tag = kTypeTagString;

    // Remove the opening and closing quotes
    val->stringValue = make_shared<string>(t.text.substr(1, t.text.size() - 2));

    val->sourcePosition           = make_shared<SourcePosition>();
    val->sourcePosition->line     = t.line;
    val->sourcePosition->column   = t.column;
    val->sourcePosition->filename = t.filename;
    return val;
}

pair<shared_ptr<ASTNode>, vector<Token>::iterator> Parser::parseList(vector<Token>* tokens,
        vector<Token>::iterator it) const {
    auto list = make_shared<vector<shared_ptr<ASTNode>>>();

    while (it != tokens->end()) {
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
            val->tag       = kTypeTagList;
            val->listValue = list;

            val->sourcePosition           = make_shared<SourcePosition>();
            val->sourcePosition->line     = t.line;
            val->sourcePosition->column   = t.column;
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
    auto lastToken = *(it - 1);
    auto pos       = make_shared<SourcePosition>();
    pos->column   = lastToken.column;
    pos->line     = lastToken.line;
    pos->filename = lastToken.filename;

    throw ParserException(kParserExceptionMissingRParen, "Missing right paren.", pos);
}

pair<shared_ptr<ASTNode>, vector<Token>::iterator> Parser::parseQuote(vector<Token>* const tokens,
        vector<Token>::iterator it,
        QuoteType quote_type) const {
    auto list = make_shared<vector<shared_ptr<ASTNode>>>();

    auto t = *it;

    // Add 'quote' symbol to head of list
    auto sym = make_shared<ASTNode>();
    sym->tag = kTypeTagSymbol;

    switch (quote_type) {
    case kQuoteTypeQuote:sym->stringValue = make_shared<string>("quote");
        break;

    case kQuoteTypeQuasiQuote:sym->stringValue = make_shared<string>("quasiquote");
        break;

    case kQuoteTypeUnquote:sym->stringValue = make_shared<string>("unquote");
        break;

    case kQuoteTypeUnquoteSplice:sym->stringValue = make_shared<string>("unquote-splice");
        break;
    }

    sym->sourcePosition           = make_shared<SourcePosition>();
    sym->sourcePosition->line     = t.line;
    sym->sourcePosition->column   = t.column;
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
        auto pos = make_shared<SourcePosition>();
        pos->line     = t.line;
        pos->column   = t.column;
        pos->filename = t.filename;

        throw ParserException(kParserExceptionUnexpectedRParen, "Unexpected right paren", pos);
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
    val->tag       = kTypeTagList;
    val->listValue = list;

    val->sourcePosition           = make_shared<SourcePosition>();
    val->sourcePosition->line     = t.line;
    val->sourcePosition->column   = t.column;
    val->sourcePosition->filename = t.filename;
    return std::make_pair(val, it);
}

shared_ptr<ASTNode> Parser::readLispValue(void* val, const shared_ptr<SourcePosition>& source_position) {
    auto form = make_shared<ASTNode>();

    if (rt_is_integer(val) == TRUE_PTR) {
        form->tag          = kTypeTagInteger;
        form->integerValue = rt_integer_value(val);
    }
    else if (rt_is_float(val) == TRUE_PTR) {
        form->tag        = kTypeTagFloat;
        form->floatValue = rt_float_value(val);
    }
    else if (rt_is_boolean(val) == TRUE_PTR) {
        form->tag          = kTypeTagBoolean;
        form->booleanValue = val == TRUE_PTR;
    }
    else if (rt_is_string(val) == TRUE_PTR) {
        form->tag         = kTypeTagString;
        form->stringValue = make_shared<string>(rt_string_value(val));
    }
    else if (rt_is_keyword(val) == TRUE_PTR) {
        form->tag = kTypeTagKeyword;
        form->stringValue = make_shared<string>(rt_keyword_extract_string(val));
    }
    else if (rt_is_symbol(val) == TRUE_PTR) {
        form->tag         = kTypeTagSymbol;
        form->stringValue = make_shared<string>(rt_symbol_extract_string(val));
    }
    else if (rt_is_pair(val) == TRUE_PTR) {
        form->tag = kTypeTagList;

        auto list = make_shared<vector<shared_ptr<ASTNode>>>();

        auto head = val;
        while (rt_is_pair(head) == TRUE_PTR) {
            auto current = rt_car(head);
            list->push_back(readLispValue(current, source_position));

            head = rt_cdr(head);
        }

        if (head != NIL_PTR) {
            list->push_back(readLispValue(head, source_position));
        }

        form->listValue = list;
    }
    else if (val == NIL_PTR) {
        form->tag = kTypeTagNil;
    }
    else {
        if (is_object(val)) {
            auto obj = TAG_TO_OBJECT(val);
            std::cout << "Object: " << obj->tag << std::endl;
        }
        print_expr(val);
        throw std::exception();
    }

    form->sourcePosition = source_position;
    return form;
}
}