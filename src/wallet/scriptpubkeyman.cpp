// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/scriptpubkeyman.h"
#include "crypter.h"
#include "script/standard.h"

bool ScriptPubKeyMan::SetupGeneration(bool force)
{
    if (CanGenerateKeys() && !force) {
        return false;
    }

    SetHDSeed(GenerateNewSeed(), force);
    if (!NewKeyPool()) {
        return false;
    }
    return true;
}

bool ScriptPubKeyMan::CanGenerateKeys()
{
    // A wallet can generate keys if it has an HD seed (IsHDEnabled) or it is a non-HD wallet (pre FEATURE_HD)
    LOCK(wallet->cs_wallet);
    return IsHDEnabled();
}

bool ScriptPubKeyMan::IsHDEnabled() const
{
    return !hdChain.IsNull();
}

bool ScriptPubKeyMan::CanGetAddresses(bool internal)
{
    LOCK(wallet->cs_wallet);
    // Check if the keypool has keys
    bool keypool_has_keys;
    if (internal && wallet->CanSupportFeature(FEATURE_HD_SPLIT)) {
        keypool_has_keys = setInternalKeyPool.size() > 0;
    } else {
        keypool_has_keys = KeypoolCountExternalKeys() > 0;
    }
    // If the keypool doesn't have keys, check if we can generate them
    if (!keypool_has_keys) {
        return CanGenerateKeys();
    }
    return keypool_has_keys;
}

size_t ScriptPubKeyMan::KeypoolCountExternalKeys()
{
    AssertLockHeld(wallet->cs_wallet);
    return setExternalKeyPool.size() + set_pre_split_keypool.size();
}

unsigned int ScriptPubKeyMan::GetKeyPoolSize() const
{
    AssertLockHeld(wallet->cs_wallet);
    return setInternalKeyPool.size() + setExternalKeyPool.size() + set_pre_split_keypool.size();
}

