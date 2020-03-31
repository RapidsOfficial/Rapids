// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_test_fixture.h"
#include "wallet/wallet.h"

#include "rpc/server.h"
#include "rpc/client.h"

#include "sapling/key_io_sapling.h"
#include "sapling/address.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include <univalue.h>

extern UniValue CallRPC(std::string args); // Implemented in rpc_tests.cpp

BOOST_FIXTURE_TEST_SUITE(sapling_rpc_wallet_tests, WalletTestingSetup)

/**
 * This test covers RPC command validateaddress
 */
BOOST_AUTO_TEST_CASE(rpc_wallet_sapling_validateaddress)
{
    SelectParams(CBaseChainParams::MAIN);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue retValue;

    // Check number of args
    BOOST_CHECK_THROW(CallRPC("validateaddress"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("validateaddress toomany args"), std::runtime_error);

    // Wallet should be empty:
    std::set<libzcash::SaplingPaymentAddress> addrs;
    pwalletMain->GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==0);

    // This Sapling address is not valid, it belongs to another network
    BOOST_CHECK_NO_THROW(retValue = CallRPC("validateaddress ptestsapling1nrn6exksuqtpld9gu6fwdz4hwg54h2x37gutdds89pfyg6mtjf63km45a8eare5qla45cj75vs8"));
    UniValue resultObj = retValue.get_obj();
    bool b = find_value(resultObj, "isvalid").get_bool();
    BOOST_CHECK_EQUAL(b, false);

    // This Sapling address is valid, but the spending key is not in this wallet
    BOOST_CHECK_NO_THROW(retValue = CallRPC("validateaddress ps1u87kylcmn28yclnx2uy0psnvuhs2xn608ukm6n2nshrpg2nzyu3n62ls8j77m9cgp40dx40evej"));
    resultObj = retValue.get_obj();
    b = find_value(resultObj, "isvalid").get_bool();
    BOOST_CHECK_EQUAL(b, true);
    BOOST_CHECK_EQUAL(find_value(resultObj, "type").get_str(), "sapling");
    b = find_value(resultObj, "ismine").get_bool();
    BOOST_CHECK_EQUAL(b, false);
    BOOST_CHECK_EQUAL(find_value(resultObj, "diversifier").get_str(), "e1fd627f1b9a8e4c7e6657");
    BOOST_CHECK_EQUAL(find_value(resultObj, "diversifiedtransmissionkey").get_str(), "d35e0d0897edbd3cf02b3d2327622a14c685534dbd2d3f4f4fa3e0e56cc2f008");
}

BOOST_AUTO_TEST_SUITE_END()
