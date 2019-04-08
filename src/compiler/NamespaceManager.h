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

#ifndef ELECTRUM_NAMESPACEMANAGER_H
#define ELECTRUM_NAMESPACEMANAGER_H

#include <memory>
#include <string>
#include <unordered_map>

#include "Namespace.h"

namespace electrum {
class NamespaceManager {

public:
    enum class ReturnCode {
        NoErr,
        SymbolNotFound,
        SymbolAlreadyExists
    };

    /// Holds namespaces and their definitions
    std::unordered_map<std::string, std::shared_ptr<Namespace>> namespaces;

    std::optional<Definition> lookupSymbolInNS(
            const std::shared_ptr<Namespace>& ns,
            std::optional<std::string> target_ns,
            const std::string& sym_name);
    static std::optional<Definition> lookupGlobalSymbolInNS(
            const std::shared_ptr<Namespace>& ns,
            const std::string& sym_name);
    std::shared_ptr<Namespace> getOrCreateNamespace(std::string ns);
    bool addGlobalDefinition(const std::shared_ptr<Namespace>& ns,
            const std::string& sym_name,
            DefinitionType type,
            EvaluationPhase phase);
    bool
    importNS(const std::shared_ptr<Namespace>& source_ns, std::shared_ptr<Namespace> import_ns,
            std::optional<std::string> alias);
    ReturnCode importSymbol(const std::shared_ptr<Namespace>& dest_ns, const std::shared_ptr<Namespace>& source_ns, const std::string& symbol_name,
            const std::optional<std::string>& alias);
};
}

#endif //ELECTRUM_NAMESPACEMANAGER_H
