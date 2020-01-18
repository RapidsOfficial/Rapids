// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpiv/zpos.h"


/*
 * LEGACY: Kept for IBD in order to verify zerocoin stakes occurred when zPoS was active
 * Find the first occurrence of a certain accumulator checksum.
 * Return block index pointer or nullptr if not found
 */


CLegacyZPivStake::CLegacyZPivStake(const libzerocoin::CoinSpend& spend)
{
    this->nChecksum = spend.getAccumulatorChecksum();
    this->denom = spend.getDenomination();
    uint256 nSerial = spend.getCoinSerialNumber().getuint256();
    this->hashSerial = Hash(nSerial.begin(), nSerial.end());
}

CBlockIndex* CLegacyZPivStake::GetIndexFrom()
{
    // First look in the legacy database
    int nHeightChecksum = 0;
    if (zerocoinDB->ReadAccChecksum(nChecksum, denom, nHeightChecksum)) {
        return chainActive[nHeightChecksum];
    }

    // Not found. Scan the chain.
    CBlockIndex* pindex = chainActive[Params().Zerocoin_StartHeight()];
    if (!pindex) return nullptr;
    const int last_block = Params().Zerocoin_Block_Last_Checkpoint();
    while (pindex && pindex->nHeight <= last_block) {
        if (ParseAccChecksum(pindex->nAccumulatorCheckpoint, denom) == nChecksum) {
            // Found. Save to database and return
            zerocoinDB->WriteAccChecksum(nChecksum, denom, pindex->nHeight);
            return pindex;
        }
        //Skip forward in groups of 10 blocks since checkpoints only change every 10 blocks
        if (pindex->nHeight % 10 == 0) {
            pindex = chainActive[pindex->nHeight + 10];
            continue;
        }
        pindex = chainActive.Next(pindex);
    }
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
