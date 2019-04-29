//
// Created by andybest on 10/04/19.
//

#include <linenoise.h>
#include <csignal>
#include <iostream>
#include <string>
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
        case SIGINT:
            std::cout << "Handled SIGINT" << std::endl;
            exit(signum);
        default:
            std::cout << "Unhandled signal" << std::endl;
    }
}

void loadStdlib(electrum::Compiler *c) {
    std::ifstream t("../../../stdlib/stdlib.el");
    std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());

    c->compileAndEvalString(str);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, sigHandler);

    rt_init_gc(electrum::kGCModeInterpreterOwned);
    electrum::Compiler c;
    loadStdlib(&c);

    while (!done) {
        auto input = linenoise("> ");
        if(input == nullptr) { continue; }
        if(strcmp(input, "(quit)") == 0) { done = true; continue; }

        linenoiseHistoryAdd(input);

        try {
            auto rv = c.compileAndEvalString(input);
            electrum::print_expr(rv);
        }
        catch (electrum::CompilerException &e) {
            std::cout << "Error: " << e.what() << std::endl;
            std::cout << "\t" << *e.sourcePosition()->filename << ":"
                      << e.sourcePosition()->line << ":"
                      << e.sourcePosition()->column << std::endl;
        }
        catch (...) {
            std::cout << "Uncaught error" << std::endl;
        }

    }

    rt_deinit_gc();

    return 0;
}
