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

#ifndef ELECTRUM_NAMESPACE_H
#define ELECTRUM_NAMESPACE_H

#include <optional>
#include <utility>
#include <vector>
#include <string>
#include <utility>
#include "EvaluationPhase.h"

namespace electrum {

enum DefinitionType {
  kDefinitionTypeUnknown = -1,
  kDefinitionTypeFunction,
  kDefinitionTypeMacro,
  kDefinitionTypeVariable,
};

struct Definition {
  DefinitionType type;
  EvaluationPhase phase;
  std::string ns;
  std::string name;
};

struct SymbolImport {
  /// Namespace of the symbol
  std::string ns;

  /// Name of the symbol
  std::string sym;

  /// Optional alias
  std::optional<std::string> alias;
};

struct NamespaceImport {
  std::string name;
  std::optional<std::string> alias;
};

struct Namespace {
  const std::string name;

  // Global definitions in this namespace
  std::unordered_map<std::string, Definition> global_definitions;

  // Imported namespaces
  std::vector<NamespaceImport> ns_imports;

  // Symbols imported from other namespaces
  std::unordered_map<std::string, SymbolImport> symbol_imports;

  explicit Namespace(std::string name): name(std::move(name)) {}
};

}

#endif //ELECTRUM_NAMESPACE_H
