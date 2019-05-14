//
// Created by andybest on 10/04/19.
//

#include <linenoise.h>
#include <csignal>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <streambuf>
#include "compiler/CompilerExceptions.h"
#include "runtime/GarbageCollector.h"
#include "runtime/Runtime.h"
#include "compiler/Compiler.h"

auto done = false;

void sigHandler(int signum) {
    std::cout << "Got signal " << signum << std::endl;
    switch (signum) {
    case SIGINT:std::cout << "Handled SIGINT" << std::endl;
        exit(signum);
    default:std::cout << "Unhandled signal" << std::endl;
    }
}

void loadStdlib(electrum::Compiler* c) {
    std::ifstream t("../../../stdlib/stdlib.el");
    std::string   str((std::istreambuf_iterator<char>(t)),
            std::istreambuf_iterator<char>());

    c->compileAndEvalString(str);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, sigHandler);

    rt_init_gc(electrum::kGCModeInterpreterOwned);
    electrum::Compiler c;
    loadStdlib(&c);

    while (!done) {
        bool has_full_input           = false;

        std::stringstream input;
        auto              input_lines = 0;

        while (!has_full_input) {

            std::string ln_input;
            if (input_lines == 0) {
                ln_input = linenoise("  > ");
            }
            else {
                ln_input = linenoise("... ");
            }

            if (ln_input.empty()) { continue; }

            input << ln_input;

            if (input_lines == 0 && input.str() == "(quit)") {
                done = true;
                break;
            }

            input_lines += 1;

            try {
                auto rv = c.compileAndEvalString(input.str());
                std::cout << std::endl;
                rt_print(rv);
                std::cout << std::endl;
            }
            catch (electrum::CompilerException& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                std::cerr << "\t" << *e.sourcePosition()->filename << ":"
                          << e.sourcePosition()->line << ":"
                          << e.sourcePosition()->column << std::endl;
                break;
            }
            catch (electrum::ParserException& e) {
                if (e.exceptionType_ != electrum::kParserExceptionMissingRParen) {
                    std::cerr << "Error: " << e.what() << std::endl;
                    std::cerr << "\t" << *e.sourcePosition()->filename << ":"
                              << e.sourcePosition()->line << ":"
                              << e.sourcePosition()->column << std::endl;
                } else {
                    continue;
                }
            }
            catch (...) {
                std::cerr<< "Uncaught error" << std::endl;
                break;
            }

            has_full_input = true;
        }

        linenoiseHistoryAdd(input.str().c_str());
    }

    rt_deinit_gc();

    return 0;
}
