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

    //! Generates new Sapling key
    libzcash::SaplingPaymentAddress GenerateNewSaplingZKey();
    //! Adds Sapling spending key to the store, and saves it to disk
    bool AddSaplingZKey(const libzcash::SaplingSpendingKey &key,
                        const boost::optional<libzcash::SaplingPaymentAddress> &defaultAddr = boost::none);
    bool AddCryptedSaplingSpendingKey(
            const libzcash::SaplingFullViewingKey &fvk,
            const std::vector<unsigned char> &vchCryptedSecret,
            const boost::optional<libzcash::SaplingPaymentAddress> &defaultAddr = boost::none);
    //! Returns true if the wallet contains the spending key
    bool HaveSpendingKeyForPaymentAddress(const libzcash::SaplingPaymentAddress &zaddr) const;


    // Sapling metadata
    std::map<libzcash::SaplingPaymentAddress, CKeyMetadata> mapSaplingZKeyMetadata;

private:
    /* Parent wallet */
    CWallet* wallet{nullptr};
};

#endif //PIVX_SAPLINGSCRIPTPUBKEYMAN_H
