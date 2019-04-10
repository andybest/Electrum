
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

#include <cassert>
#include <algorithm>
#include <utility>
#include "NamespaceManager.h"

namespace electrum {

std::shared_ptr<Namespace> NamespaceManager::getOrCreateNamespace(std::string ns) {
    auto result = namespaces.find(ns);
    if (result != namespaces.end()) {
        return result->second;
    }

    auto new_ns = std::make_shared<Namespace>(ns);
    namespaces[ns] = new_ns;
    return new_ns;
}

std::optional<Definition> NamespaceManager::lookupGlobalSymbolInNS(
        const std::shared_ptr<Namespace>& ns,
        const std::string& sym_name) {
    auto result = ns->global_definitions.find(sym_name);
    if (result != ns->global_definitions.end()) {
        return std::make_optional<Definition>(result->second);
    }
    return std::nullopt;
}

std::optional<Definition> NamespaceManager::lookupSymbolInNS(
        const std::shared_ptr<Namespace>& ns,
        std::optional<std::string> target_ns,
        const std::string& sym_name) {
    if (target_ns) {
        auto it = std::find_if(ns->ns_imports.begin(),
                ns->ns_imports.end(),
                [&](NamespaceImport i) {
                  if (i.alias.has_value()) {
                      return *i.alias == *target_ns;
                  }

                  return i.name == *target_ns;
                });

        if (it != ns->ns_imports.end()) {
            auto t_ns = getOrCreateNamespace(it->name);
            return lookupGlobalSymbolInNS(t_ns, sym_name);
        }

        // Fully qualified ns lookup
        auto result = namespaces.find(*target_ns);
        if(result != namespaces.end()) {
            auto t_ns = getOrCreateNamespace(*target_ns);
            return lookupGlobalSymbolInNS(t_ns, sym_name);
        }

        return std::nullopt;
    }

    // Lookup in this namespace's globals
    if (auto result = lookupGlobalSymbolInNS(ns, sym_name)) {
        return result;
    }

    // Finally, look up in imported symbols
    auto result = ns->symbol_imports.find(sym_name);
    if (result != ns->symbol_imports.end()) {
        auto import    = result->second;
        auto import_ns = getOrCreateNamespace(import.ns);
        return lookupGlobalSymbolInNS(import_ns, import.sym);
    }

    // Can't find symbol
    return std::nullopt;
}

bool NamespaceManager::addGlobalDefinition(
        const std::shared_ptr<Namespace>& ns,
        const std::string& sym_name,
        DefinitionType type,
        EvaluationPhase phase) {
    assert(ns != nullptr);

    if (ns->global_definitions.find(sym_name) != ns->global_definitions.end()) {
        // Symbol already defined
        return false;
    }

    Definition d;
    d.name  = sym_name;
    d.ns    = ns->name;
    d.type  = type;
    d.phase = phase;

    ns->global_definitions[sym_name] = d;
    return true;
}

bool NamespaceManager::importNS(
        const std::shared_ptr<Namespace>& source_ns,
        std::shared_ptr<Namespace> import_ns,
        std::optional<std::string> alias) {
    // If ns is has already been imported, return false
    auto ns = std::find_if(source_ns->ns_imports.begin(),
            source_ns->ns_imports.end(),
            [&](NamespaceImport i) {
              return i.name == import_ns->name;
            });

    if (ns != source_ns->ns_imports.end()) {
        return false;
    }

    NamespaceImport i;
    i.name  = import_ns->name;
    i.alias = std::move(alias);
    source_ns->ns_imports.push_back(i);

    return true;
}

NamespaceManager::ReturnCode NamespaceManager::importSymbol(
        const std::shared_ptr<Namespace>& dest_ns,
        const std::shared_ptr<Namespace>& source_ns,
        const std::string& symbol_name,
        const std::optional<std::string>& alias) {

    if(!lookupGlobalSymbolInNS(source_ns, symbol_name).has_value()) {
        return ReturnCode::SymbolNotFound;
    }

    SymbolImport s;
    s.ns = source_ns->name;
    s.sym = symbol_name;
    s.alias = alias;

    std::string import_sym;
    if(alias.has_value()) {
        import_sym = *alias;
    } else {
        import_sym = symbol_name;
    }

    if(dest_ns->symbol_imports.find(import_sym) != dest_ns->symbol_imports.end()) {
        return ReturnCode::SymbolAlreadyExists;
    }

    dest_ns->symbol_imports[import_sym] = s;

    return ReturnCode::NoErr;
}

}
