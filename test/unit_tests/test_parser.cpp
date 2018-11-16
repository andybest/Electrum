
#include "gtest/gtest.h"
#include "compiler/Parser.h"
#include "types/Types.h"

#define PARSE_STRING(s) Parser p; auto val = p.readString((s))
#define ASSERT_INT(v, intVal) EXPECT_EQ((v)->tag, kTypeTagInteger); EXPECT_EQ((v)->integerValue, intVal)
#define ASSERT_FLOAT(v, floatVal) EXPECT_EQ((v)->tag, kTypeTagFloat); EXPECT_FLOAT_EQ(v->floatValue, floatVal)

using namespace electrum;

TEST(Parser, parses_integer) {
    PARSE_STRING("1");
    ASSERT_INT(val, 1);
}

TEST(Parser, parses_float) {
    PARSE_STRING("1.2345");
    ASSERT_FLOAT(val, 1.2345);
}

TEST(Parser, parses_symbol) {
    PARSE_STRING("lambda");

    EXPECT_EQ(val->tag, kTypeTagSymbol);
    EXPECT_EQ(*val->stringValue, "lambda");
}

TEST(Parser, parses_string) {
    PARSE_STRING("\"foo\"");

    EXPECT_EQ(val->tag, kTypeTagString);
    EXPECT_EQ(*val->stringValue, "foo");
}

TEST(Parser, parses_keyword) {
    PARSE_STRING(":foo");

    EXPECT_EQ(val->tag, kTypeTagKeyword);
    EXPECT_EQ(*val->stringValue, "foo");
}

TEST(Parser, parses_list) {
    PARSE_STRING("(1 2 3)");

    EXPECT_EQ(val->tag, kTypeTagList);
    EXPECT_EQ(val->listValue->size(), 3);

    ASSERT_INT(val->listValue->at(0), 1);
    ASSERT_INT(val->listValue->at(1), 2);
    ASSERT_INT(val->listValue->at(2), 3);
}

TEST(Parser, parses_nested_list) {
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