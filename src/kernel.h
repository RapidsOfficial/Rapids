// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2013-2014 The NovaCoin Developers
// Copyright (c) 2014-2018 The BlackCoin Developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_KERNEL_H
#define PIVX_KERNEL_H

#include "main.h"
#include "stakeinput.h"

/* PoS Validation */
bool GetHashProofOfStake(const CBlockIndex* pindexPrev, CStakeInput* stake, const unsigned int nTimeTx, const bool fVerify, uint256& hashProofOfStakeRet);
bool CheckStakeKernelHash(const CBlockIndex* pindexPrev, const unsigned int nBits, CStakeInput* stake, const unsigned int nTimeTx, uint256& hashProofOfStake, const bool fVerify = false);
bool CheckProofOfStake(const CBlock& block, uint256& hashProofOfStake, std::unique_ptr<CStakeInput>& stake, int nPreviousBlockHeight);
// Initialize the stake input object
bool initStakeInput(const CBlock& block, std::unique_ptr<CStakeInput>& stake, int nPreviousBlockHeight);
// (New) Stake Modifier
uint256 ComputeStakeModifier(const CBlockIndex* pindexPrev, const uint256& kernel);
// Stake (find valid kernel)
bool Stake(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int64_t& nTimeTx, uint256& hashProofOfStake);

/* Utils */
int64_t GetTimeSlot(const int64_t nTime);
int64_t GetCurrentTimeSlot();
uint32_t ParseAccChecksum(uint256 nCheckpoint, const libzerocoin::CoinDenomination denom);


/* Old Stake Modifier */
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex);
bool GetOldStakeModifier(CStakeInput* stake, uint64_t& nStakeModifier);
bool GetOldModifier(const uint256& hashBlockFrom, uint64_t& nStakeModifier);
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);

#endif // PIVX_KERNEL_H
