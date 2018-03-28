#ifndef PIVX_ZPIVTRACKER_H
#define PIVX_ZPIVTRACKER_H

#include "primitives/zerocoin.h"

class CzPIVTracker
{
private:
    std::string strWalletFile;
    std::map<uint256, CMintMeta> mapSerialHashes;
public:
    CzPIVTracker(std::string strWalletFile);
    bool Archive(CZerocoinMint& mint, bool fUpdateDB = true);
    bool HasPubcoin(const CBigNum& bnValue) const;
    bool HasPubcoinHash(const uint256& hashPubcoin) const;
    bool HasSerial(const CBigNum& bnSerial) const;
    bool HasSerialHash(const uint256& hashSerial) const;
    bool IsEmpty() const { return mapSerialHashes.empty(); }
    CMintMeta Get(const uint256& hashSerial);
    CAmount GetBalance(bool fConfirmedOnly, bool fUnconfirmedOnly) const;
    std::vector<uint256> GetSerialHashes();
    std::vector<CMintMeta> GetMints(bool fConfirmedOnly) const;
    CAmount GetUnconfirmedBalance() const;
    bool UpdateMint(CZerocoinMint& mint, bool fUpdateDB = true);
    void Clear();
};

#endif //PIVX_ZPIVTRACKER_H
