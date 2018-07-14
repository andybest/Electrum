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

#ifndef ELECTRUM_NAMESPACE_H
#define ELECTRUM_NAMESPACE_H

#include <unordered_map>
#include <memory>
#include <string>

namespace electrum {

    using std::unordered_map;
    using std::make_shared;
    using std::shared_ptr;

    class ENamespace {
    public:
        ENamespace(std::string name);

        //void* lookupBindingWithName(char *binding);
        //void* lookupBindingWithSymbol(void *symbol);
    private:
        std::string name;
        unordered_map<std::string, void*> mappings_;
    };

    shared_ptr<unordered_map<std::string, shared_ptr<ENamespace>>> global_namespaces;

    void init_global_namespaces();
};


#endif //ELECTRUM_NAMESPACE_H
