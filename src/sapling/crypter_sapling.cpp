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

bool CCryptoKeyStore::AddCryptedSaplingSpendingKey(
        const libzcash::SaplingExtendedFullViewingKey &extfvk,
        const std::vector<unsigned char> &vchCryptedSecret,
        const libzcash::SaplingPaymentAddress &defaultAddr)
{
    LOCK(cs_KeyStore);
    if (!SetCrypted()) {
        return false;
    }

    // if SaplingFullViewingKey is not in SaplingFullViewingKeyMap, add it
    if (!AddSaplingFullViewingKey(extfvk.fvk, defaultAddr)){
        return false;
    }

    mapCryptedSaplingSpendingKeys[extfvk] = vchCryptedSecret;
    return true;
}

static bool DecryptSaplingSpendingKey(const CKeyingMaterial& vMasterKey,
                                      const std::vector<unsigned char>& vchCryptedSecret,
                                      const libzcash::SaplingExtendedFullViewingKey& extfvk,
                                      libzcash::SaplingExtendedSpendingKey& sk)
{
    CKeyingMaterial vchSecret;
    if(!DecryptSecret(vMasterKey, vchCryptedSecret, extfvk.fvk.GetFingerprint(), vchSecret))
        return false;

    if (vchSecret.size() != ZIP32_XSK_SIZE)
        return false;

    CSecureDataStream ss(vchSecret, SER_NETWORK, PROTOCOL_VERSION);
    ss >> sk;
    return sk.expsk.full_viewing_key() == extfvk.fvk;
}

bool CCryptoKeyStore::GetSaplingSpendingKey(const libzcash::SaplingFullViewingKey &fvk, libzcash::SaplingExtendedSpendingKey &skOut) const
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::GetSaplingSpendingKey(fvk, skOut);

        for (auto entry : mapCryptedSaplingSpendingKeys) { // Work more on this flow..
            if (entry.first.fvk == fvk) {
                const std::vector<unsigned char> &vchCryptedSecret = entry.second;
                return DecryptSaplingSpendingKey(vMasterKey, vchCryptedSecret, entry.first, skOut);
            }
        }
    }
    return false;
}

bool CCryptoKeyStore::HaveSaplingSpendingKey(const libzcash::SaplingFullViewingKey &fvk) const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted())
        return CBasicKeyStore::HaveSaplingSpendingKey(fvk);
    for (auto entry : mapCryptedSaplingSpendingKeys) { // work more on this flow..
        if (entry.first.fvk == fvk) {
            return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::UnlockSaplingKeys(const CKeyingMaterial& vMasterKeyIn, bool fDecryptionThoroughlyChecked)
{
    if (mapCryptedSaplingSpendingKeys.empty()) {
        LogPrintf("%s: mapCryptedSaplingSpendingKeys empty. No need to unlock anything.\n", __func__);
        return true;
    }

    bool keyFail = false;
    bool keyPass = false;
    CryptedSaplingSpendingKeyMap::const_iterator miSapling = mapCryptedSaplingSpendingKeys.begin();
    for (; miSapling != mapCryptedSaplingSpendingKeys.end(); ++miSapling) {
        const libzcash::SaplingExtendedFullViewingKey &extfvk = (*miSapling).first;
        const std::vector<unsigned char> &vchCryptedSecret = (*miSapling).second;
        libzcash::SaplingExtendedSpendingKey sk;
        if (!DecryptSaplingSpendingKey(vMasterKeyIn, vchCryptedSecret, extfvk, sk)) {
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
