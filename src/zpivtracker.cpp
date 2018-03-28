#include "zpivtracker.h"
#include "util.h"
#include "sync.h"
#include "main.h"
#include "walletdb.h"

using namespace std;

CzPIVTracker::CzPIVTracker(std::string strWalletFile)
{
    this->strWalletFile = strWalletFile;
    mapSerialHashes.clear();
}

bool CzPIVTracker::Archive(CZerocoinMint& mint, bool fUpdateDB)
{
    uint256 hashSerial = GetSerialHash(mint.GetSerialNumber());
    if (mapSerialHashes.count(hashSerial))
        mapSerialHashes.at(hashSerial).isArchived = true;

    if (!fUpdateDB)
        return true;

    return CWalletDB(strWalletFile).ArchiveMintOrphan(mint);
}

CMintMeta CzPIVTracker::Get(const uint256 &hashSerial)
{
    return mapSerialHashes.at(hashSerial);
}

std::vector<uint256> CzPIVTracker::GetSerialHashes()
{
    vector<uint256> vHashes;
    for (auto it : mapSerialHashes)
        vHashes.emplace_back(it.first);

    return vHashes;
}

CAmount CzPIVTracker::GetBalance(bool fConfirmedOnly, bool fUnconfirmedOnly) const
{
    CAmount nTotal = 0;
    //! zerocoin specific fields
    std::map<libzerocoin::CoinDenomination, unsigned int> myZerocoinSupply;
    for (auto& denom : libzerocoin::zerocoinDenomList) {
        myZerocoinSupply.insert(make_pair(denom, 0));
    }

    {
        //LOCK(cs_pivtracker);
        // Get Unused coins
        for (auto& it : mapSerialHashes) {
            CMintMeta meta = it.second;
            if (meta.isUsed || meta.isArchived)
                continue;
            bool fConfirmed = (meta.nHeight < chainActive.Height() - Params().Zerocoin_MintRequiredConfirmations());
            if (fConfirmedOnly && !fConfirmed)
                continue;
            if (fUnconfirmedOnly && fConfirmed)
                continue;

            nTotal += libzerocoin::ZerocoinDenominationToAmount(meta.denom);
            myZerocoinSupply.at(meta.denom)++;
        }
    }
    for (auto& denom : libzerocoin::zerocoinDenomList) {
        LogPrint("zero","%s My coins for denomination %d pubcoin %s\n", __func__,denom, myZerocoinSupply.at(denom));
    }
    LogPrint("zero","Total value of coins %d\n",nTotal);

    if (nTotal < 0 ) nTotal = 0; // Sanity never hurts

    return nTotal;
}

CAmount CzPIVTracker::GetUnconfirmedBalance() const
{
    return GetBalance(false, true);
}

std::vector<CMintMeta> CzPIVTracker::GetMints(bool fConfirmedOnly) const
{
    vector<CMintMeta> vMints;
    for (auto& it : mapSerialHashes) {
        CMintMeta mint = it.second;
        if (mint.isArchived || mint.isUsed)
            continue;
        bool fConfirmed = (mint.nHeight < chainActive.Height() - Params().Zerocoin_MintRequiredConfirmations());
        if (fConfirmedOnly && !fConfirmed)
            continue;
        vMints.emplace_back(mint);
    }
    return vMints;
}

bool CzPIVTracker::HasPubcoin(const CBigNum &bnValue) const
{
    // Check if this mint's pubcoin value belongs to our mapSerialHashes (which includes hashpubcoin values)
    uint256 hash = GetPubCoinHash(bnValue);
    return HasPubcoinHash(hash);
}

bool CzPIVTracker::HasPubcoinHash(const uint256& hashPubcoin) const
{
    for (auto it : mapSerialHashes) {
        CMintMeta meta = it.second;
        if (meta.hashPubcoin == hashPubcoin)
            return true;
    }
    return false;
}

bool CzPIVTracker::HasSerial(const CBigNum& bnSerial) const
{
    uint256 hash = GetSerialHash(bnSerial);
    return HasSerialHash(hash);
}

bool CzPIVTracker::HasSerialHash(const uint256& hashSerial) const
{
    auto it = mapSerialHashes.find(hashSerial);
    return it != mapSerialHashes.end();
}

bool CzPIVTracker::UpdateMint(CZerocoinMint& mint, bool fUpdateDB)
{
    //Update the mint info in the map
    uint256 nSerial = mint.GetSerialNumber().getuint256();
    uint256 hashSerial = Hash(nSerial.begin(), nSerial.end());
    uint8_t nVersion = (uint8_t)libzerocoin::ExtractVersionFromSerial(mint.GetSerialNumber());
    mint.SetVersion(nVersion);

    CMintMeta meta;
    meta.hashSerial = hashSerial;
    meta.hashPubcoin = GetPubCoinHash(mint.GetValue());
    meta.denom = mint.GetDenomination();
    meta.nVersion = mint.GetVersion();
    meta.nHeight = mint.GetHeight();
    meta.isUsed = mint.IsUsed();
    meta.isArchived = false;

    mapSerialHashes[hashSerial] = meta;
    if (!fUpdateDB)
        return true;

    return CWalletDB(strWalletFile).WriteZerocoinMint(mint);
}

void CzPIVTracker::Clear()
{
    mapSerialHashes.clear();
}