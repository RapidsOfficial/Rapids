// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include "dbwrapper.h"
#include "main.h"
#include "zpiv/zerocoin.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

class CCoins;
class uint256;

//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 100;
//! max. -dbcache in (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 16384 : 1024;
//! min. -dbcache in (MiB)
static const int64_t nMinDbCache = 4;

struct CDiskTxPos : public CDiskBlockPos {
    unsigned int nTxOffset; // after header

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(*(CDiskBlockPos*)this);
        READWRITE(VARINT(nTxOffset));
    }

    CDiskTxPos(const CDiskBlockPos& blockIn, unsigned int nTxOffsetIn) : CDiskBlockPos(blockIn.nFile, blockIn.nPos), nTxOffset(nTxOffsetIn)
    {
    }

    CDiskTxPos()
    {
        SetNull();
    }

    void SetNull()
    {
        CDiskBlockPos::SetNull();
        nTxOffset = 0;
    }
};

/** CCoinsView backed by the LevelDB coin database (chainstate/) */
class CCoinsViewDB : public CCoinsView
{
protected:
    CDBWrapper db;

public:
    CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoins(const uint256& txid, CCoins& coins) const;
    bool HaveCoins(const uint256& txid) const;
    uint256 GetBestBlock() const;
    bool BatchWrite(CCoinsMap& mapCoins, const uint256& hashBlock);
    bool GetStats(CCoinsStats& stats) const;
};

/** Access to the block database (blocks/index/) */
class CBlockTreeDB : public CDBWrapper
{
public:
    CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    CBlockTreeDB(const CBlockTreeDB&);
    void operator=(const CBlockTreeDB&);

public:
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo& fileinfo);
    bool ReadLastBlockFile(int& nFile);
    bool WriteReindexing(bool fReindex);
    bool ReadReindexing(bool& fReindex);
    bool ReadTxIndex(const uint256& txid, CDiskTxPos& pos);
    bool WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >& list);
    bool WriteFlag(const std::string& name, bool fValue);
    bool ReadFlag(const std::string& name, bool& fValue);
    bool WriteInt(const std::string& name, int nValue);
    bool ReadInt(const std::string& name, int& nValue);
    bool LoadBlockIndexGuts();
    bool ReadLegacyBlockIndex(const uint256& blockHash, CLegacyBlockIndex& biRet);
    bool WriteMoneySupply(const int64_t& nSupply);
    bool ReadMoneySupply(int64_t& nSupply) const;
};

/** Zerocoin database (zerocoin/) */
class CZerocoinDB : public CDBWrapper
{
public:
    CZerocoinDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    CZerocoinDB(const CZerocoinDB&);
    void operator=(const CZerocoinDB&);

public:
    /** Write zPIV mints to the zerocoinDB in a batch */
    bool WriteCoinMintBatch(const std::vector<std::pair<libzerocoin::PublicCoin, uint256> >& mintInfo);
    bool ReadCoinMint(const CBigNum& bnPubcoin, uint256& txHash);
    bool ReadCoinMint(const uint256& hashPubcoin, uint256& hashTx);
    /** Write zPIV spends to the zerocoinDB in a batch */
    bool WriteCoinSpendBatch(const std::vector<std::pair<libzerocoin::CoinSpend, uint256> >& spendInfo);
    bool ReadCoinSpend(const CBigNum& bnSerial, uint256& txHash);
    bool ReadCoinSpend(const uint256& hashSerial, uint256 &txHash);
    bool EraseCoinMint(const CBigNum& bnPubcoin);
    bool EraseCoinSpend(const CBigNum& bnSerial);
    bool WipeCoins(std::string strType);

    /** Map supply [denom] --> supply     */
    bool WriteZCSupply(const std::map<libzerocoin::CoinDenomination, int64_t>& mapZCS);
    bool ReadZCSupply(std::map<libzerocoin::CoinDenomination, int64_t>& mapZCS) const;
    /** Accumulators (only for zPoS IBD): [checksum, denom] --> block height **/
    bool WriteAccChecksum(const uint32_t& nChecksum, const libzerocoin::CoinDenomination denom, const int nHeight);
    bool ReadAccChecksum(const uint32_t& nChecksum, const libzerocoin::CoinDenomination denom, int& nHeightRet);
    bool EraseAccChecksum(const uint32_t& nChecksum, const libzerocoin::CoinDenomination denom);
    bool WipeAccChecksums();
};

#endif // BITCOIN_TXDB_H
