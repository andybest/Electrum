//
// Created by Andy Best on 07/07/2018.
//

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
