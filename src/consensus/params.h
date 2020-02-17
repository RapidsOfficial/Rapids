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
    uint256 powLimit;
    uint256 posLimitV1;
    uint256 posLimitV2;
    int nCoinbaseMaturity;
    int64_t nTargetTimespan;
    int64_t nTargetTimespanV2;
    int64_t nTargetSpacing;
    int nTimeSlotLength;

    // Height based activations
    int height_start_BIP65;

    // TODO: Implement the following parameters
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;

    int64_t TargetTimespan(const bool fV2 = true) const { return fV2 ? nTargetTimespanV2 : nTargetTimespan; }
    uint256 ProofOfStakeLimit(const bool fV2) const { return fV2 ? posLimitV2 : posLimitV1; }
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
