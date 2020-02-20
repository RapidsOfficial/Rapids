// Copyright (c) 2012-2013 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "utilstrencodings.h"
#include "test/test_pivx.h"

#include <boost/test/unit_test.hpp>
#include <librustzcash.h>

BOOST_FIXTURE_TEST_SUITE(rust_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(rust_testvectors)
{
    // Ensure that our Rust interactions are working in production builds. This is
    // temporary and should be removed.
    {
        BOOST_CHECK(librustzcash_xor(0x0f0f0f0f0f0f0f0f, 0x1111111111111111) == 0x1e1e1e1e1e1e1e1e);
    }

}

BOOST_AUTO_TEST_SUITE_END()
