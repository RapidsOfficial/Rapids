// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypter.h"

#include "script/script.h"
#include "script/standard.h"
#include "util.h"
#include "uint256.h"

#include <openssl/aes.h>
#include <openssl/evp.h>
#include "wallet/wallet.h"

bool CCryptoKeyStore::AddSaplingSpendingKey(
        const libzcash::SaplingExtendedSpendingKey &sk,
        const boost::optional<libzcash::SaplingPaymentAddress> &defaultAddr)
{
    {
        LOCK(cs_SpendingKeyStore);
        if (!IsCrypted()) {
            return CBasicKeyStore::AddSaplingSpendingKey(sk, defaultAddr);
        }

        if (IsLocked()) {
            return false;
        }

        std::vector<unsigned char> vchCryptedSecret;
        CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << sk;
        CKeyingMaterial vchSecret(ss.begin(), ss.end());
        auto address = sk.DefaultAddress();
        auto fvk = sk.expsk.full_viewing_key();
        if (!EncryptSecret(vMasterKey, vchSecret, fvk.GetFingerprint(), vchCryptedSecret)) {
            return false;
        }

        if (!AddCryptedSaplingSpendingKey(fvk, vchCryptedSecret, defaultAddr)) {
            return false;
        }
    }
    return true;
}

bool CCryptoKeyStore::AddCryptedSaplingSpendingKey(
        const libzcash::SaplingFullViewingKey &fvk,
        const std::vector<unsigned char> &vchCryptedSecret,
        const boost::optional<libzcash::SaplingPaymentAddress> &defaultAddr)
{
    LOCK(cs_SpendingKeyStore);
    if (!SetCrypted()) {
        return false;
    }

    // if SaplingFullViewingKey is not in SaplingFullViewingKeyMap, add it
    if (!AddSaplingFullViewingKey(fvk, defaultAddr)){
        return false;
    }

    mapCryptedSaplingSpendingKeys[fvk] = vchCryptedSecret;
    return true;
}

static bool DecryptSaplingSpendingKey(const CKeyingMaterial& vMasterKey,
                                      const std::vector<unsigned char>& vchCryptedSecret,
                                      const libzcash::SaplingFullViewingKey& fvk,
                                      libzcash::SaplingExtendedSpendingKey& sk)
{
    CKeyingMaterial vchSecret;
    if(!DecryptSecret(vMasterKey, vchCryptedSecret, fvk.GetFingerprint(), vchSecret))
        return false;

    if (vchSecret.size() != ZIP32_XSK_SIZE)
        return false;

    CSecureDataStream ss(vchSecret, SER_NETWORK, PROTOCOL_VERSION);
    ss >> sk;
    return sk.expsk.full_viewing_key() == fvk;
}

bool CCryptoKeyStore::GetSaplingSpendingKey(const libzcash::SaplingFullViewingKey &fvk, libzcash::SaplingExtendedSpendingKey &skOut) const
{
    {
        LOCK(cs_SpendingKeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::GetSaplingSpendingKey(fvk, skOut);

        CryptedSaplingSpendingKeyMap::const_iterator mi = mapCryptedSaplingSpendingKeys.find(fvk);
        if (mi != mapCryptedSaplingSpendingKeys.end())
        {
            const std::vector<unsigned char> &vchCryptedSecret = (*mi).second;
            return DecryptSaplingSpendingKey(vMasterKey, vchCryptedSecret, fvk, skOut);
        }
    }
    return false;
}

bool CCryptoKeyStore::HaveSaplingSpendingKey(const libzcash::SaplingFullViewingKey &fvk) const
{
    {
        LOCK(cs_SpendingKeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::HaveSaplingSpendingKey(fvk);
        return mapCryptedSaplingSpendingKeys.count(fvk) > 0;
    }
    return false;
}

bool CCryptoKeyStore::EncryptSaplingKeys(CKeyingMaterial& vMasterKeyIn)
{
    for (SaplingSpendingKeyMap::value_type& mSaplingSpendingKey : mapSaplingSpendingKeys) {
        const libzcash::SaplingExtendedSpendingKey &sk = mSaplingSpendingKey.second;
        CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << sk;
        CKeyingMaterial vchSecret(ss.begin(), ss.end());
        libzcash::SaplingFullViewingKey fvk = sk.expsk.full_viewing_key();
        std::vector<unsigned char> vchCryptedSecret;
        if (!EncryptSecret(vMasterKeyIn, vchSecret, fvk.GetFingerprint(), vchCryptedSecret)) {
            return false;
        }
        if (!AddCryptedSaplingSpendingKey(fvk, vchCryptedSecret)) {
            return false;
        }
    }
    mapSaplingSpendingKeys.clear();
    return true;
}

bool CCryptoKeyStore::UnlockSaplingKeys(const CKeyingMaterial& vMasterKeyIn, bool fDecryptionThoroughlyChecked)
{
    bool keyFail = false;
    bool keyPass = false;
    CryptedSaplingSpendingKeyMap::const_iterator miSapling = mapCryptedSaplingSpendingKeys.begin();
    for (; miSapling != mapCryptedSaplingSpendingKeys.end(); ++miSapling) {
        const libzcash::SaplingFullViewingKey &fvk = (*miSapling).first;
        const std::vector<unsigned char> &vchCryptedSecret = (*miSapling).second;
        libzcash::SaplingExtendedSpendingKey sk;
        if (!DecryptSaplingSpendingKey(vMasterKeyIn, vchCryptedSecret, fvk, sk))
        {
            keyFail = true;
            break;
        }
        keyPass = true;
        if (fDecryptionThoroughlyChecked)
            break;
    }

    if (keyPass && keyFail) {
        LogPrintf("Sapling wallet is probably corrupted: Some keys decrypt but not all.");
        throw std::runtime_error("Error unlocking sapling wallet: some keys decrypt but not all. Your wallet file may be corrupt.");
    }
    if (keyFail || !keyPass)
        return false;

    return true;
}