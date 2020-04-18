// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpiv/zpos.h"
#include "zpivchain.h"


/*
 * LEGACY: Kept for IBD in order to verify zerocoin stakes occurred when zPoS was active
 * Find the first occurrence of a certain accumulator checksum.
 * Return block index pointer or nullptr if not found
 */

uint32_t ParseAccChecksum(uint256 nCheckpoint, const libzerocoin::CoinDenomination denom)
{
    int pos = std::distance(libzerocoin::zerocoinDenomList.begin(),
            find(libzerocoin::zerocoinDenomList.begin(), libzerocoin::zerocoinDenomList.end(), denom));
    nCheckpoint = nCheckpoint >> (32*((libzerocoin::zerocoinDenomList.size() - 1) - pos));
    return nCheckpoint.Get32();
}

bool CLegacyZPivStake::InitFromTxIn(const CTxIn& txin)
{
    // Construct the stakeinput object
    if (!txin.IsZerocoinSpend())
        return error("%s: unable to initialize CLegacyZPivStake from non zc-spend");

    // Check spend type
    libzerocoin::CoinSpend spend = TxInToZerocoinSpend(txin);
    if (spend.getSpendType() != libzerocoin::SpendType::STAKE)
        return error("%s : spend is using the wrong SpendType (%d)", __func__, (int)spend.getSpendType());

    *this = CLegacyZPivStake(spend);

    // Find the pindex with the accumulator checksum
    if (!GetIndexFrom())
        return error("%s : Failed to find the block index for zpiv stake origin", __func__);

    // All good
    return true;
}

CLegacyZPivStake::CLegacyZPivStake(const libzerocoin::CoinSpend& spend)
{
    this->nChecksum = spend.getAccumulatorChecksum();
    this->denom = spend.getDenomination();
    uint256 nSerial = spend.getCoinSerialNumber().getuint256();
    this->hashSerial = Hash(nSerial.begin(), nSerial.end());
}

CBlockIndex* CLegacyZPivStake::GetIndexFrom()
{
    return nullptr;
}

CAmount CLegacyZPivStake::GetValue() const
{
    return denom * COIN;
}

CDataStream CLegacyZPivStake::GetUniqueness() const
{
    CDataStream ss(SER_GETHASH, 0);
    ss << hashSerial;
    return ss;
}

// Verify stake contextual checks
bool CLegacyZPivStake::ContextCheck(int nHeight, uint32_t nTime)
{
    return false;
}
