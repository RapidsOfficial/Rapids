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


BOOST_FIXTURE_TEST_SUITE(wallet_zkeys_tests, BasicTestingSetup)

/**
  * This test covers Sapling methods on CWallet
  * GenerateNewSaplingZKey()
  */
BOOST_AUTO_TEST_CASE(testVectors) {

    CWallet wallet;
    // wallet should be empty
    // std::set<libzcash::SaplingPaymentAddress> addrs;
    // wallet.GetSaplingPaymentAddresses(addrs);
    // ASSERT_EQ(0, addrs.size());

    // wallet should have one key
    auto saplingAddr = wallet.GenerateNewSaplingZKey();
    // ASSERT_NE(boost::get<libzcash::SaplingPaymentAddress>(&address), nullptr);
    // auto sapling_addr = boost::get<libzcash::SaplingPaymentAddress>(saplingAddr);
    // wallet.GetSaplingPaymentAddresses(addrs);
    // ASSERT_EQ(1, addrs.size());

    auto sk = libzcash::SaplingSpendingKey::random();
    auto full_viewing_key = sk.full_viewing_key();
    BOOST_CHECK(wallet.AddSaplingSpendingKey(sk));

    // verify wallet has spending key for the address
    BOOST_CHECK(wallet.HaveSaplingSpendingKey(full_viewing_key));

    // check key is the same
    libzcash::SaplingSpendingKey keyOut;
    wallet.GetSaplingSpendingKey(full_viewing_key, keyOut);
    BOOST_CHECK(sk == keyOut);
}

BOOST_AUTO_TEST_SUITE_END()
