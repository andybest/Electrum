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

#ifndef ELECTRUM_EVALUATIONPHASE_H
#define ELECTRUM_EVALUATIONPHASE_H

#include <boost/detail/bitmask.hpp>

/// Values that represent the phases in which the currently compiling expression will be evaluated in
enum EvaluationPhase {
  /// No evaluation
          kEvaluationPhaseNone = 0U,

  /// The expression will be evaluated at load time
          kEvaluationPhaseLoadTime = 1U << 0U,

  /// The expression will be evaluated at compile time
          kEvaluationPhaseCompileTime = 1U << 1U
};

BOOST_BITMASK(EvaluationPhase)

#endif //ELECTRUM_EVALUATIONPHASE_H
