#include "tokencore/tokencore.h"
#include "tokencore/rules.h"

#include "chainparams.h"
#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

#include <stdint.h>
#include <string>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(tokencore_params_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(get_params)
{
    const CConsensusParams& params = ConsensusParams();
    BOOST_CHECK_EQUAL(params.exodusReward, 100);
}

BOOST_AUTO_TEST_CASE(network_restrictions_main)
{
    const CConsensusParams& params = ConsensusParams("main");
    BOOST_CHECK_EQUAL(params.TOKEN_STO_BLOCK, 342650);
}

BOOST_AUTO_TEST_CASE(network_restrictions_test)
{
    const CConsensusParams& params = ConsensusParams("test");
    BOOST_CHECK_EQUAL(params.TOKEN_STO_BLOCK, 0);
    BOOST_CHECK_EQUAL(params.TOKEN_METADEX_BLOCK, 0);
}

BOOST_AUTO_TEST_CASE(ecosystem_restrictions_main)
{
    // Unit tests and mainnet use the same params
    BOOST_CHECK(!IsTransactionTypeAllowed(0, TOKEN_PROPERTY_MSC, TOKEN_TYPE_OFFER_ACCEPT_A_BET, MP_TX_PKT_V0));
    BOOST_CHECK(IsTransactionTypeAllowed(0, TOKEN_PROPERTY_TMSC, TOKEN_TYPE_OFFER_ACCEPT_A_BET, MP_TX_PKT_V0));
}

BOOST_AUTO_TEST_CASE(ecosystem_restrictions_test)
{
    SelectParams(CBaseChainParams::TESTNET);
    BOOST_CHECK(!IsTransactionTypeAllowed(0, TOKEN_PROPERTY_MSC, TOKEN_TYPE_OFFER_ACCEPT_A_BET, MP_TX_PKT_V0));
    BOOST_CHECK(IsTransactionTypeAllowed(0, TOKEN_PROPERTY_TMSC, TOKEN_TYPE_OFFER_ACCEPT_A_BET, MP_TX_PKT_V0));
    // Restore original
    SelectParams(CBaseChainParams::MAIN);
}

BOOST_AUTO_TEST_CASE(update_feature_network)
{
    const std::string& network = Params().NetworkIDString();

    int oldActivationBlock = ConsensusParams(network).TOKEN_BET_BLOCK;
    int newActivationBlock = 123;

    // Before updated
    BOOST_CHECK(oldActivationBlock != newActivationBlock);
    BOOST_CHECK_EQUAL(oldActivationBlock, ConsensusParams().TOKEN_BET_BLOCK);
    BOOST_CHECK(!IsTransactionTypeAllowed(newActivationBlock, TOKEN_PROPERTY_MSC, TOKEN_TYPE_OFFER_ACCEPT_A_BET, MP_TX_PKT_V0));

    // Update
    ConsensusParams(network).TOKEN_BET_BLOCK = newActivationBlock;
    BOOST_CHECK_EQUAL(newActivationBlock, ConsensusParams().TOKEN_BET_BLOCK);
    BOOST_CHECK(IsTransactionTypeAllowed(newActivationBlock, TOKEN_PROPERTY_MSC, TOKEN_TYPE_OFFER_ACCEPT_A_BET, MP_TX_PKT_V0));

    // Restore original
    ConsensusParams(network).TOKEN_BET_BLOCK = oldActivationBlock;
    BOOST_CHECK_EQUAL(oldActivationBlock, ConsensusParams().TOKEN_BET_BLOCK);
}

BOOST_AUTO_TEST_CASE(update_feature)
{
    int oldActivationBlock = ConsensusParams().TOKEN_BET_BLOCK;
    int newActivationBlock = 123;

    // Before updated
    BOOST_CHECK(oldActivationBlock != newActivationBlock);
    BOOST_CHECK(!IsTransactionTypeAllowed(newActivationBlock, TOKEN_PROPERTY_MSC, TOKEN_TYPE_OFFER_ACCEPT_A_BET, MP_TX_PKT_V0));

    // Update
    MutableConsensusParams().TOKEN_BET_BLOCK = newActivationBlock;
    BOOST_CHECK_EQUAL(newActivationBlock, ConsensusParams().TOKEN_BET_BLOCK);
    BOOST_CHECK(IsTransactionTypeAllowed(newActivationBlock, TOKEN_PROPERTY_MSC, TOKEN_TYPE_OFFER_ACCEPT_A_BET, MP_TX_PKT_V0));

    // Restore original
    MutableConsensusParams().TOKEN_BET_BLOCK = oldActivationBlock;
    BOOST_CHECK_EQUAL(oldActivationBlock, ConsensusParams().TOKEN_BET_BLOCK);
}


BOOST_AUTO_TEST_SUITE_END()
