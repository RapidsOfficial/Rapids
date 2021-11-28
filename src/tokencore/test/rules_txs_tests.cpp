#include "tokencore/tokencore.h"
#include "tokencore/rules.h"

#include "test/test_bitcoin.h"

#include <stdint.h>
#include <limits>

#include <boost/test/unit_test.hpp>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(tokencore_rules_txs_tests, BasicTestingSetup)

const int MAX_BLOCK = std::numeric_limits<int>::max();
const int MAX_VERSION = std::numeric_limits<uint16_t>::max();

BOOST_AUTO_TEST_CASE(simple_send_restrictions)
{
    int TOKEN_SEND_BLOCK = ConsensusParams().TOKEN_SEND_BLOCK;

    BOOST_CHECK(!IsTransactionTypeAllowed(0,                TOKEN_PROPERTY_BTC,  TOKEN_TYPE_SIMPLE_SEND, MP_TX_PKT_V0));
    BOOST_CHECK(!IsTransactionTypeAllowed(TOKEN_SEND_BLOCK,   TOKEN_PROPERTY_BTC,  TOKEN_TYPE_SIMPLE_SEND, MP_TX_PKT_V0));
    BOOST_CHECK(!IsTransactionTypeAllowed(MAX_BLOCK,        TOKEN_PROPERTY_BTC,  TOKEN_TYPE_SIMPLE_SEND, MAX_VERSION));
    BOOST_CHECK(!IsTransactionTypeAllowed(0,                TOKEN_PROPERTY_MSC,  TOKEN_TYPE_SIMPLE_SEND, MAX_VERSION));
    BOOST_CHECK(!IsTransactionTypeAllowed(TOKEN_SEND_BLOCK-1, TOKEN_PROPERTY_MSC,  TOKEN_TYPE_SIMPLE_SEND, MP_TX_PKT_V0));
    BOOST_CHECK(!IsTransactionTypeAllowed(MAX_BLOCK,        TOKEN_PROPERTY_MSC,  TOKEN_TYPE_SIMPLE_SEND, MP_TX_PKT_V1));
    BOOST_CHECK(!IsTransactionTypeAllowed(TOKEN_SEND_BLOCK,   TOKEN_PROPERTY_TMSC, TOKEN_TYPE_SIMPLE_SEND, MAX_VERSION));
    BOOST_CHECK(!IsTransactionTypeAllowed(MAX_BLOCK,        TOKEN_PROPERTY_TMSC, TOKEN_TYPE_SIMPLE_SEND, MP_TX_PKT_V1));

    BOOST_CHECK(IsTransactionTypeAllowed(TOKEN_SEND_BLOCK,    TOKEN_PROPERTY_MSC,  TOKEN_TYPE_SIMPLE_SEND, MP_TX_PKT_V0));
    BOOST_CHECK(IsTransactionTypeAllowed(MAX_BLOCK,         TOKEN_PROPERTY_MSC,  TOKEN_TYPE_SIMPLE_SEND, MP_TX_PKT_V0));
    BOOST_CHECK(IsTransactionTypeAllowed(0,                 TOKEN_PROPERTY_TMSC, TOKEN_TYPE_SIMPLE_SEND, MP_TX_PKT_V0));
    BOOST_CHECK(IsTransactionTypeAllowed(MAX_BLOCK,         TOKEN_PROPERTY_TMSC, TOKEN_TYPE_SIMPLE_SEND, MP_TX_PKT_V0));
}


BOOST_AUTO_TEST_SUITE_END()
