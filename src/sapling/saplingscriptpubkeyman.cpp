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
    auto ivk = fvk.in_viewing_key();
    auto addr = sk.default_address();

    // Check for collision, even though it is unlikely to ever occur
    if (wallet->HaveSaplingSpendingKey(fvk))
        throw std::runtime_error("SaplingScriptPubKeyMan::GenerateNewSaplingZKey(): Collision detected");

    // Create new metadata
    int64_t nCreationTime = GetTime();
    mapSaplingZKeyMetadata[ivk] = CKeyMetadata(nCreationTime);

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

void SaplingScriptPubKeyMan::SetHDSeed(const CPubKey& seed, bool force)
{
    if (!hdChain.IsNull() && !force)
        throw std::runtime_error(std::string(__func__) + ": sapling trying to set a hd seed on an already created chain");

    LOCK(wallet->cs_wallet);
    // store the keyid (hash160) together with
    // the child index counter in the database
    // as a hdChain object
    CHDChain newHdChain(HDChain::ChainCounterType::Sapling);
    if (!newHdChain.SetSeed(seed.GetID()) ) {
        throw std::runtime_error(std::string(__func__) + ": set sapling hd seed failed");
    }

    SetHDChain(newHdChain, false);
}

void SaplingScriptPubKeyMan::SetHDChain(CHDChain& chain, bool memonly)
{
    LOCK(wallet->cs_wallet);
    if (chain.chainType != HDChain::ChainCounterType::Sapling)
        throw std::runtime_error(std::string(__func__) + ": trying to store an invalid chain type");

    if (!memonly && !CWalletDB(wallet->strWalletFile).WriteHDChain(chain))
        throw std::runtime_error(std::string(__func__) + ": writing sapling chain failed");

    hdChain = chain;

    // Sanity check
    if (!wallet->HaveKey(hdChain.GetID()))
        throw std::runtime_error(std::string(__func__) + ": Not found sapling seed in wallet");
}