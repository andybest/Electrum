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

#include "ENamespace.h"
#include "Runtime.h"

namespace electrum {
void init_global_namespaces() {
    global_namespaces = make_shared<
            unordered_map<std::string, shared_ptr<ENamespace>>>();
}

shared_ptr<ENamespace> get_or_create_namespace(const std::string& name) {
    auto result = global_namespaces->find(name);
    if (result!=global_namespaces->end()) {
        return result->second;
    }

    auto ns = std::make_shared<ENamespace>(name);
    (*global_namespaces)[name] = ns;
    return ns;
}

ENamespace::ENamespace(std::string name)
        :name(name) {
}
}