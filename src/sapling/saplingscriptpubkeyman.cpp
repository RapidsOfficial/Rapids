// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sapling/saplingscriptpubkeyman.h"


//! TODO: Should be Sapling address format, SaplingPaymentAddress
// Generate a new Sapling spending key and return its public payment address
libzcash::SaplingPaymentAddress SaplingScriptPubKeyMan::GenerateNewSaplingZKey()
{
    LOCK(wallet->cs_wallet); // mapSaplingZKeyMetadata

    // Try to get the seed
    CKey seedKey;
    if (!wallet->GetKey(hdChain.GetID(), seedKey))
        throw std::runtime_error(std::string(__func__) + ": HD seed not found");

    HDSeed seed(seedKey.GetPrivKey());
    auto m = libzcash::SaplingExtendedSpendingKey::Master(seed);

    // We use a fixed keypath scheme of m/32'/coin_type'/account'
    // Derive m/32'
    auto m_32h = m.Derive(32 | ZIP32_HARDENED_KEY_LIMIT);
    // Derive m/32'/coin_type'
    auto m_32h_cth = m_32h.Derive(119 | ZIP32_HARDENED_KEY_LIMIT);

    // Derive account key at next index, skip keys already known to the wallet
    libzcash::SaplingExtendedSpendingKey xsk;
    do {
        xsk = m_32h_cth.Derive(hdChain.nExternalChainCounter | ZIP32_HARDENED_KEY_LIMIT);
        hdChain.nExternalChainCounter++; // Increment childkey index
    } while (wallet->HaveSaplingSpendingKey(xsk.expsk.full_viewing_key()));

    // Update the chain model in the database
    if (wallet->fFileBacked && !CWalletDB(wallet->strWalletFile).WriteHDChain(hdChain))
        throw std::runtime_error(std::string(__func__) + ": Writing HD chain model failed");

    // Create new metadata
    int64_t nCreationTime = GetTime();
    auto ivk = xsk.expsk.full_viewing_key().in_viewing_key();
    CKeyMetadata metadata(nCreationTime);
    metadata.key_origin.path.push_back(32 | BIP32_HARDENED_KEY_LIMIT);
    metadata.key_origin.path.push_back(119 | BIP32_HARDENED_KEY_LIMIT);
    metadata.key_origin.path.push_back(hdChain.nExternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
    metadata.hd_seed_id = hdChain.GetID();
    mapSaplingZKeyMetadata[ivk] = metadata;

    auto addr = xsk.DefaultAddress();
    if (!AddSaplingZKey(xsk, addr)) {
        throw std::runtime_error(std::string(__func__) + ": AddSaplingZKey failed");
    }
    // return default sapling payment address.
    return addr;
}

// Add spending key to keystore
bool SaplingScriptPubKeyMan::AddSaplingZKey(
        const libzcash::SaplingExtendedSpendingKey &sk,
        const libzcash::SaplingPaymentAddress &defaultAddr)
{
    AssertLockHeld(wallet->cs_wallet); // mapSaplingZKeyMetadata

    if (!AddSaplingSpendingKey(sk, defaultAddr)) {
        return false;
    }

    if (!wallet->fFileBacked) {
        return true;
    }

    if (!wallet->IsCrypted()) {
        auto ivk = sk.expsk.full_viewing_key().in_viewing_key();
        return CWalletDB(wallet->strWalletFile).WriteSaplingZKey(ivk, sk, mapSaplingZKeyMetadata[ivk]);
    }

    return true;
}

bool SaplingScriptPubKeyMan::AddSaplingSpendingKey(
        const libzcash::SaplingExtendedSpendingKey &sk,
        const libzcash::SaplingPaymentAddress &defaultAddr)
{
    {
        LOCK(wallet->cs_KeyStore);
        if (!wallet->IsCrypted()) {
            return wallet->AddSaplingSpendingKey(sk, defaultAddr); // keystore
        }

        if (wallet->IsLocked()) {
            return false;
        }

        std::vector<unsigned char> vchCryptedSecret;
        CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << sk;
        CKeyingMaterial vchSecret(ss.begin(), ss.end());
        auto address = sk.DefaultAddress();
        auto extfvk = sk.ToXFVK();
        if (!EncryptSecret(wallet->GetEncryptionKey(), vchSecret, extfvk.fvk.GetFingerprint(), vchCryptedSecret)) {
            return false;
        }

        if (!AddCryptedSaplingSpendingKeyDB(extfvk, vchCryptedSecret, defaultAddr)) {
            return false;
        }
    }
    return true;
}

// Add payment address -> incoming viewing key map entry
bool SaplingScriptPubKeyMan::AddSaplingIncomingViewingKey(
        const libzcash::SaplingIncomingViewingKey &ivk,
        const libzcash::SaplingPaymentAddress &addr)
{
    AssertLockHeld(wallet->cs_wallet); // mapSaplingZKeyMetadata

    if (!wallet->AddSaplingIncomingViewingKey(ivk, addr)) {
        return false;
    }

    if (!wallet->fFileBacked) {
        return true;
    }

    if (!wallet->IsCrypted()) {
        return CWalletDB(wallet->strWalletFile).WriteSaplingPaymentAddress(addr, ivk);
    }

    return true;
}

bool SaplingScriptPubKeyMan::EncryptSaplingKeys(CKeyingMaterial& vMasterKeyIn)
{
    AssertLockHeld(wallet->cs_wallet); // mapSaplingSpendingKeys

    for (SaplingSpendingKeyMap::value_type& mSaplingSpendingKey : wallet->mapSaplingSpendingKeys) {
        const libzcash::SaplingExtendedSpendingKey &sk = mSaplingSpendingKey.second;
        CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << sk;
        CKeyingMaterial vchSecret(ss.begin(), ss.end());
        auto extfvk = sk.ToXFVK();
        std::vector<unsigned char> vchCryptedSecret;
        if (!EncryptSecret(vMasterKeyIn, vchSecret, extfvk.fvk.GetFingerprint(), vchCryptedSecret)) {
            return false;
        }
        if (!AddCryptedSaplingSpendingKeyDB(extfvk, vchCryptedSecret, sk.DefaultAddress())) {
            return false;
        }
    }
    wallet->mapSaplingSpendingKeys.clear();
    return true;
}

bool SaplingScriptPubKeyMan::AddCryptedSaplingSpendingKeyDB(const libzcash::SaplingExtendedFullViewingKey &extfvk,
                                           const std::vector<unsigned char> &vchCryptedSecret,
                                           const libzcash::SaplingPaymentAddress &defaultAddr)
{
    if (!wallet->AddCryptedSaplingSpendingKey(extfvk, vchCryptedSecret, defaultAddr))
        return false;
    if (!wallet->fFileBacked)
        return true;
    {
        LOCK(wallet->cs_wallet);
        if (wallet->pwalletdbEncryption) {
            return wallet->pwalletdbEncryption->WriteCryptedSaplingZKey(extfvk,
                                                                vchCryptedSecret,
                                                                mapSaplingZKeyMetadata[extfvk.fvk.in_viewing_key()]);
        } else {
            return CWalletDB(wallet->strWalletFile).WriteCryptedSaplingZKey(extfvk,
                                                                    vchCryptedSecret,
                                                                    mapSaplingZKeyMetadata[extfvk.fvk.in_viewing_key()]);
        }
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

///////////////////// Load ////////////////////////////////////////

bool SaplingScriptPubKeyMan::LoadCryptedSaplingZKey(
        const libzcash::SaplingExtendedFullViewingKey &extfvk,
        const std::vector<unsigned char> &vchCryptedSecret)
{
    return wallet->AddCryptedSaplingSpendingKey(extfvk, vchCryptedSecret, extfvk.DefaultAddress());
}

bool SaplingScriptPubKeyMan::LoadSaplingZKeyMetadata(const libzcash::SaplingIncomingViewingKey &ivk, const CKeyMetadata &meta)
{
    AssertLockHeld(wallet->cs_wallet); // mapSaplingZKeyMetadata
    mapSaplingZKeyMetadata[ivk] = meta;
    return true;
}

bool SaplingScriptPubKeyMan::LoadSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key)
{
    return wallet->AddSaplingSpendingKey(key, key.DefaultAddress());
}

bool SaplingScriptPubKeyMan::LoadSaplingPaymentAddress(
        const libzcash::SaplingPaymentAddress &addr,
        const libzcash::SaplingIncomingViewingKey &ivk)
{
    return wallet->AddSaplingIncomingViewingKey(ivk, addr);
}

///////////////////// Setup ///////////////////////////////////////

bool SaplingScriptPubKeyMan::SetupGeneration(const CKeyID& keyID, bool force)
{
    SetHDSeed(keyID, force);
    return true;
}

void SaplingScriptPubKeyMan::SetHDSeed(const CPubKey& seed, bool force, bool memonly)
{
    SetHDSeed(seed.GetID(), force, memonly);
}

void SaplingScriptPubKeyMan::SetHDSeed(const CKeyID& keyID, bool force, bool memonly)
{
    if (!hdChain.IsNull() && !force)
        throw std::runtime_error(std::string(__func__) + ": sapling trying to set a hd seed on an already created chain");

    LOCK(wallet->cs_wallet);
    // store the keyid (hash160) together with
    // the child index counter in the database
    // as a hdChain object
    CHDChain newHdChain(HDChain::ChainCounterType::Sapling);
    if (!newHdChain.SetSeed(keyID) ) {
        throw std::runtime_error(std::string(__func__) + ": set sapling hd seed failed");
    }

    SetHDChain(newHdChain, memonly);
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
