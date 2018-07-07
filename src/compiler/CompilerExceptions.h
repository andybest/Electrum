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

#ifndef ELECTRUM_COMPILEREXCEPTIONS_H
#define ELECTRUM_COMPILEREXCEPTIONS_H

#include <exception>
#include <memory>
#include <string>
#include "types/Types.h"

namespace electrum {
    class CompilerException : public std::exception {
    public:
        explicit CompilerException(const char *message,
                                   shared_ptr<SourcePosition> sourcePosition) : message_(message),
                                                                                sourcePosition_(sourcePosition) {
        }

        explicit CompilerException(std::string message,
                                   shared_ptr<SourcePosition> sourcePosition) : message_(message),
                                                                                sourcePosition_(sourcePosition) {
        }

        virtual ~CompilerException() throw() {}

        virtual const char *what() const throw() {
            return message_.c_str();
        }

        std::shared_ptr<SourcePosition> sourcePosition() {
            return sourcePosition_;
        }

    protected:
        std::string message_;
        std::shared_ptr<SourcePosition> sourcePosition_;
    };
}

#endif //ELECTRUM_COMPILEREXCEPTIONS_H
