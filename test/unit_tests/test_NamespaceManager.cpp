/*
 MIT License

 Copyright (c) 2019 Andy Best

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

#include "gtest/gtest.h"
#include "compiler/NamespaceManager.h"
#include "compiler/Namespace.h"

using namespace electrum;

TEST(NamespaceManager, createsNewNamespace) {
    NamespaceManager m;

    EXPECT_EQ(m.namespaces.size(), 0);

    auto ns = m.getOrCreateNamespace("foo");
    EXPECT_EQ(m.namespaces.size(), 1);
    ASSERT_EQ(ns, m.namespaces["foo"]);

    EXPECT_EQ(ns->name, "foo");
}

TEST(NamespaceManager, findsGlobalSymbol) {
    NamespaceManager m;
    auto ns = m.getOrCreateNamespace("foo");

    EXPECT_TRUE(m.addGlobalDefinition(ns,
            "bar",
            kDefinitionTypeVariable,
            kEvaluationPhaseCompileTime));

    EXPECT_TRUE(m.lookupGlobalSymbolInNS(ns, "bar").has_value());
}

TEST(NamespaceManager, doesNotAddAlreadyExistingSymbol) {

    NamespaceManager m;
    auto ns = m.getOrCreateNamespace("foo");

    EXPECT_TRUE(m.addGlobalDefinition(ns,
            "bar",
            kDefinitionTypeVariable,
            kEvaluationPhaseCompileTime));

    EXPECT_FALSE(m.addGlobalDefinition(ns,
            "bar",
            kDefinitionTypeVariable,
            kEvaluationPhaseCompileTime));
}

TEST(NamespaceManager, findsSymbolInImportedNamespace) {
    NamespaceManager m;
    auto ns_foo = m.getOrCreateNamespace("foo");
    auto ns_bar = m.getOrCreateNamespace("bar");

    EXPECT_TRUE(m.addGlobalDefinition(ns_bar,
            "baz",
            kDefinitionTypeVariable,
            kEvaluationPhaseCompileTime));

    m.importNS(ns_foo, ns_bar, std::nullopt);
    auto def = m.lookupSymbolInNS(ns_foo, std::make_optional("bar"), "baz");

    EXPECT_TRUE(def.has_value());
}

TEST(NamespaceManager, findsSymbolInImportedNamespaceWithAlias) {
    NamespaceManager m;
    auto ns_foo = m.getOrCreateNamespace("foo");
    auto ns_bar = m.getOrCreateNamespace("bar");

    EXPECT_TRUE(m.addGlobalDefinition(ns_bar,
            "baz",
            kDefinitionTypeVariable,
            kEvaluationPhaseCompileTime));

    m.importNS(ns_foo, ns_bar, "bar-alias");
    auto def = m.lookupSymbolInNS(ns_foo, "bar-alias", "baz");
    EXPECT_TRUE(def.has_value());
}

TEST(NamespaceManager, findsImportedSymbol) {
    NamespaceManager m;
    auto ns_foo = m.getOrCreateNamespace("foo");
    auto ns_bar = m.getOrCreateNamespace("bar");

    EXPECT_TRUE(m.addGlobalDefinition(ns_bar,
            "baz",
            kDefinitionTypeVariable,
            kEvaluationPhaseCompileTime));

    auto rv = m.importSymbol(ns_foo, ns_bar, "baz", std::nullopt);
    ASSERT_EQ(rv, NamespaceManager::ReturnCode::NoErr);

    auto sym_result = m.lookupSymbolInNS(ns_foo, std::nullopt, "baz");
    ASSERT_TRUE(sym_result.has_value());

    EXPECT_EQ(sym_result->name, "baz");
    EXPECT_EQ(sym_result->ns, "bar");
}

TEST(NamespaceManager, findsImportedSymbolWithAlias) {
    NamespaceManager m;
    auto ns_foo = m.getOrCreateNamespace("foo");
    auto ns_bar = m.getOrCreateNamespace("bar");

    EXPECT_TRUE(m.addGlobalDefinition(ns_bar,
            "baz",
            kDefinitionTypeVariable,
            kEvaluationPhaseCompileTime));

    auto rv = m.importSymbol(ns_foo, ns_bar, "baz", "bazalias");
    ASSERT_EQ(rv, NamespaceManager::ReturnCode::NoErr);

    auto sym_result = m.lookupSymbolInNS(ns_foo, std::nullopt, "bazalias");
    ASSERT_TRUE(sym_result.has_value());

    EXPECT_EQ(sym_result->name, "baz");
    EXPECT_EQ(sym_result->ns, "bar");
}
