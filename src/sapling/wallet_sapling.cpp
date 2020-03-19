// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"


//! TODO: Should be Sapling address format, SaplingPaymentAddress
// Generate a new Sapling spending key and return its public payment address
libzcash::SaplingPaymentAddress CWallet::GenerateNewSaplingZKey()
{
    AssertLockHeld(cs_wallet); // mapZKeyMetadata
    auto sk = libzcash::SaplingSpendingKey::random(); //todo: connect zip32
    auto fvk = sk.full_viewing_key();
    auto addrOpt = sk.default_address();
    if (addrOpt){
        auto addr = addrOpt.value();
        // Check for collision, even though it is unlikely to ever occur
        if (CCryptoKeyStore::HaveSaplingSpendingKey(fvk))
            throw std::runtime_error("CWallet::GenerateNewSaplingZKey(): Collision detected");

        // Create new metadata
        int64_t nCreationTime = GetTime();
        mapSaplingZKeyMetadata[addr] = CKeyMetadata(nCreationTime);

        if (!AddSaplingZKey(sk)) {
            throw std::runtime_error("CWallet::GenerateNewSaplingZKey(): AddSaplingZKey failed");
        }
        // return default sapling payment address.
        return addr;
    } else {
        throw std::runtime_error("CWallet::GenerateNewSaplingZKey(): default_address() did not return address");
    }
}

// Add spending key to keystore
// TODO: persist to disk
bool CWallet::AddSaplingZKey(const libzcash::SaplingSpendingKey &sk)
{
    AssertLockHeld(cs_wallet); // mapSaplingZKeyMetadata
    auto addr = sk.default_address();

    if (!CCryptoKeyStore::AddSaplingSpendingKey(sk)) {
        return false;
    }

    // // check if we need to remove from viewing keys
    // if (HaveViewingKey(addr)) {
    //     RemoveViewingKey(key.viewing_key());
    // }

    // if (!fFileBacked) {
    //     return true;
    // }

    // if (!IsCrypted()) {
    //     return CWalletDB(strWalletFile).WriteSaplingZKey(addr,
    //                                               sk,
    //                                               mapSaplingZKeyMetadata[addr]);
    // }
    return true;
}


