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

bool CCryptoKeyStore::AddSaplingSpendingKey(const libzcash::SaplingSpendingKey &sk)
{
    {
        LOCK(cs_SpendingKeyStore);
        if (!IsCrypted()) {
            return CBasicKeyStore::AddSaplingSpendingKey(sk);
        }

        if (IsLocked()) {
            return false;
        }

        std::vector<unsigned char> vchCryptedSecret;
        CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << sk;
        CKeyingMaterial vchSecret(ss.begin(), ss.end());
        auto address = sk.default_address();
        auto fvk = sk.full_viewing_key();
        if (!EncryptSecret(vMasterKey, vchSecret, fvk.GetFingerprint(), vchCryptedSecret)) {
            return false;
        }

        if (!AddCryptedSaplingSpendingKey(fvk, vchCryptedSecret)) {
            return false;
        }
    }
    return true;
}

// TODO: Handle note decryptors
bool CCryptoKeyStore::AddCryptedSaplingSpendingKey(const libzcash::SaplingFullViewingKey &fvk,
                                                   const std::vector<unsigned char> &vchCryptedSecret)
{
    LOCK(cs_SpendingKeyStore);
    if (!SetCrypted()) {
        return false;
    }

    mapCryptedSaplingSpendingKeys[fvk] = vchCryptedSecret;
    return true;
}

static bool DecryptSaplingSpendingKey(const CKeyingMaterial& vMasterKey,
                                      const std::vector<unsigned char>& vchCryptedSecret,
                                      const libzcash::SaplingFullViewingKey& fvk,
                                      libzcash::SaplingSpendingKey& sk)
{
    CKeyingMaterial vchSecret;
    if(!DecryptSecret(vMasterKey, vchCryptedSecret, fvk.GetFingerprint(), vchSecret))
        return false;

    if (vchSecret.size() != libzcash::SerializedSaplingSpendingKeySize)
        return false;

    CSecureDataStream ss(vchSecret, SER_NETWORK, PROTOCOL_VERSION);
    ss >> sk;
    return sk.full_viewing_key() == fvk;
}

bool CCryptoKeyStore::GetSaplingSpendingKey(const libzcash::SaplingFullViewingKey &fvk, libzcash::SaplingSpendingKey &skOut) const
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