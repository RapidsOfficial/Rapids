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

class CStakeKernel {
public:
    /**
     * CStakeKernel Constructor
     *
     * @param[in]   pindexPrev      index of the parent of the kernel block
     * @param[in]   stakeInput      input for the coinstake of the kernel block
     * @param[in]   nBits           target difficulty bits of the kernel block
     * @param[in]   nTimeTx         time of the kernel block
     */
    CStakeKernel(const CBlockIndex* const pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int nTimeTx);

    // Return stake kernel hash
    uint256 GetHash() const;

    // Check that the kernel hash meets the target required
    bool CheckKernelHash(bool fSkipLog = false) const;

private:
    // kernel message hashed
    CDataStream stakeModifier{CDataStream(SER_GETHASH, 0)};
    int nTimeBlockFrom{0};
    CDataStream stakeUniqueness{CDataStream(SER_GETHASH, 0)};
    int nTime{0};
    // hash target
    unsigned int nBits{0};     // difficulty for the target
    CAmount stakeValue{0};     // target multiplier
};

/* PoS Validation */
bool GetHashProofOfStake(const CBlockIndex* pindexPrev, CStakeInput* stake, const unsigned int nTimeTx, const bool fVerify, uint256& hashProofOfStakeRet);
bool CheckStakeKernelHash(const CBlockIndex* pindexPrev, const unsigned int nBits, CStakeInput* stake, const unsigned int nTimeTx, uint256& hashProofOfStake, const bool fVerify = false);
bool CheckProofOfStake(const CBlock& block, uint256& hashProofOfStake, std::unique_ptr<CStakeInput>& stake, int nPreviousBlockHeight);
// Initialize the stake input object
bool initStakeInput(const CBlock& block, std::unique_ptr<CStakeInput>& stake, const CBlockIndex* pindexPrev);
// Stake (find valid kernel)
bool Stake(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int64_t& nTimeTx, uint256& hashProofOfStake);


/* Old Stake Modifier */
bool GetOldStakeModifier(CStakeInput* stake, uint64_t& nStakeModifier);
bool GetOldModifier(const uint256& hashBlockFrom, uint64_t& nStakeModifier);
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);

#endif // PIVX_KERNEL_H