bool ScriptPubKeyMan::GetKeyFromPool(CPubKey& result, bool internal)
{
    if (!CanGetAddresses(internal)) {
        LogPrintf("Cannot get address\n");
        return false;
    }

    CKeyPool keypool;
    {
        LOCK(wallet->cs_wallet);
        int64_t nIndex;
        if (!ReserveKeyFromKeyPool(nIndex, keypool, internal)) {
            if (wallet->IsLocked()) {
                LogPrintf("Wallet locked, cannot get address\n");
                return false;
            }
            CWalletDB batch(wallet->strWalletFile);
            result = GenerateNewKey(batch, internal);
            return true;
        }
        KeepDestination(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

bool ScriptPubKeyMan::GetReservedKey(bool internal, int64_t& index, CKeyPool& keypool)
{
    if (!CanGetAddresses(internal)) {
        return false;
    }

    if (!ReserveKeyFromKeyPool(index, keypool, internal)) {
        return false;
    }
    return true;
}

bool ScriptPubKeyMan::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool, bool fRequestedInternal)
{
    nIndex = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(wallet->cs_wallet);

        bool fReturningInternal = fRequestedInternal;
        fReturningInternal &= (IsHDEnabled() && wallet->CanSupportFeature(FEATURE_HD_SPLIT));
        bool use_split_keypool = set_pre_split_keypool.empty();
        std::set<int64_t>& setKeyPool = use_split_keypool ? (fReturningInternal ? setInternalKeyPool : setExternalKeyPool) : set_pre_split_keypool;

        // Get the oldest key
        if (setKeyPool.empty()) {
            return false;
        }

        CWalletDB batch(wallet->strWalletFile);

        auto it = setKeyPool.begin();
        nIndex = *it;
        setKeyPool.erase(it);
        if (!batch.ReadPool(nIndex, keypool)) {
            throw std::runtime_error(std::string(__func__) + ": read failed");
        }
        CPubKey pk;
        if (!wallet->GetPubKey(keypool.vchPubKey.GetID(), pk)) {
            throw std::runtime_error(std::string(__func__) + ": unknown key in key pool");
        }
        // If the key was pre-split keypool, we don't care about what type it is
        if (use_split_keypool && keypool.fInternal != fReturningInternal) {
            throw std::runtime_error(std::string(__func__) + ": keypool entry misclassified");
        }
        if (!keypool.vchPubKey.IsValid()) {
            throw std::runtime_error(std::string(__func__) + ": keypool entry invalid");
        }

        assert(m_index_to_reserved_key.count(nIndex) == 0);
        m_index_to_reserved_key[nIndex] = keypool.vchPubKey.GetID();
        m_pool_key_to_index.erase(keypool.vchPubKey.GetID());
        LogPrintf("%s: keypool reserve %d\n", __func__, nIndex);
    }
    //NotifyCanGetAddressesChanged();
    return true;
}

void ScriptPubKeyMan::KeepDestination(int64_t nIndex)
{
    // Remove from key pool
    CWalletDB batch(wallet->strWalletFile);
    batch.ErasePool(nIndex);
    CPubKey pubkey;
    bool have_pk = wallet->GetPubKey(m_index_to_reserved_key.at(nIndex), pubkey);
    assert(have_pk);
    m_index_to_reserved_key.erase(nIndex);
    LogPrintf("keypool keep %d\n", nIndex);
}

void ScriptPubKeyMan::ReturnDestination(int64_t nIndex, bool fInternal, const CTxDestination&)
{
    // Return to key pool
    {
        LOCK(wallet->cs_wallet);
        if (fInternal) {
            setInternalKeyPool.insert(nIndex);
        } else if (!set_pre_split_keypool.empty()) {
            set_pre_split_keypool.insert(nIndex);
        } else {
            setExternalKeyPool.insert(nIndex);
        }
        CKeyID& pubkey_id = m_index_to_reserved_key.at(nIndex);
        m_pool_key_to_index[pubkey_id] = nIndex;
        m_index_to_reserved_key.erase(nIndex);
        //NotifyCanGetAddressesChanged();
    }
    LogPrintf("%s: keypool return %d\n", __func__, nIndex);
}

void ScriptPubKeyMan::MarkReserveKeysAsUsed(int64_t keypool_id)
{
    AssertLockHeld(wallet->cs_wallet);
    bool internal = setInternalKeyPool.count(keypool_id);
    if (!internal) assert(setExternalKeyPool.count(keypool_id) || set_pre_split_keypool.count(keypool_id));
    std::set<int64_t> *setKeyPool = internal ? &setInternalKeyPool : (set_pre_split_keypool.empty() ? &setExternalKeyPool : &set_pre_split_keypool);
    auto it = setKeyPool->begin();

    CWalletDB batch(wallet->strWalletFile);
    while (it != std::end(*setKeyPool)) {
        const int64_t& index = *(it);
        if (index > keypool_id) break; // set*KeyPool is ordered

        CKeyPool keypool;
        if (batch.ReadPool(index, keypool)) { //TODO: This should be unnecessary
            m_pool_key_to_index.erase(keypool.vchPubKey.GetID());
        }
        batch.ErasePool(index);
        LogPrintf("keypool index %d removed\n", index);
        it = setKeyPool->erase(it);
    }
}

void ScriptPubKeyMan::MarkUnusedAddresses(const CScript& script)
{
    AssertLockHeld(wallet->cs_wallet);
    // extract addresses and check if they match with an unused keypool key
    for (const auto& keyid : wallet->GetAffectedKeys(script)) {
        std::map<CKeyID, int64_t>::const_iterator mi = m_pool_key_to_index.find(keyid);
        if (mi != m_pool_key_to_index.end()) {
            LogPrintf("%s: Detected a used keypool key, mark all keypool key up to this key as used\n", __func__);
            MarkReserveKeysAsUsed(mi->second);

            if (!TopUp()) {
                LogPrintf("%s: Topping up keypool failed (locked wallet)\n", __func__);
            }
        }
    }
}

/**
 * Mark old keypool keys as used,
 * and generate all new keys
 */
bool ScriptPubKeyMan::NewKeyPool()
{
    {
        LOCK(wallet->cs_wallet);

        CWalletDB walletdb(wallet->strWalletFile);
        // Internal
        for (const int64_t nIndex : setInternalKeyPool) {
            walletdb.ErasePool(nIndex);
        }
        setInternalKeyPool.clear();

        // External
        for (const int64_t nIndex : setExternalKeyPool) {
            walletdb.ErasePool(nIndex);
        }
        setExternalKeyPool.clear();

        // key -> index.
        m_pool_key_to_index.clear();

        if (!TopUp()) {
            return false;
        }
        LogPrintf("ScriptPubKeyMan::NewKeyPool rewrote keypool\n");
    }
    return true;
}

/**
 * Fill the key pool
 */
bool ScriptPubKeyMan::TopUp(unsigned int kpSize)
{
    if (!CanGenerateKeys()) {
        return false;
    }
    {
        LOCK(wallet->cs_wallet);
        if (wallet->IsLocked()) return false;

        // Top up key pool
        unsigned int nTargetSize;
        if (kpSize > 0)
            nTargetSize = kpSize;
        else
            nTargetSize = std::max(GetArg("-keypool", DEFAULT_KEYPOOL_SIZE), (int64_t) 0);

        // Count amount of available keys (internal, external)
        // make sure the keypool of external and internal keys fits the user selected target (-keypool)
        int64_t missingExternal = std::max(std::max((int64_t) nTargetSize, (int64_t) 1) - (int64_t)setExternalKeyPool.size(), (int64_t) 0);
        int64_t missingInternal = std::max(std::max((int64_t) nTargetSize, (int64_t) 1) - (int64_t)setInternalKeyPool.size(), (int64_t) 0);

        if (!IsHDEnabled() || !wallet->CanSupportFeature(FEATURE_HD_SPLIT)) {
            // don't create extra internal keys
            missingInternal = 0;
        }
        bool internal = false;
        CWalletDB batch(wallet->strWalletFile);
        for (int64_t i = missingInternal + missingExternal; i--;) {
            if (i < missingInternal) {
                internal = true;
            }

            CPubKey pubkey(GenerateNewKey(batch, internal));
            AddKeypoolPubkeyWithDB(pubkey, internal, batch);
        }
        if (missingInternal + missingExternal > 0) {
            LogPrintf("keypool added %d keys (%d internal), size=%u (%u internal)\n", missingInternal + missingExternal, missingInternal, setInternalKeyPool.size() + setExternalKeyPool.size() + set_pre_split_keypool.size(), setInternalKeyPool.size());
        }
    }
    // TODO: Implement this.
    //NotifyCanGetAddressesChanged();
    return true;
}

void ScriptPubKeyMan::AddKeypoolPubkeyWithDB(const CPubKey& pubkey, const bool internal, CWalletDB &batch)
{
    LOCK(wallet->cs_wallet);
    assert(m_max_keypool_index < std::numeric_limits<int64_t>::max()); // How in the hell did you use so many keys?
    int64_t index = ++m_max_keypool_index;
    if (!batch.WritePool(index, CKeyPool(pubkey, internal))) {
        throw std::runtime_error(std::string(__func__) + ": writing imported pubkey failed");
    }
    if (internal) {
        setInternalKeyPool.insert(index);
    } else {
        setExternalKeyPool.insert(index);
    }
    m_pool_key_to_index[pubkey.GetID()] = index;
}

/**
 * Generate a new key and stores it in db.
 */
CPubKey ScriptPubKeyMan::GenerateNewKey(CWalletDB &batch, bool internal)
{
    AssertLockHeld(wallet->cs_wallet);
    bool fCompressed = wallet->CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    CKey secret;

    // Create new metadata
    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    // use HD key derivation if HD was enabled during wallet creation and a seed is present
    if (IsHDEnabled()) {
        DeriveNewChildKey(batch, metadata, secret, (wallet->CanSupportFeature(FEATURE_HD_SPLIT) ? internal : false));
    } else {
        secret.MakeNewKey(fCompressed);
    }

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed) {
        wallet->SetMinVersion(FEATURE_COMPRPUBKEY);
    }

    CPubKey pubkey = secret.GetPubKey();
    assert(secret.VerifyPubKey(pubkey));

    wallet->mapKeyMetadata[pubkey.GetID()] = metadata;
    UpdateTimeFirstKey(nCreationTime);

    if (!AddKeyPubKeyWithDB(batch, secret, pubkey)) {
        throw std::runtime_error(std::string(__func__) + ": AddKey failed");
    }
    return pubkey;
}

const uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

void ScriptPubKeyMan::DeriveNewChildKey(CWalletDB &batch, CKeyMetadata& metadata, CKey& secret, bool internal)
{
    AssertLockHeld(wallet->cs_wallet);
    // Use BIP44 keypath scheme i.e. m / purpose' / coin_type' / account' / change / address_index
    CKey seed;                     //seed (256bit)
    CExtKey masterKey;             //hd master key
    CExtKey purposeKey;            //key at m/purpose' --> key at m/44'
    CExtKey cointypeKey;           //key at m/purpose'/coin_type'  --> key at m/44'/119'
    CExtKey accountKey;            //key at m/purpose'/coin_type'/account' ---> key at m/44'/119'/account_num
    CExtKey changeKey;             //key at m/purpose'/coin_type'/account'/change ---> key at m/44'/119'/account_num/change', external = 0' or internal = 1'.
    CExtKey childKey;              //key at m/purpose'/coin_type'/account'/change/address_index ---> key at m/44'/119'/account_num/change'/<n>'

    // For now only one account.
    int nAccountNumber = 0;
    // try to get the seed
    if (!wallet->GetKey(hdChain.GetID(), seed))
        throw std::runtime_error(std::string(__func__) + ": seed not found");

    masterKey.SetSeed(seed.begin(), seed.size());

    // derive m/0'
    // use hardened derivation (child keys >= 0x80000000 are hardened after bip32)
    masterKey.Derive(purposeKey, 44 | BIP32_HARDENED_KEY_LIMIT);
    // derive m/purpose'/coin_type'
    purposeKey.Derive(cointypeKey, 119 | BIP32_HARDENED_KEY_LIMIT);
    // derive m/purpose'/coin_type'/account' // Hardcoded to account 0 for now.
    cointypeKey.Derive(accountKey, nAccountNumber | BIP32_HARDENED_KEY_LIMIT);
    // derive m/purpose'/coin_type'/account'/change
    assert(internal ? wallet->CanSupportFeature(FEATURE_HD_SPLIT) : true);
    accountKey.Derive(changeKey, BIP32_HARDENED_KEY_LIMIT+(internal ? 1 : 0));

    // derive child key at next index, skip keys already known to the wallet
    do {
        // always derive hardened keys
        // childIndex | BIP32_HARDENED_KEY_LIMIT = derive childIndex in hardened child-index-range
        // example: 1 | BIP32_HARDENED_KEY_LIMIT == 0x80000001 == 2147483649

        // m/44'/119'/account_num/change'/<n>'
        metadata.key_origin.path.push_back(44 | BIP32_HARDENED_KEY_LIMIT);
        metadata.key_origin.path.push_back(119 | BIP32_HARDENED_KEY_LIMIT);
        metadata.key_origin.path.push_back(nAccountNumber | BIP32_HARDENED_KEY_LIMIT);
        if (internal) {
            changeKey.Derive(childKey, hdChain.nInternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            metadata.key_origin.path.push_back(1 | BIP32_HARDENED_KEY_LIMIT);
            metadata.key_origin.path.push_back(hdChain.nInternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            hdChain.nInternalChainCounter++;
        }
        else {
            changeKey.Derive(childKey, hdChain.nExternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            metadata.key_origin.path.push_back(0 | BIP32_HARDENED_KEY_LIMIT);
            metadata.key_origin.path.push_back(hdChain.nExternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            hdChain.nExternalChainCounter++;
        }
    } while (wallet->HaveKey(childKey.key.GetPubKey().GetID()));
    secret = childKey.key;
    metadata.hd_seed_id = hdChain.GetID();
    CKeyID master_id = masterKey.key.GetPubKey().GetID();
    std::copy(master_id.begin(), master_id.begin() + 4, metadata.key_origin.fingerprint);
    // update the chain model in the database
    if (!batch.WriteHDChain(hdChain))
        throw std::runtime_error(std::string(__func__) + ": Writing HD chain model failed");
}

void ScriptPubKeyMan::LoadKeyPool(int64_t nIndex, const CKeyPool &keypool)
{
    AssertLockHeld(wallet->cs_wallet);
    if (keypool.m_pre_split) {
        set_pre_split_keypool.insert(nIndex);
    } else if (keypool.fInternal) {
        setInternalKeyPool.insert(nIndex);
    } else {
        setExternalKeyPool.insert(nIndex);
    }
    m_max_keypool_index = std::max(m_max_keypool_index, nIndex);
    m_pool_key_to_index[keypool.vchPubKey.GetID()] = nIndex;

    // If no metadata exists yet, create a default with the pool key's
    // creation time. Note that this may be overwritten by actually
    // stored metadata for that key later, which is fine.
    CKeyID keyid = keypool.vchPubKey.GetID();
    if (wallet->mapKeyMetadata.count(keyid) == 0)
        wallet->mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);
}

/**
 * Update wallet first key creation time. This should be called whenever keys
 * are added to the wallet, with the oldest key creation time.
 */
void ScriptPubKeyMan::UpdateTimeFirstKey(int64_t nCreateTime)
{
    AssertLockHeld(wallet->cs_wallet);
    if (nCreateTime <= 1) {
        // Cannot determine birthday information, so set the wallet birthday to
        // the beginning of time.
        wallet->nTimeFirstKey = 1;
    } else if (!wallet->nTimeFirstKey || nCreateTime < wallet->nTimeFirstKey) {
        wallet->nTimeFirstKey = nCreateTime;
    }
}

bool ScriptPubKeyMan::AddKeyPubKeyWithDB(CWalletDB& batch, const CKey& secret, const CPubKey& pubkey)
{
    AssertLockHeld(wallet->cs_wallet);

    // Make sure we aren't adding private keys to private key disabled wallets
    //assert(!m_storage.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS));

    // FillableSigningProvider has no concept of wallet databases, but calls AddCryptedKey
    // which is overridden below.  To avoid flushes, the database handle is
    // tunneled through to it.
    bool needsDB = !encrypted_batch;
    if (needsDB) {
        encrypted_batch = &batch;
    }
    if (!AddKeyPubKeyInner(secret, pubkey)) {
        if (needsDB) encrypted_batch = nullptr;
        return false;
    }
    if (needsDB) encrypted_batch = nullptr;
    return true;
}

bool ScriptPubKeyMan::AddKeyPubKeyInner(const CKey& key, const CPubKey &pubkey)
{
    LOCK(wallet->cs_KeyStore);
    if (!wallet->HasEncryptionKeys()) {
        return wallet->AddKeyPubKey(key, pubkey);
    }

    if (wallet->IsLocked()) {
        return false;
    }

    std::vector<unsigned char> vchCryptedSecret;
    CKeyingMaterial vchSecret(key.begin(), key.end());
    if (!EncryptSecret(wallet->GetEncryptionKey(), vchSecret, pubkey.GetHash(), vchCryptedSecret)) {
        return false;
    }

    if (!wallet->AddCryptedKey(pubkey, vchCryptedSecret)) {
        return false;
    }
    return true;
}

////////////////////// Seed Generation ///////////////////////////////////

CPubKey ScriptPubKeyMan::GenerateNewSeed()
{
    CKey secret;
    secret.MakeNewKey(true);
    return DeriveNewSeed(secret);
}

CPubKey ScriptPubKeyMan::DeriveNewSeed(const CKey& key)
{
    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    // Calculate the seed
    CPubKey seed = key.GetPubKey();
    assert(key.VerifyPubKey(seed));

    // Set seed id
    metadata.hd_seed_id = seed.GetID();

    {
        LOCK(wallet->cs_wallet);

        // mem store the metadata
        wallet->mapKeyMetadata[seed.GetID()] = metadata;

        // write the key&metadata to the database
        if (!wallet->AddKeyPubKey(key, seed))
            throw std::runtime_error(std::string(__func__) + ": AddKeyPubKey failed");
    }

    return seed;
}


//////////////////////////////////////////////////////////////////////

void ScriptPubKeyMan::SetHDSeed(const CPubKey& seed, bool force)
{
    if (!hdChain.IsNull() && !force)
        throw std::runtime_error(std::string(__func__) + ": trying to set a hd seed on an already created chain");

    LOCK(wallet->cs_wallet);
    // store the keyid (hash160) together with
    // the child index counter in the database
    // as a hdChain object
    CHDChain newHdChain;
    if (!newHdChain.SetSeed(seed.GetID()) ) {
        throw std::runtime_error(std::string(__func__) + ": set hd seed failed");
    }

    SetHDChain(newHdChain, false);
    // TODO: Connect this if is needed.
    //NotifyCanGetAddressesChanged();
}

void ScriptPubKeyMan::SetHDChain(CHDChain& chain, bool memonly)
{
    LOCK(wallet->cs_wallet);
    if (!memonly && !CWalletDB(wallet->strWalletFile).WriteHDChain(chain))
        throw std::runtime_error(std::string(__func__) + ": writing chain failed");

    hdChain = chain;

    // Sanity check
    if (!wallet->HaveKey(hdChain.GetID()))
        throw std::runtime_error(std::string(__func__) + ": Not found seed in walelt");
}