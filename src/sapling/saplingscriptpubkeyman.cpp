// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sapling/saplingscriptpubkeyman.h"


//! TODO: Should be Sapling address format, SaplingPaymentAddress
// Generate a new Sapling spending key and return its public payment address
libzcash::SaplingPaymentAddress SaplingScriptPubKeyMan::GenerateNewSaplingZKey()
{
    AssertLockHeld(wallet->cs_wallet); // mapZKeyMetadata

    auto sk = libzcash::SaplingSpendingKey::random();
    auto fvk = sk.full_viewing_key();
    auto addr = sk.default_address();

    // Check for collision, even though it is unlikely to ever occur
    if (wallet->HaveSaplingSpendingKey(fvk))
        throw std::runtime_error("SaplingScriptPubKeyMan::GenerateNewSaplingZKey(): Collision detected");

    // Create new metadata
    int64_t nCreationTime = GetTime();
    mapSaplingZKeyMetadata[addr] = CKeyMetadata(nCreationTime);

    if (!AddSaplingZKey(sk, addr)) {
        throw std::runtime_error("SaplingScriptPubKeyMan::GenerateNewSaplingZKey(): AddSaplingZKey failed");
    }
    // return default sapling payment address.
    return addr;
}

// Add spending key to keystore
// TODO: persist to disk
bool SaplingScriptPubKeyMan::AddSaplingZKey(
        const libzcash::SaplingSpendingKey &sk,
        const boost::optional<libzcash::SaplingPaymentAddress> &defaultAddr)
{
    AssertLockHeld(wallet->cs_wallet); // mapSaplingZKeyMetadata

    if (!wallet->AddSaplingSpendingKey(sk, defaultAddr)) {
        return false;
    }

    if (!wallet->fFileBacked) {
        return true;
    }

    // TODO: Persist to disk
    // if (!IsCrypted()) {
    //     return CWalletDB(strWalletFile).WriteSaplingZKey(addr,
    //                                               sk,
    //                                               mapSaplingZKeyMetadata[addr]);
    // }
    return true;
}

bool SaplingScriptPubKeyMan::AddCryptedSaplingSpendingKey(const libzcash::SaplingFullViewingKey &fvk,
                                           const std::vector<unsigned char> &vchCryptedSecret,
                                           const boost::optional<libzcash::SaplingPaymentAddress> &defaultAddr)
{
    if (!wallet->AddCryptedSaplingSpendingKey(fvk, vchCryptedSecret, defaultAddr))
        return false;
    if (!wallet->fFileBacked)
        return true;
    {
        // TODO: Sapling - Write to disk
    }
    return false;
}

bool SaplingScriptPubKeyMan::HaveSpendingKeyForPaymentAddress(const libzcash::SaplingPaymentAddress &zaddr) const
{
    libzcash::SaplingIncomingViewingKey ivk;
    libzcash::SaplingFullViewingKey fvk;

    return wallet->GetSaplingIncomingViewingKey(zaddr, ivk) &&
           wallet->GetSaplingFullViewingKey(ivk, fvk) &&
           wallet->HaveSaplingSpendingKey(fvk);
}