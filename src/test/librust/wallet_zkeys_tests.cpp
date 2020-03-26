// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"

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

BOOST_FIXTURE_TEST_SUITE(wallet_zkeys_tests, BasicTestingSetup)

/**
  * This test covers Sapling methods on CWallet
  * GenerateNewSaplingZKey()
  */
BOOST_AUTO_TEST_CASE(store_and_load_sapling_zkeys) {
    SelectParams(CBaseChainParams::MAIN);

    CWallet wallet;

    // wallet should be empty
    std::set<libzcash::SaplingPaymentAddress> addrs;
    wallet.GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(0, addrs.size());

    auto address = wallet.GenerateNewSaplingZKey();
    wallet.GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(1, addrs.size());

    // verify wallet has incoming viewing key for the address
    BOOST_CHECK(wallet.HaveSaplingIncomingViewingKey(address));

    // manually add new spending key to wallet
    libzcash::SaplingSpendingKey sk = libzcash::SaplingSpendingKey::random();
    BOOST_CHECK(wallet.AddSaplingZKey(sk, sk.default_address()));

    // verify wallet did add it
    auto fvk = sk.full_viewing_key();
    BOOST_CHECK(wallet.HaveSaplingSpendingKey(fvk));

    // verify spending key stored correctly
    libzcash::SaplingSpendingKey keyOut;
    wallet.GetSaplingSpendingKey(fvk, keyOut);
    BOOST_CHECK(sk == keyOut);

    // verify there are two keys
    wallet.GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(2, addrs.size());
    BOOST_CHECK_EQUAL(1, addrs.count(address));
    BOOST_CHECK_EQUAL(1, addrs.count(sk.default_address()));
}

BOOST_AUTO_TEST_SUITE_END()
