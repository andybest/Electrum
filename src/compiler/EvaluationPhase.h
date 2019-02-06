//
// Created by Andy Best on 2019-01-17.
//

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
