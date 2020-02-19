// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"
#include <map>
#include <string>

namespace Consensus {

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    int nCoinbaseMaturity;

    // TODO: Implement the following parameters
    uint256 powLimit;
    uint256 posLimitv1;
    uint256 posLimitv2;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nTargetSpacing;
    int64_t nTargetTimespan;

    int CoinbaseMaturity() const { return nCoinbaseMaturity; }

    // TODO: Implement the following methods
    int64_t DifficultyAdjustmentInterval() const { return nTargetTimespan / nTargetSpacing; }
    uint256 ProofOfWorkLimit() const { return powLimit; }
    uint256 ProofOfStakeLimit(const bool fV2) const { return fV2 ? posLimitv2 : posLimitv1; }
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
