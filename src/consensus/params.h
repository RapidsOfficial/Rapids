// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "amount.h"
#include "libzerocoin/Params.h"
#include "uint256.h"
#include <map>
#include <string>

namespace Consensus {

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    bool fPowAllowMinDifficultyBlocks;
    uint256 powLimit;
    uint256 posLimitV1;
    uint256 posLimitV2;
    int nBudgetCycleBlocks;
    int nBudgetFeeConfirmations;
    int nCoinbaseMaturity;
    int nFutureTimeDriftPoW;
    int nFutureTimeDriftPoS;
    int nMasternodeCountDrift;
    CAmount nMaxMoneyOut;
    int nPoolMaxTransactions;
    int64_t nProposalEstablishmentTime;
    int nStakeMinAge;
    int nStakeMinDepth;
    int64_t nTargetTimespan;
    int64_t nTargetTimespanV2;
    int64_t nTargetSpacing;
    int nTimeSlotLength;

    // spork keys
    std::string strSporkPubKey;
    std::string strSporkPubKeyOld;
    int64_t nTime_EnforceNewSporkKey;
    int64_t nTime_RejectOldSporkKey;

    // height-based activations
    int height_last_PoW;
    int height_last_ZC_AccumCheckpoint;
    int height_last_ZC_WrappedSerials;
    int height_start_BIP65;                         // Blocks v5 start
    int height_start_InvalidUTXOsCheck;
    int height_start_MessSignaturesV2;
    int height_start_StakeModifierNewSelection;
    int height_start_StakeModifierV2;               // Blocks v6 start
    int height_start_TimeProtoV2;                   // Blocks v7 start
    int height_start_ZC;                            // Blocks v4 start
    int height_start_ZC_InvalidSerials;
    int height_start_ZC_PublicSpends;
    int height_start_ZC_SerialRangeCheck;
    int height_start_ZC_SerialsV2;
    int height_ZC_RecalcAccumulators;
    int height_new_client;

    // validation by-pass
    int64_t nPivxBadBlockTime;
    unsigned int nPivxBadBlockBits;


    int64_t TargetTimespan(const bool fV2 = true) const { return fV2 ? nTargetTimespanV2 : nTargetTimespan; }
    uint256 ProofOfStakeLimit(const bool fV2) const { return fV2 ? posLimitV2 : posLimitV1; }
    bool MoneyRange(const CAmount& nValue) const { return (nValue >= 0 && nValue <= nMaxMoneyOut); }
    bool IsMessSigV2(const int nHeight) const { return nHeight >= height_start_MessSignaturesV2; }
    bool IsTimeProtocolV2(const int nHeight) const { return nHeight >= height_start_TimeProtoV2; }

    int FutureBlockTimeDrift(const int nHeight) const
    {
        // PoS (TimeV2): 14 seconds
        if (IsTimeProtocolV2(nHeight)) return nTimeSlotLength - 1;
        // PoS (TimeV1): 3 minutes - PoW: 2 hours
        return (nHeight > height_last_PoW ? nFutureTimeDriftPoS : nFutureTimeDriftPoW);
    }

    bool IsValidBlockTimeStamp(const int64_t nTime, const int nHeight) const
    {
        // Before time protocol V2, blocks can have arbitrary timestamps
        if (!IsTimeProtocolV2(nHeight)) return true;
        // Time protocol v2 requires time in slots
        return (nTime % nTimeSlotLength) == 0;
    }

    bool HasStakeMinAgeOrDepth(const int contextHeight, const uint32_t contextTime,
            const int utxoFromBlockHeight, const uint32_t utxoFromBlockTime) const
    {
        // before stake modifier V2, we require the utxo to be nStakeMinAge old
        if (contextHeight < height_start_StakeModifierV2)
            return (utxoFromBlockTime + nStakeMinAge <= contextTime);
        // with stake modifier V2+, we require the utxo to be nStakeMinDepth deep in the chain
        return (contextHeight - utxoFromBlockHeight >= nStakeMinDepth);
    }


    /*
     * (Legacy) Zerocoin consensus params
     */
    std::string ZC_Modulus;  // parsed in Zerocoin_Params (either as hex or dec string)
    int ZC_MaxPublicSpendsPerTx;
    int ZC_MaxSpendsPerTx;
    int ZC_MinMintConfirmations;
    CAmount ZC_MinMintFee;
    int ZC_MinStakeDepth;
    int ZC_TimeStart;
    CAmount ZC_WrappedSerialsSupply;

    libzerocoin::ZerocoinParams* Zerocoin_Params(bool useModulusV1) const
    {
        static CBigNum bnHexModulus = 0;
        if (!bnHexModulus) bnHexModulus.SetHex(ZC_Modulus);
        static libzerocoin::ZerocoinParams ZCParamsHex = libzerocoin::ZerocoinParams(bnHexModulus);
        static CBigNum bnDecModulus = 0;
        if (!bnDecModulus) bnDecModulus.SetDec(ZC_Modulus);
        static libzerocoin::ZerocoinParams ZCParamsDec = libzerocoin::ZerocoinParams(bnDecModulus);
        return (useModulusV1 ? &ZCParamsHex : &ZCParamsDec);
    }
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
