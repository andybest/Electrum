
#include "gtest/gtest.h"
#include "compiler/Parser.h"
#include "types/Types.h"

#define PARSE_STRING(s) Parser p; auto val = p.readString((s))
#define ASSERT_INT(v, intVal) EXPECT_EQ((v)->tag, kTypeTagInteger); EXPECT_EQ((v)->integerValue, intVal)
#define ASSERT_FLOAT(v, floatVal) EXPECT_EQ((v)->tag, kTypeTagFloat); EXPECT_FLOAT_EQ(v->floatValue, floatVal)

using namespace electrum;

TEST(Parser, parsesInteger) {
    PARSE_STRING("1");
    ASSERT_INT(val, 1);
}

TEST(Parser, parsesFloat) {
    PARSE_STRING("1.2345");
    ASSERT_FLOAT(val, 1.2345);
}

TEST(Parser, parsesSymbol) {
    PARSE_STRING("lambda");

    EXPECT_EQ(val->tag, kTypeTagSymbol);
    EXPECT_EQ(*val->stringValue, "lambda");
}

TEST(Parser, parsesString) {
    PARSE_STRING("\"foo\"");

    EXPECT_EQ(val->tag, kTypeTagString);
    EXPECT_EQ(*val->stringValue, "foo");
}

TEST(Parser, parsesKeyword) {
    PARSE_STRING(":foo");

    EXPECT_EQ(val->tag, kTypeTagKeyword);
    EXPECT_EQ(*val->stringValue, "foo");
}

TEST(Parser, parsesNil) {
    PARSE_STRING("nil");

    EXPECT_EQ(val->tag, kTypeTagNil);
}

TEST(Parser, parsesList) {
    PARSE_STRING("(1 2 3)");

    EXPECT_EQ(val->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->size(), 3);

    ASSERT_INT(val->listValue->at(0), 1);
    ASSERT_INT(val->listValue->at(1), 2);
    ASSERT_INT(val->listValue->at(2), 3);
}

TEST(Parser, parsesNestedList) {
    PARSE_STRING("((1 2) 3 (4 (5)))");

    EXPECT_EQ(val->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->size(), 3);

    EXPECT_EQ(val->listValue->at(0)->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->at(0)->listValue->size(), 2);

    EXPECT_EQ(val->listValue->at(2)->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->at(2)->listValue->size(), 2);

    EXPECT_EQ(val->listValue->at(2)->listValue->at(1)->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->at(2)->listValue->at(1)->listValue->size(), 1);
}

TEST(Parser, parsesQuoteReaderMacro) {
    PARSE_STRING("'foo");

    EXPECT_EQ(val->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->size(), 2);

    EXPECT_EQ(val->listValue->at(0)->tag, kTypeTagSymbol);
    EXPECT_EQ(*val->listValue->at(0)->stringValue, "quote");

    EXPECT_EQ(val->listValue->at(1)->tag, kTypeTagSymbol);
    EXPECT_EQ(*val->listValue->at(1)->stringValue, "foo");
}

TEST(Parser, parsesQuoteListReaderMacro) {
    PARSE_STRING("'(1 2)");

    EXPECT_EQ(val->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->size(), 2);

    EXPECT_EQ(val->listValue->at(0)->tag, kTypeTagSymbol);
    EXPECT_EQ(*val->listValue->at(0)->stringValue, "quote");

    EXPECT_EQ(val->listValue->at(1)->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->at(1)->listValue->size(), 2);

    EXPECT_EQ(val->listValue->at(1)->listValue->at(0)->tag, kTypeTagInteger);
    EXPECT_EQ(val->listValue->at(1)->listValue->at(0)->integerValue, 1);

    EXPECT_EQ(val->listValue->at(1)->listValue->at(1)->tag, kTypeTagInteger);
    EXPECT_EQ(val->listValue->at(1)->listValue->at(1)->integerValue, 2);
}

TEST(Parser, parsesQuotedQuote) {
    PARSE_STRING("'(1 'a)");

    // (quote (1 (quote a)))

    EXPECT_EQ(val->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->size(), 2);

    EXPECT_EQ(val->listValue->at(0)->tag, kTypeTagSymbol);
    EXPECT_EQ(*val->listValue->at(0)->stringValue, "quote");

    EXPECT_EQ(val->listValue->at(1)->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->at(1)->listValue->size(), 2);

    EXPECT_EQ(val->listValue->at(1)->listValue->at(0)->tag, kTypeTagInteger);
    EXPECT_EQ(val->listValue->at(1)->listValue->at(0)->integerValue, 1);

    EXPECT_EQ(val->listValue->at(1)->listValue->at(1)->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->at(1)->listValue->at(1)->listValue->size(), 2);

    EXPECT_EQ(val->listValue->at(1)->listValue->at(1)->listValue->at(0)->tag, kTypeTagSymbol);
    EXPECT_EQ(*val->listValue->at(1)->listValue->at(1)->listValue->at(0)->stringValue, "quote");
}

TEST(Parser, parsesQuasiquoteReaderMacro) {
    PARSE_STRING("`(1 2)");

    EXPECT_EQ(val->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->size(), 2);

    EXPECT_EQ(val->listValue->at(0)->tag, kTypeTagSymbol);
    EXPECT_EQ(*val->listValue->at(0)->stringValue, "quasiquote");

    EXPECT_EQ(val->listValue->at(1)->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->at(1)->listValue->size(), 2);

    EXPECT_EQ(val->listValue->at(1)->listValue->at(0)->tag, kTypeTagInteger);
    EXPECT_EQ(val->listValue->at(1)->listValue->at(0)->integerValue, 1);

    EXPECT_EQ(val->listValue->at(1)->listValue->at(1)->tag, kTypeTagInteger);
    EXPECT_EQ(val->listValue->at(1)->listValue->at(1)->integerValue, 2);
}

TEST(Parser, parsesUnquoteSplice) {
    PARSE_STRING(",@a");

    EXPECT_EQ(val->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->size(), 2);

    EXPECT_EQ(val->listValue->at(0)->tag, kTypeTagSymbol);
    EXPECT_EQ(*val->listValue->at(0)->stringValue, "unquote-splice");

    EXPECT_EQ(val->listValue->at(1)->tag, kTypeTagSymbol);
    EXPECT_EQ(*val->listValue->at(1)->stringValue, "a");
}