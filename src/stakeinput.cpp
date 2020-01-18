// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stakeinput.h"

#include "chain.h"
#include "main.h"
#include "txdb.h"
#include "zpiv/deterministicmint.h"
#include "wallet/wallet.h"

/*
 * LEGACY: Kept for IBD in order to verify zerocoin stakes occurred when zPoS was active
 * Find the first occurrence of a certain accumulator checksum.
 * Return block index pointer or nullptr if not found
 */

CZPivStake::CZPivStake(const libzerocoin::CoinSpend& spend)
{
    this->nChecksum = spend.getAccumulatorChecksum();
    this->denom = spend.getDenomination();
    uint256 nSerial = spend.getCoinSerialNumber().getuint256();
    this->hashSerial = Hash(nSerial.begin(), nSerial.end());
    fMint = false;
}

uint32_t CZPivStake::GetChecksum()
{
    return nChecksum;
}

CBlockIndex* CZPivStake::GetIndexFrom()
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

CAmount CZPivStake::GetValue()
{
    return denom * COIN;
}

// Use the first accumulator checkpoint that occurs 60 minutes after the block being staked from
bool CZPivStake::GetModifier(uint64_t& nStakeModifier)
{
    CBlockIndex* pindex = GetIndexFrom();
    if (!pindex)
        return error("%s: failed to get index from", __func__);

    if(Params().IsRegTestNet()) {
        nStakeModifier = 0;
        return true;
    }

    int64_t nTimeBlockFrom = pindex->GetBlockTime();
    const int nHeightStop = std::min(chainActive.Height(), Params().Zerocoin_Block_Last_Checkpoint()-1);
    while (pindex && pindex->nHeight + 1 <= nHeightStop) {
        if (pindex->GetBlockTime() - nTimeBlockFrom > 60 * 60) {
            nStakeModifier = pindex->nAccumulatorCheckpoint.Get64();
            return true;
        }
        pindex = chainActive.Next(pindex);
    }

    return false;
}

CDataStream CZPivStake::GetUniqueness()
{
    //The unique identifier for a zPIV is a hash of the serial
    CDataStream ss(SER_GETHASH, 0);
    ss << hashSerial;
    return ss;
}

bool CZPivStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
    return error("%s: zPOS Disabled", __func__);
}

bool CZPivStake::CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal)
{
    return error("%s: zPOS Disabled", __func__);
}

bool CZPivStake::GetTxFrom(CTransaction& tx)
{
    return false;
}

bool CZPivStake::MarkSpent(CWallet *pwallet, const uint256& txid)
{
    CzPIVTracker* zpivTracker = pwallet->zpivTracker.get();
    CMintMeta meta;
    if (!zpivTracker->GetMetaFromStakeHash(hashSerial, meta))
        return error("%s: tracker does not have serialhash", __func__);

    zpivTracker->SetPubcoinUsed(meta.hashPubcoin, txid);
    return true;
}

//!PIV Stake
bool CPivStake::SetInput(CTransaction txPrev, unsigned int n)
{
    this->txFrom = txPrev;
    this->nPosition = n;
    return true;
}

bool CPivStake::GetTxFrom(CTransaction& tx)
{
    tx = txFrom;
    return true;
}

bool CPivStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
    txIn = CTxIn(txFrom.GetHash(), nPosition);
    return true;
}

CAmount CPivStake::GetValue()
{
    return txFrom.vout[nPosition].nValue;
}

bool CPivStake::CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    CScript scriptPubKeyKernel = txFrom.vout[nPosition].scriptPubKey;
    if (!Solver(scriptPubKeyKernel, whichType, vSolutions))
        return error("%s: failed to parse kernel", __func__);

    if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH && whichType != TX_COLDSTAKE)
        return error("%s: type=%d (%s) not supported for scriptPubKeyKernel", __func__, whichType, GetTxnOutputType(whichType));

    CScript scriptPubKey;
    CKey key;
    if (whichType == TX_PUBKEYHASH) {
        // if P2PKH check that we have the input private key
        if (!pwallet->GetKey(CKeyID(uint160(vSolutions[0])), key))
            return error("%s: Unable to get staking private key", __func__);

        // convert to P2PK inputs
        scriptPubKey << key.GetPubKey() << OP_CHECKSIG;

    } else {
        // if P2CS, check that we have the coldstaking private key
        if ( whichType == TX_COLDSTAKE && !pwallet->GetKey(CKeyID(uint160(vSolutions[0])), key) )
            return error("%s: Unable to get cold staking private key", __func__);

        // keep the same script
        scriptPubKey = scriptPubKeyKernel;
    }

    vout.emplace_back(CTxOut(0, scriptPubKey));

    // Calculate if we need to split the output
    int nSplit = nTotal / (static_cast<CAmount>(pwallet->nStakeSplitThreshold * COIN));
    if (nSplit > 1) {
        // if nTotal is twice or more of the threshold; create more outputs
        int txSizeMax = MAX_STANDARD_TX_SIZE >> 11; // limit splits to <10% of the max TX size (/2048)
        if (nSplit > txSizeMax)
            nSplit = txSizeMax;
        for (int i = nSplit; i > 1; i--) {
            LogPrintf("%s: StakeSplit: nTotal = %d; adding output %d of %d\n", __func__, nTotal, (nSplit-i)+2, nSplit);
            vout.emplace_back(CTxOut(0, scriptPubKey));
        }
    }

    return true;
}

bool CPivStake::GetModifier(uint64_t& nStakeModifier)
{
    if (this->nStakeModifier == 0) {
        // look for the modifier
        GetIndexFrom();
        if (!pindexFrom)
            return error("%s: failed to get index from", __func__);
        // TODO: This method must be removed from here in the short terms.. it's a call to an static method in kernel.cpp when this class method is only called from kernel.cpp, no comments..
        if (!GetKernelStakeModifier(pindexFrom->GetBlockHash(), this->nStakeModifier, this->nStakeModifierHeight, this->nStakeModifierTime, false))
            return error("CheckStakeKernelHash(): failed to get kernel stake modifier");
    }
    nStakeModifier = this->nStakeModifier;
    return true;
}

CDataStream CPivStake::GetUniqueness()
{
    //The unique identifier for a PIV stake is the outpoint
    CDataStream ss(SER_NETWORK, 0);
    ss << nPosition << txFrom.GetHash();
    return ss;
}

//The block that the UTXO was added to the chain
CBlockIndex* CPivStake::GetIndexFrom()
{
    if (pindexFrom)
        return pindexFrom;
    uint256 hashBlock = 0;
    CTransaction tx;
    if (GetTransaction(txFrom.GetHash(), tx, hashBlock, true)) {
        // If the index is in the chain, then set it as the "index from"
        if (mapBlockIndex.count(hashBlock)) {
            CBlockIndex* pindex = mapBlockIndex.at(hashBlock);
            if (chainActive.Contains(pindex))
                pindexFrom = pindex;
        }
    } else {
        LogPrintf("%s : failed to find tx %s\n", __func__, txFrom.GetHash().GetHex());
    }

    return pindexFrom;
}
