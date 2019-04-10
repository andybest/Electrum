//
// Created by andybest on 10/04/19.
//

#include <linenoise.h>
#include <iostream>
#include "compiler/CompilerExceptions.h"
#include "runtime/GarbageCollector.h"
#include "runtime/Runtime.h"
#include "compiler/Compiler.h"

auto done = false;

int main(int argc, char* argv[]) {
    rt_init_gc(electrum::kGCModeInterpreterOwned);
    electrum::Compiler c;

    while (!done) {
        auto input = linenoise("> ");
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
