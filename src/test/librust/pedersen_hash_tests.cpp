// Copyright (c) 2012-2013 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uint256.h"
#include "utilstrencodings.h"
#include "test/test_pivx.h"

#include <boost/test/unit_test.hpp>
#include <librustzcash.h>

BOOST_FIXTURE_TEST_SUITE(pedersen_hash_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(pedersen_hash_testvectors)
{
    // Good scenario check
    const uint256 a = uint256S("0acaa62d40fcdd9192ed35ea9df31660ccf7f6c60566530faaa444fb5d0d410e");
    const uint256 b = uint256S("6041357de59ba64959d1b60f93de24dfe5ea1e26ed9e8a73d35b225a1845ba70");
    uint256 result;

    librustzcash_merkle_hash(25, a.begin(), b.begin(), result.begin());

    uint256 expected_result = uint256S("4253b36834b3f64cc6182f1816911e1c9460cb88afeafb155244dd0038ad4717");

    BOOST_CHECK(result == expected_result);

    // Simple bad scenario check
    const uint256 c = uint256(1) + a;
    result.SetNull();
    librustzcash_merkle_hash(25, c.begin(), b.begin(), result.begin());
    BOOST_CHECK(result != expected_result);
}

BOOST_AUTO_TEST_SUITE_END()
