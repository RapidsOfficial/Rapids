// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_test_fixture.h"

#include "sapling/util.h"
#include "sapling/address.hpp"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "util.h"
#include <boost/test/unit_test.hpp>

/**
 * This test covers methods on CWallet
 * GenerateNewZKey()
 * AddZKey()
 * LoadZKey()
 * LoadZKeyMetadata()
 */

BOOST_FIXTURE_TEST_SUITE(wallet_zkeys_tests, WalletTestingSetup)

/**
  * This test covers Sapling methods on CWallet
  * GenerateNewSaplingZKey()
  */
BOOST_AUTO_TEST_CASE(StoreAndLoadSaplingZkeys) {
    SelectParams(CBaseChainParams::MAIN);

    CWallet wallet;

    // wallet should be empty
    std::set<libzcash::SaplingPaymentAddress> addrs;
    wallet.GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(0, addrs.size());

    // No HD seed in the wallet
    BOOST_CHECK_THROW(wallet.GenerateNewSaplingZKey(), std::runtime_error);

    // Random seed
    CKey seed;
    seed.MakeNewKey(true);
    wallet.AddKeyPubKey(seed, seed.GetPubKey());
    wallet.GetSaplingScriptPubKeyMan()->SetHDSeed(seed.GetPubKey(), false, true);

    // wallet should have one key
    auto address = wallet.GenerateNewSaplingZKey();
    wallet.GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(1, addrs.size());

    // verify wallet has incoming viewing key for the address
    BOOST_CHECK(wallet.HaveSaplingIncomingViewingKey(address));

    // manually add new spending key to wallet
    HDSeed seed1(seed.GetPrivKey());
    auto m = libzcash::SaplingExtendedSpendingKey::Master(seed1);
    auto sk = m.Derive(0);
    BOOST_CHECK(wallet.AddSaplingZKey(sk, sk.DefaultAddress()));

    // verify wallet did add it
    auto fvk = sk.expsk.full_viewing_key();
    BOOST_CHECK(wallet.HaveSaplingSpendingKey(fvk));

    // verify spending key stored correctly
    libzcash::SaplingExtendedSpendingKey keyOut;
    wallet.GetSaplingSpendingKey(fvk, keyOut);
    BOOST_CHECK(sk == keyOut);

    // verify there are two keys
    wallet.GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(2, addrs.size());
    BOOST_CHECK_EQUAL(1, addrs.count(address));
    BOOST_CHECK_EQUAL(1, addrs.count(sk.DefaultAddress()));
}

/**
  * This test covers methods on CWalletDB to load/save crypted sapling z keys.
  */
BOOST_AUTO_TEST_CASE(WriteCryptedSaplingZkeyDirectToDb) {
    SelectParams(CBaseChainParams::TESTNET);

    BOOST_CHECK(!pwalletMain->HasSaplingSPKM());
    assert(pwalletMain->SetupSPKM(true));

    // wallet should be empty
    std::set<libzcash::SaplingPaymentAddress> addrs;
    pwalletMain->GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(0, addrs.size());

    // Add random key to the wallet
    auto address = pwalletMain->GenerateNewSaplingZKey();

    // wallet should have one key
    pwalletMain->GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(1, addrs.size());

    // encrypt wallet
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = "hello";
    BOOST_CHECK(pwalletMain->EncryptWallet(strWalletPass));

    // adding a new key will fail as the wallet is locked
    BOOST_CHECK_THROW(pwalletMain->GenerateNewSaplingZKey(), std::runtime_error);

    // unlock wallet and then add
    pwalletMain->Unlock(strWalletPass);
    libzcash::SaplingPaymentAddress address2 = pwalletMain->GenerateNewSaplingZKey();

    // Create a new wallet from the existing wallet path
    bool fFirstRun;
    CWallet wallet2(pwalletMain->strWalletFile);
    BOOST_CHECK_EQUAL(DB_LOAD_OK, wallet2.LoadWallet(fFirstRun));

    // Confirm it's not the same as the other wallet
    BOOST_CHECK(pwalletMain != &wallet2);
    BOOST_CHECK(wallet2.HasSaplingSPKM());

    // wallet should have two keys
    wallet2.GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(2, addrs.size());

    //check we have entries for our payment addresses
    BOOST_CHECK(addrs.count(address));
    BOOST_CHECK(addrs.count(address2));

    // spending key is crypted, so we can't extract valid payment address
    libzcash::SaplingExtendedSpendingKey keyOut;
    BOOST_CHECK(!wallet2.GetSaplingExtendedSpendingKey(address, keyOut));
    assert(!(address == keyOut.DefaultAddress()));

    // unlock wallet to get spending keys and verify payment addresses
    wallet2.Unlock(strWalletPass);

    BOOST_CHECK(wallet2.GetSaplingExtendedSpendingKey(address, keyOut));
    BOOST_CHECK(address == keyOut.DefaultAddress());

    BOOST_CHECK(wallet2.GetSaplingExtendedSpendingKey(address2, keyOut));
    BOOST_CHECK(address2 == keyOut.DefaultAddress());
}


BOOST_AUTO_TEST_SUITE_END()
