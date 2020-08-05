// Copyright (c) 2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_SAPLINGSCRIPTPUBKEYMAN_H
#define PIVX_SAPLINGSCRIPTPUBKEYMAN_H

#include "wallet/hdchain.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"

/*
 * Sapling keys manager
 * todo: add real description..
 */
class SaplingScriptPubKeyMan {

public:
    SaplingScriptPubKeyMan(CWallet *parent) : wallet(parent) {}

    ~SaplingScriptPubKeyMan() {};

    bool SetupGeneration(const CKeyID& keyID, bool force = false);
    bool IsEnabled() const { return !hdChain.IsNull(); };

    /* Set the current HD seed (will reset the chain child index counters)
      Sets the seed's version based on the current wallet version (so the
      caller must ensure the current wallet version is correct before calling
      this function). */
    void SetHDSeed(const CPubKey& key, bool force = false, bool memonly = false);
    void SetHDSeed(const CKeyID& keyID, bool force = false, bool memonly = false);

    /* Set the HD chain model (chain child index counters) */
    void SetHDChain(CHDChain& chain, bool memonly);
    const CHDChain& GetHDChain() const { return hdChain; }

    /* Encrypt Sapling keys */
    bool EncryptSaplingKeys(CKeyingMaterial& vMasterKeyIn);

    //! Generates new Sapling key
    libzcash::SaplingPaymentAddress GenerateNewSaplingZKey();
    //! Adds Sapling spending key to the store, and saves it to disk
    bool AddSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key,
                        const libzcash::SaplingPaymentAddress &defaultAddr);
    bool AddSaplingIncomingViewingKey(
            const libzcash::SaplingIncomingViewingKey &ivk,
            const libzcash::SaplingPaymentAddress &addr);
    bool AddCryptedSaplingSpendingKeyDB(
            const libzcash::SaplingExtendedFullViewingKey &extfvk,
            const std::vector<unsigned char> &vchCryptedSecret,
            const libzcash::SaplingPaymentAddress &defaultAddr);
    //! Returns true if the wallet contains the spending key
    bool HaveSpendingKeyForPaymentAddress(const libzcash::SaplingPaymentAddress &zaddr) const;

    //! Adds spending key to the store, without saving it to disk (used by LoadWallet)
    bool LoadSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key);
    //! Load spending key metadata (used by LoadWallet)
    bool LoadSaplingZKeyMetadata(const libzcash::SaplingIncomingViewingKey &ivk, const CKeyMetadata &meta);
    //! Adds an encrypted spending key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedSaplingZKey(const libzcash::SaplingExtendedFullViewingKey &extfvk,
                                const std::vector<unsigned char> &vchCryptedSecret);
    //! Adds a Sapling payment address -> incoming viewing key map entry,
    //! without saving it to disk (used by LoadWallet)
    bool LoadSaplingPaymentAddress(
            const libzcash::SaplingPaymentAddress &addr,
            const libzcash::SaplingIncomingViewingKey &ivk);
    bool AddSaplingSpendingKey(
            const libzcash::SaplingExtendedSpendingKey &sk,
            const libzcash::SaplingPaymentAddress &defaultAddr);

    // Sapling metadata
    std::map<libzcash::SaplingIncomingViewingKey, CKeyMetadata> mapSaplingZKeyMetadata;

private:
    /* Parent wallet */
    CWallet* wallet{nullptr};
    /* the HD chain data model (external/internal chain counters) */
    CHDChain hdChain;
};

#endif //PIVX_SAPLINGSCRIPTPUBKEYMAN_H
