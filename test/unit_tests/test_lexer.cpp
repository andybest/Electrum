#include "gtest/gtest.h"
#include "lexer/LexerDefs.h"
#include "lex.yy.h"
#include "LexerHelpers.h"

using namespace electrum;

#define TOKENIZE_STRING(str)    Lexer l((str)); auto tokens = l.getTokens()

TEST(Lexer, lexes_simple_integers) {
    TOKENIZE_STRING("1 234 30456");

    EXPECT_EQ(tokens.size(), 3);

    ASSERT_TOKEN(tokens[0], kTokenTypeInteger, "1");
    ASSERT_TOKEN(tokens[1], kTokenTypeInteger, "234");
    ASSERT_TOKEN(tokens[2], kTokenTypeInteger, "30456");
}

TEST(Lexer, lexes_simple_floats) {
    TOKENIZE_STRING("0.1 0.0 1234.5678 1234.0 0.1234");

    EXPECT_EQ(tokens.size(), 5);

    ASSERT_TOKEN(tokens[0], kTokenTypeFloat, "0.1");
    ASSERT_TOKEN(tokens[1], kTokenTypeFloat, "0.0");
    ASSERT_TOKEN(tokens[2], kTokenTypeFloat, "1234.5678");
    ASSERT_TOKEN(tokens[3], kTokenTypeFloat, "1234.0");
    ASSERT_TOKEN(tokens[4], kTokenTypeFloat, "0.1234");
}

TEST(Lexer, lexes_parens) {
    TOKENIZE_STRING("( ) ()");

    EXPECT_EQ(tokens.size(), 4);

    ASSERT_TOKEN(tokens[0], kTokenTypeLParen, "(");
    ASSERT_TOKEN(tokens[1], kTokenTypeRParen, ")");
    ASSERT_TOKEN(tokens[2], kTokenTypeLParen, "(");
    ASSERT_TOKEN(tokens[3], kTokenTypeRParen, ")");
}

TEST(Lexer, lexes_symbols) {
    TOKENIZE_STRING("... +soup+ ->string lambda q <=?");

    EXPECT_EQ(tokens.size(), 6);

    ASSERT_TOKEN(tokens[0], kTokenTypeSymbol, "...");
    ASSERT_TOKEN(tokens[1], kTokenTypeSymbol, "+soup+");
    ASSERT_TOKEN(tokens[2], kTokenTypeSymbol, "->string");
    ASSERT_TOKEN(tokens[3], kTokenTypeSymbol, "lambda");
    ASSERT_TOKEN(tokens[4], kTokenTypeSymbol, "q");
    ASSERT_TOKEN(tokens[5], kTokenTypeSymbol, "<=?");
}

TEST(Lexer, lexes_unicode_symbols) {
    TOKENIZE_STRING(u8"λ \U0001F300 \U0001F900 \U0001F600 \U0001F680");

    EXPECT_EQ(tokens.size(), 5);

    ASSERT_TOKEN(tokens[0], kTokenTypeSymbol, u8"λ");
    ASSERT_TOKEN(tokens[1], kTokenTypeSymbol, u8"\U0001F300"); // Misc symbols & pictographs
    ASSERT_TOKEN(tokens[2], kTokenTypeSymbol, u8"\U0001F900"); // Supplemental symbols & pictographs
    ASSERT_TOKEN(tokens[3], kTokenTypeSymbol, u8"\U0001F600"); // Emoticons
    ASSERT_TOKEN(tokens[4], kTokenTypeSymbol, u8"\U0001F680"); // Transport & map symbols
}

TEST(Lexer, lexes_quote_quasiquote_unquote_spliceunquote) {
    TOKENIZE_STRING("`(foo ,bar 'baz ,@rest)");

    EXPECT_EQ(tokens.size(), 10);

    ASSERT_TOKEN(tokens[0], kTokenTypeQuasiQuote, "`");
    ASSERT_TOKEN(tokens[1], kTokenTypeLParen, "(");
    ASSERT_TOKEN(tokens[2], kTokenTypeSymbol, "foo");
    ASSERT_TOKEN(tokens[3], kTokenTypeUnquote, ",");
    ASSERT_TOKEN(tokens[4], kTokenTypeSymbol, "bar");
    ASSERT_TOKEN(tokens[5], kTokenTypeQuote, "'");
    ASSERT_TOKEN(tokens[6], kTokenTypeSymbol, "baz");
    ASSERT_TOKEN(tokens[7], kTokenTypeSpliceUnquote, ",@");
    ASSERT_TOKEN(tokens[8], kTokenTypeSymbol, "rest");
    ASSERT_TOKEN(tokens[9], kTokenTypeRParen, ")");
}