/**
 * @file rpc.cpp
 *
 * This file contains RPC calls for data retrieval.
 */

#include "tokencore/rpc.h"

#include "tokencore/activation.h"
#include "tokencore/consensushash.h"
#include "tokencore/convert.h"
#include "tokencore/dbfees.h"
#include "tokencore/dbspinfo.h"
#include "tokencore/dbstolist.h"
#include "tokencore/dbtradelist.h"
#include "tokencore/dbtxlist.h"
#include "tokencore/dex.h"
#include "tokencore/errors.h"
#include "tokencore/log.h"
#include "tokencore/mdex.h"
#include "tokencore/notifications.h"
#include "tokencore/tokencore.h"
#include "tokencore/parsing.h"
#include "tokencore/rpcrequirements.h"
#include "tokencore/rpctx.h"
#include "tokencore/rpctxobject.h"
#include "tokencore/rpcvalues.h"
#include "tokencore/rules.h"
#include "tokencore/sp.h"
#include "tokencore/sto.h"
#include "tokencore/tally.h"
#include "tokencore/tx.h"
#include "tokencore/utilsbitcoin.h"
#include "tokencore/version.h"
#include "tokencore/walletfetchtxs.h"
#include "tokencore/walletutils.h"

#include "amount.h"
#include "base58.h"
#include "chainparams.h"
#include "init.h"
#include "main.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "rpc/server.h"
#include "tinyformat.h"
#include "txmempool.h"
#include "uint256.h"
#include "utilstrencodings.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <univalue.h>

#include <stdint.h>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>

using std::runtime_error;
using namespace mastercore;

/**
 * Throws a JSONRPCError, depending on error code.
 */
void PopulateFailure(int error)
{
    switch (error) {
        case MP_TX_NOT_FOUND:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
        case MP_TX_UNCONFIRMED:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unconfirmed transactions are not supported");
        case MP_BLOCK_NOT_IN_CHAIN:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not part of the active chain");
        case MP_CROWDSALE_WITHOUT_PROPERTY:
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Potential database corruption: \
                                                  \"Crowdsale Purchase\" without valid property identifier");
        case MP_INVALID_TX_IN_DB_FOUND:
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Potential database corruption: Invalid transaction found");
        case MP_TX_IS_NOT_TOKEN_PROTOCOL:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No RPDx Protocol transaction");
    }
    throw JSONRPCError(RPC_INTERNAL_ERROR, "Generic transaction population failure");
}

void PropertyToJSON(const CMPSPInfo::Entry& sProperty, UniValue& property_obj)
{
    property_obj.pushKV("name", sProperty.name);
    property_obj.pushKV("ticker", sProperty.ticker);
    property_obj.pushKV("category", sProperty.category);
    property_obj.pushKV("subcategory", sProperty.subcategory);
    property_obj.pushKV("data", sProperty.data);
    property_obj.pushKV("url", sProperty.url);
    property_obj.pushKV("divisible", sProperty.isDivisible());
}

void MetaDexObjectToJSON(const CMPMetaDEx& obj, UniValue& metadex_obj)
{
    bool propertyIdForSaleIsDivisible = isPropertyDivisible(obj.getProperty());
    bool propertyIdDesiredIsDivisible = isPropertyDivisible(obj.getDesProperty());
    // add data to JSON object
    metadex_obj.pushKV("address", obj.getAddr());
    metadex_obj.pushKV("txid", obj.getHash().GetHex());
    if (obj.getAction() == 4) metadex_obj.pushKV("ecosystem", isTestEcosystemProperty(obj.getProperty()) ? "test" : "main");
    metadex_obj.pushKV("propertyidforsale", (uint64_t) obj.getProperty());
    metadex_obj.pushKV("propertynameforsale", getPropertyName(obj.getProperty()));

    CMPSPInfo::Entry saleproperty;
    pDbSpInfo->getSP(obj.getProperty(), saleproperty);
    metadex_obj.pushKV("tokenforsalename", saleproperty.name);
    metadex_obj.pushKV("tokenforsale", saleproperty.ticker);

    metadex_obj.pushKV("propertyidforsaleisdivisible", propertyIdForSaleIsDivisible);
    metadex_obj.pushKV("amountforsale", FormatMP(obj.getProperty(), obj.getAmountForSale()));
    metadex_obj.pushKV("amountremaining", FormatMP(obj.getProperty(), obj.getAmountRemaining()));
    metadex_obj.pushKV("propertyiddesired", (uint64_t) obj.getDesProperty());
    metadex_obj.pushKV("propertynamedesired", getPropertyName(obj.getDesProperty()));

    CMPSPInfo::Entry desiredproperty;
    pDbSpInfo->getSP(obj.getDesProperty(), desiredproperty);
    metadex_obj.pushKV("tokendesiredname", desiredproperty.name);
    metadex_obj.pushKV("tokendesired", desiredproperty.ticker);

    metadex_obj.pushKV("propertyiddesiredisdivisible", propertyIdDesiredIsDivisible);
    metadex_obj.pushKV("amountdesired", FormatMP(obj.getDesProperty(), obj.getAmountDesired()));
    metadex_obj.pushKV("amounttofill", FormatMP(obj.getDesProperty(), obj.getAmountToFill()));
    metadex_obj.pushKV("action", (int) obj.getAction());
    metadex_obj.pushKV("block", obj.getBlock());
    metadex_obj.pushKV("blocktime", obj.getBlockTime());
}

void MetaDexObjectsToJSON(std::vector<CMPMetaDEx>& vMetaDexObjs, UniValue& response)
{
    MetaDEx_compare compareByHeight;

    // sorts metadex objects based on block height and position in block
    std::sort (vMetaDexObjs.begin(), vMetaDexObjs.end(), compareByHeight);

    for (std::vector<CMPMetaDEx>::const_iterator it = vMetaDexObjs.begin(); it != vMetaDexObjs.end(); ++it) {
        UniValue metadex_obj(UniValue::VOBJ);
        MetaDexObjectToJSON(*it, metadex_obj);

        response.push_back(metadex_obj);
    }
}

bool BalanceToJSON(const std::string& address, uint32_t property, UniValue& balance_obj, bool divisible)
{
    // confirmed balance minus unconfirmed, spent amounts
    int64_t nAvailable = GetAvailableTokenBalance(address, property);
    int64_t nReserved = GetReservedTokenBalance(address, property);
    int64_t nFrozen = GetFrozenTokenBalance(address, property);

    if (divisible) {
        balance_obj.pushKV("balance", FormatDivisibleMP(nAvailable));
        balance_obj.pushKV("reserved", FormatDivisibleMP(nReserved));
        balance_obj.pushKV("frozen", FormatDivisibleMP(nFrozen));
    } else {
        balance_obj.pushKV("balance", FormatIndivisibleMP(nAvailable));
        balance_obj.pushKV("reserved", FormatIndivisibleMP(nReserved));
        balance_obj.pushKV("frozen", FormatIndivisibleMP(nFrozen));
    }

    return (nAvailable || nReserved || nFrozen);
}

// Obtains details of a fee distribution
static UniValue token_getfeedistribution(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "token_getfeedistribution distributionid\n"
            "\nGet the details for a fee distribution.\n"
            "\nArguments:\n"
            "1. distributionid           (number, required) the distribution to obtain details for\n"
            "\nResult:\n"
            "{\n"
            "  \"distributionid\" : n,          (number) the distribution id\n"
            "  \"propertyid\" : n,              (number) the property id of the distributed tokens\n"
            "  \"block\" : n,                   (number) the block the distribution occurred\n"
            "  \"amount\" : \"n.nnnnnnnn\",     (string) the amount that was distributed\n"
            "  \"recipients\": [                (array of JSON objects) a list of recipients\n"
            "    {\n"
            "      \"address\" : \"address\",          (string) the address of the recipient\n"
            "      \"amount\" : \"n.nnnnnnnn\"         (string) the amount of fees received by the recipient\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("token_getfeedistribution", "1")
            + HelpExampleRpc("token_getfeedistribution", "1")
        );

    int id = request.params[0].get_int();

    int block = 0;
    uint32_t propertyId = 0;
    int64_t total = 0;

    UniValue response(UniValue::VOBJ);

    bool found = pDbFeeHistory->GetDistributionData(id, &propertyId, &block, &total);
    if (!found) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Fee distribution ID does not exist");
    }
    response.pushKV("distributionid", id);
    response.pushKV("propertyid", (uint64_t)propertyId);
    response.pushKV("block", block);
    response.pushKV("amount", FormatMP(propertyId, total));
    UniValue recipients(UniValue::VARR);
    std::set<std::pair<std::string,int64_t> > sRecipients = pDbFeeHistory->GetFeeDistribution(id);
    bool divisible = isPropertyDivisible(propertyId);
    if (!sRecipients.empty()) {
        for (std::set<std::pair<std::string,int64_t> >::iterator it = sRecipients.begin(); it != sRecipients.end(); it++) {
            std::string address = (*it).first;
            int64_t amount = (*it).second;
            UniValue recipient(UniValue::VOBJ);
            recipient.pushKV("address", address);
            if (divisible) {
                recipient.pushKV("amount", FormatDivisibleMP(amount));
            } else {
                recipient.pushKV("amount", FormatIndivisibleMP(amount));
            }
            recipients.push_back(recipient);
        }
    }
    response.pushKV("recipients", recipients);
    return response;
}

// Obtains all fee distributions for a property
// TODO : Split off code to populate a fee distribution object into a seperate function
static UniValue token_getfeedistributions(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "token_getfeedistributions propertyid\n"
            "\nGet the details of all fee distributions for a property.\n"
            "\nArguments:\n"
            "1. propertyid           (number, required) the property id to retrieve distributions for\n"
            "\nResult:\n"
            "[                       (array of JSON objects)\n"
            "  {\n"
            "    \"distributionid\" : n,          (number) the distribution id\n"
            "    \"propertyid\" : n,              (number) the property id of the distributed tokens\n"
            "    \"block\" : n,                   (number) the block the distribution occurred\n"
            "    \"amount\" : \"n.nnnnnnnn\",     (string) the amount that was distributed\n"
            "    \"recipients\": [                (array of JSON objects) a list of recipients\n"
            "      {\n"
            "        \"address\" : \"address\",          (string) the address of the recipient\n"
            "        \"amount\" : \"n.nnnnnnnn\"         (string) the amount of fees received by the recipient\n"
            "      },\n"
            "      ...\n"
            "  }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("token_getfeedistributions", "1")
            + HelpExampleRpc("token_getfeedistributions", "1")
        );

    uint32_t prop = ParsePropertyId(request.params[0]);
    RequireExistingProperty(prop);

    UniValue response(UniValue::VARR);

    std::set<int> sDistributions = pDbFeeHistory->GetDistributionsForProperty(prop);
    if (!sDistributions.empty()) {
        for (std::set<int>::iterator it = sDistributions.begin(); it != sDistributions.end(); it++) {
            int id = *it;
            int block = 0;
            uint32_t propertyId = 0;
            int64_t total = 0;
            UniValue responseObj(UniValue::VOBJ);
            bool found = pDbFeeHistory->GetDistributionData(id, &propertyId, &block, &total);
            if (!found) {
                PrintToLog("Fee History Error - Distribution data not found for distribution ID %d but it was included in GetDistributionsForProperty(prop %d)\n", id, prop);
                continue;
            }
            responseObj.pushKV("distributionid", id);
            responseObj.pushKV("propertyid", (uint64_t)propertyId);
            responseObj.pushKV("block", block);
            responseObj.pushKV("amount", FormatMP(propertyId, total));
            UniValue recipients(UniValue::VARR);
            std::set<std::pair<std::string,int64_t> > sRecipients = pDbFeeHistory->GetFeeDistribution(id);
            bool divisible = isPropertyDivisible(propertyId);
            if (!sRecipients.empty()) {
                for (std::set<std::pair<std::string,int64_t> >::iterator it = sRecipients.begin(); it != sRecipients.end(); it++) {
                    std::string address = (*it).first;
                    int64_t amount = (*it).second;
                    UniValue recipient(UniValue::VOBJ);
                    recipient.pushKV("address", address);
                    if (divisible) {
                        recipient.pushKV("amount", FormatDivisibleMP(amount));
                    } else {
                        recipient.pushKV("amount", FormatIndivisibleMP(amount));
                    }
                    recipients.push_back(recipient);
                }
            }
            responseObj.pushKV("recipients", recipients);
            response.push_back(responseObj);
        }
    }
    return response;
}

// Obtains the trigger value for fee distribution for a/all properties
static UniValue token_getfeetrigger(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            "token_getfeetrigger ( propertyid )\n"
            "\nReturns the amount of fees required in the cache to trigger distribution.\n"
            "\nArguments:\n"
            "1. propertyid           (number, optional) filter the results on this property id\n"
            "\nResult:\n"
            "[                       (array of JSON objects)\n"
            "  {\n"
            "    \"propertyid\" : nnnnnnn,          (number) the property id\n"
            "    \"feetrigger\" : \"n.nnnnnnnn\",   (string) the amount of fees required to trigger distribution\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("token_getfeetrigger", "3")
            + HelpExampleRpc("token_getfeetrigger", "3")
        );

    uint32_t propertyId = 0;
    if (0 < request.params.size()) {
        propertyId = ParsePropertyId(request.params[0]);
    }

    if (propertyId > 0) {
        RequireExistingProperty(propertyId);
    }

    UniValue response(UniValue::VARR);

    for (uint8_t ecosystem = 1; ecosystem <= 2; ecosystem++) {
        uint32_t startPropertyId = (ecosystem == 1) ? 1 : TEST_ECO_PROPERTY_1;
        for (uint32_t itPropertyId = startPropertyId; itPropertyId < pDbSpInfo->peekNextSPID(ecosystem); itPropertyId++) {
            if (propertyId == 0 || propertyId == itPropertyId) {
                int64_t feeTrigger = pDbFeeCache->GetDistributionThreshold(itPropertyId);
                std::string strFeeTrigger = FormatMP(itPropertyId, feeTrigger);
                UniValue cacheObj(UniValue::VOBJ);
                cacheObj.pushKV("propertyid", (uint64_t)itPropertyId);
                cacheObj.pushKV("feetrigger", strFeeTrigger);
                response.push_back(cacheObj);
            }
        }
    }

    return response;
}

// Provides the fee share the wallet (or specific address) will receive from fee distributions
static UniValue token_getfeeshare(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw runtime_error(
            "token_getfeeshare ( address ecosystem )\n"
            "\nReturns the percentage share of fees distribution applied to the wallet (default) or address (if supplied).\n"
            "\nArguments:\n"
            "1. address              (string, optional) retrieve the fee share for the supplied address\n"
            "2. ecosystem            (number, optional) the ecosystem to check the fee share (1 for main ecosystem, 2 for test ecosystem)\n"
            "\nResult:\n"
            "[                       (array of JSON objects)\n"
            "  {\n"
            "    \"address\" : nnnnnnn,          (number) the property id\n"
            "    \"feeshare\" : \"n.nnnnnnnn\",   (string) the percentage of fees this address will receive based on current state\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("token_getfeeshare", "\"RiGQ12CCidStpSKmjdJMfzc2uE9JFD7epe\" 1")
            + HelpExampleRpc("token_getfeeshare", "\"RiGQ12CCidStpSKmjdJMfzc2uE9JFD7epe\", 1")
        );

    std::string address;
    uint8_t ecosystem = 1;
    if (0 < request.params.size()) {
        if ("*" != request.params[0].get_str()) { //ParseAddressOrEmpty doesn't take wildcards
            address = ParseAddressOrEmpty(request.params[0]);
        } else {
            address = "*";
        }
    }

    if (1 < request.params.size()) {
        ecosystem = ParseEcosystem(request.params[1]);
    }

    UniValue response(UniValue::VARR);
    bool addObj = false;

    OwnerAddrType receiversSet;
    if (ecosystem == 1) {
        receiversSet = STO_GetReceivers("FEEDISTRIBUTION", TOKEN_PROPERTY_MSC, COIN);
    } else {
        receiversSet = STO_GetReceivers("FEEDISTRIBUTION", TOKEN_PROPERTY_TMSC, COIN);
    }

    for (OwnerAddrType::reverse_iterator it = receiversSet.rbegin(); it != receiversSet.rend(); ++it) {
        addObj = false;
        if (address.empty()) {
            if (IsMyAddress(it->second)) {
                addObj = true;
            }
        } else if (address == it->second || address == "*") {
            addObj = true;
        }
        if (addObj) {
            UniValue feeShareObj(UniValue::VOBJ);
            // NOTE: using float here as this is a display value only which isn't an exact percentage and
            //       changes block to block (due to dev Token) so high precision not required(?)
            double feeShare = (double(it->first) / double(COIN)) * (double)100;
            std::string strFeeShare = strprintf("%.4f", feeShare);
            strFeeShare += "%";
            feeShareObj.pushKV("address", it->second);
            feeShareObj.pushKV("feeshare", strFeeShare);
            response.push_back(feeShareObj);
        }
    }

    return response;
}

// Provides the current values of the fee cache
static UniValue token_getfeecache(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            "token_getfeecache ( propertyid )\n"
            "\nReturns the amount of fees cached for distribution.\n"
            "\nArguments:\n"
            "1. propertyid           (number, optional) filter the results on this property id\n"
            "\nResult:\n"
            "[                       (array of JSON objects)\n"
            "  {\n"
            "    \"propertyid\" : nnnnnnn,          (number) the property id\n"
            "    \"cachedfees\" : \"n.nnnnnnnn\",   (string) the amount of fees cached for this property\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("token_getfeecache", "31")
            + HelpExampleRpc("token_getfeecache", "31")
        );

    uint32_t propertyId = 0;
    if (0 < request.params.size()) {
        propertyId = ParsePropertyId(request.params[0]);
    }

    if (propertyId > 0) {
        RequireExistingProperty(propertyId);
    }

    UniValue response(UniValue::VARR);

    for (uint8_t ecosystem = 1; ecosystem <= 2; ecosystem++) {
        uint32_t startPropertyId = (ecosystem == 1) ? 1 : TEST_ECO_PROPERTY_1;
        for (uint32_t itPropertyId = startPropertyId; itPropertyId < pDbSpInfo->peekNextSPID(ecosystem); itPropertyId++) {
            if (propertyId == 0 || propertyId == itPropertyId) {
                int64_t cachedFee = pDbFeeCache->GetCachedAmount(itPropertyId);
                if (cachedFee == 0) {
                    // filter empty results unless the call specifically requested this property
                    if (propertyId != itPropertyId) continue;
                }
                std::string strFee = FormatMP(itPropertyId, cachedFee);
                UniValue cacheObj(UniValue::VOBJ);
                cacheObj.pushKV("propertyid", (uint64_t)itPropertyId);
                cacheObj.pushKV("cachedfees", strFee);
                response.push_back(cacheObj);
            }
        }
    }

    return response;
}

// generate a list of seed blocks based on the data in LevelDB
static UniValue gettokenseedblocks(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "token_getseedblocks startblock endblock\n"
            "\nReturns a list of blocks containing Token transactions for use in seed block filtering.\n"
            "\nArguments:\n"
            "1. startblock           (number, required) the first block to look for Token transactions (inclusive)\n"
            "2. endblock             (number, required) the last block to look for Token transactions (inclusive)\n"
            "\nResult:\n"
            "[                     (array of numbers) a list of seed blocks\n"
            "   nnnnnn,              (number) the block height of the seed block\n"
            "   ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokenseedblocks", "290000 300000")
            + HelpExampleRpc("gettokenseedblocks", "290000, 300000")
        );

    int startHeight = request.params[0].get_int();
    int endHeight = request.params[1].get_int();

    RequireHeightInChain(startHeight);
    RequireHeightInChain(endHeight);

    UniValue response(UniValue::VARR);

    {
        LOCK(cs_tally);
        std::set<int> setSeedBlocks = pDbTransactionList->GetSeedBlocks(startHeight, endHeight);
        for (std::set<int>::const_iterator it = setSeedBlocks.begin(); it != setSeedBlocks.end(); ++it) {
            response.push_back(*it);
        }
    }

    return response;
}

// obtain the payload for a transaction
static UniValue gettokenpayload(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "gettokenpayload \"txid\"\n"
            "\nGet the payload for an Token transaction.\n"
            "\nArguments:\n"
            "1. txid                 (string, required) the hash of the transaction to retrieve payload\n"
            "\nResult:\n"
            "{\n"
            "  \"payload\" : \"payloadmessage\",       (string) the decoded Token payload message\n"
            "  \"payloadsize\" : n                     (number) the size of the payload\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokenpayload", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleRpc("gettokenpayload", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    uint256 txid = ParseHashV(request.params[0], "txid");

    CTransaction tx;
    uint256 blockHash;
    if (!GetTransaction(txid, tx, blockHash)) {
        PopulateFailure(MP_TX_NOT_FOUND);
    }

    int blockTime = 0;
    int blockHeight = GetHeight();
    if (!blockHash.IsNull()) {
        CBlockIndex* pBlockIndex = GetBlockIndex(blockHash);
        if (nullptr != pBlockIndex) {
            blockTime = pBlockIndex->nTime;
            blockHeight = pBlockIndex->nHeight;
        }
    }

    CMPTransaction mp_obj;
    int parseRC = ParseTransaction(tx, blockHeight, 0, mp_obj, blockTime);
    if (parseRC < 0) PopulateFailure(MP_TX_IS_NOT_TOKEN_PROTOCOL);

    UniValue payloadObj(UniValue::VOBJ);
    payloadObj.pushKV("payload", mp_obj.getPayload());
    payloadObj.pushKV("payloadsize", mp_obj.getPayloadSize());
    return payloadObj;
}

// determine whether to automatically commit transactions
static UniValue token_setautocommit(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "token_setautocommit flag\n"
            "\nSets the global flag that determines whether transactions are automatically committed and broadcast.\n"
            "\nArguments:\n"
            "1. flag                 (boolean, required) the flag\n"
            "\nResult:\n"
            "true|false              (boolean) the updated flag status\n"
            "\nExamples:\n"
            + HelpExampleCli("token_setautocommit", "false")
            + HelpExampleRpc("token_setautocommit", "false")
        );

    LOCK(cs_tally);

    autoCommit = request.params[0].get_bool();
    return autoCommit;
}

// display an MP balance via RPC
static UniValue gettokenbalance(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "gettokenbalance \"address\" ticker\n"
            "\nReturns the token balance for a given address and token.\n"
            "\nArguments:\n"
            "1. address              (string, required) the address\n"
            "2. ticker               (string, required) the token ticker\n"
            "\nResult:\n"
            "{\n"
            "  \"balance\" : \"n.nnnnnnnn\",   (string) the available balance of the address\n"
            "  \"reserved\" : \"n.nnnnnnnn\"   (string) the amount reserved by sell offers and accepts\n"
            "  \"frozen\" : \"n.nnnnnnnn\"     (string) the amount frozen by the issuer (applies to managed properties only)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokenbalance", "\"RiGQ12CCidStpSKmjdJMfzc2uE9JFD7epe\" TOKEN")
            + HelpExampleRpc("gettokenbalance", "\"RiGQ12CCidStpSKmjdJMfzc2uE9JFD7epe\", TOKEN")
        );

    std::string address = ParseAddress(request.params[0]);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(request.params[1].get_str());
    if (!propertyId)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token not found");

    RequireExistingProperty(propertyId);

    UniValue balanceObj(UniValue::VOBJ);
    BalanceToJSON(address, propertyId, balanceObj, isPropertyDivisible(propertyId));

    return balanceObj;
}

static UniValue getalltokenbalances(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "getalltokenbalances ticker\n"
            "\nReturns a list of token balances for a given token identified by ticker.\n"
            "\nArguments:\n"
            "1. ticker           (string, required) the token ticker\n"
            "\nResult:\n"
            "[                           (array of JSON objects)\n"
            "  {\n"
            "    \"address\" : \"address\",      (string) the address\n"
            "    \"balance\" : \"n.nnnnnnnn\",   (string) the available balance of the address\n"
            "    \"reserved\" : \"n.nnnnnnnn\"   (string) the amount reserved by sell offers and accepts\n"
            "    \"frozen\" : \"n.nnnnnnnn\"     (string) the amount frozen by the issuer (applies to managed properties only)\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getalltokenbalances", "TOKEN")
            + HelpExampleRpc("getalltokenbalances", "TOKEN")
        );

    uint32_t propertyId = pDbSpInfo->findSPByTicker(request.params[0].get_str());
    if (!propertyId)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token not found");

    RequireExistingProperty(propertyId);

    UniValue response(UniValue::VARR);
    bool isDivisible = isPropertyDivisible(propertyId); // we want to check this BEFORE the loop

    LOCK(cs_tally);

    for (std::unordered_map<std::string, CMPTally>::iterator it = mp_tally_map.begin(); it != mp_tally_map.end(); ++it) {
        uint32_t id = 0;
        bool includeAddress = false;
        std::string address = it->first;
        (it->second).init();
        while (0 != (id = (it->second).next())) {
            if (id == propertyId) {
                includeAddress = true;
                break;
            }
        }
        if (!includeAddress) {
            continue; // ignore this address, has never transacted in this propertyId
        }
        UniValue balanceObj(UniValue::VOBJ);
        balanceObj.pushKV("address", address);
        bool nonEmptyBalance = BalanceToJSON(address, propertyId, balanceObj, isDivisible);

        if (nonEmptyBalance) {
            response.push_back(balanceObj);
        }
    }

    return response;
}

static UniValue getalltokenbalancesforaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "getalltokenbalancesforaddress \"address\"\n"
            "\nReturns a list of all token balances for a given address.\n"
            "\nArguments:\n"
            "1. address              (string, required) the address\n"
            "\nResult:\n"
            "[                           (array of JSON objects)\n"
            "  {\n"
            "    \"propertyid\" : n,           (number) the property identifier\n"
            "    \"name\" : \"name\",            (string) the name of the property\n"
            "    \"balance\" : \"n.nnnnnnnn\",   (string) the available balance of the address\n"
            "    \"reserved\" : \"n.nnnnnnnn\"   (string) the amount reserved by sell offers and accepts\n"
            "    \"frozen\" : \"n.nnnnnnnn\"     (string) the amount frozen by the issuer (applies to managed properties only)\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getalltokenbalancesforaddress", "\"RiGQ12CCidStpSKmjdJMfzc2uE9JFD7epe\"")
            + HelpExampleRpc("getalltokenbalancesforaddress", "\"RiGQ12CCidStpSKmjdJMfzc2uE9JFD7epe\"")
        );

    std::string address = ParseAddress(request.params[0]);

    UniValue response(UniValue::VARR);

    LOCK(cs_tally);

    CMPTally* addressTally = getTally(address);

    if (nullptr == addressTally) { // addressTally object does not exist
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Address not found");
    }

    addressTally->init();

    uint32_t propertyId = 0;
    while (0 != (propertyId = addressTally->next())) {
        CMPSPInfo::Entry property;
        if (!pDbSpInfo->getSP(propertyId, property)) {
            continue;
        }

        UniValue balanceObj(UniValue::VOBJ);
        balanceObj.pushKV("propertyid", (uint64_t) propertyId);
        balanceObj.pushKV("name", property.name);
        balanceObj.pushKV("ticker", property.ticker);

        bool nonEmptyBalance = BalanceToJSON(address, propertyId, balanceObj, property.isDivisible());

        if (nonEmptyBalance) {
            response.push_back(balanceObj);
        }
    }

    return response;
}

/** Returns all addresses that may be mine. */
static std::set<std::string> getWalletAddresses(const JSONRPCRequest& request, bool fIncludeWatchOnly)
{
    std::set<std::string> result;

#ifdef ENABLE_WALLET
    LOCK(pwalletMain->cs_wallet);

    for(const auto& item : pwalletMain->mapAddressBook) {
        const CTxDestination& address = item.first;
        isminetype iIsMine = IsMine(*pwalletMain, address);

        if (iIsMine == ISMINE_SPENDABLE || (fIncludeWatchOnly && iIsMine != ISMINE_NO)) {
            result.insert(EncodeDestination(address));
        }
    }
#endif

    return result;
}

static UniValue getwallettokenbalances(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            "getwallettokenbalances ( includewatchonly )\n"
            "\nReturns a list of the total token balances of the whole wallet.\n"
            "\nArguments:\n"
            "1. includewatchonly     (boolean, optional) include balances of watchonly addresses (default: false)\n"
            "\nResult:\n"
            "[                           (array of JSON objects)\n"
            "  {\n"
            "    \"propertyid\" : n,         (number) the property identifier\n"
            "    \"name\" : \"name\",            (string) the name of the token\n"
            "    \"balance\" : \"n.nnnnnnnn\",   (string) the total available balance for the token\n"
            "    \"reserved\" : \"n.nnnnnnnn\"   (string) the total amount reserved by sell offers and accepts\n"
            "    \"frozen\" : \"n.nnnnnnnn\"     (string) the total amount frozen by the issuer (applies to managed properties only)\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getwallettokenbalances", "")
            + HelpExampleRpc("getwallettokenbalances", "")
        );

    bool fIncludeWatchOnly = false;
    if (request.params.size() > 0) {
        fIncludeWatchOnly = request.params[0].get_bool();
    }

    UniValue response(UniValue::VARR);

#ifdef ENABLE_WALLET
    if (!pwalletMain) {
        return response;
    }

    std::set<std::string> addresses = getWalletAddresses(request, fIncludeWatchOnly);
    std::map<uint32_t, std::tuple<int64_t, int64_t, int64_t>> balances;

    LOCK(cs_tally);
    for (const std::string& address : addresses) {
        CMPTally* addressTally = getTally(address);
        if (nullptr == addressTally) {
            continue; // address doesn't have tokens
        }

        uint32_t propertyId = 0;
        addressTally->init();

        while (0 != (propertyId = addressTally->next())) {
            int64_t nAvailable = GetAvailableTokenBalance(address, propertyId);
            int64_t nReserved = GetReservedTokenBalance(address, propertyId);
            int64_t nFrozen = GetFrozenTokenBalance(address, propertyId);

            if (!nAvailable && !nReserved && !nFrozen) {
                continue;
            }

            std::tuple<int64_t, int64_t, int64_t> currentBalance{0, 0, 0};
            if (balances.find(propertyId) != balances.end()) {
                currentBalance = balances[propertyId];
            }

            currentBalance = std::make_tuple(
                    std::get<0>(currentBalance) + nAvailable,
                    std::get<1>(currentBalance) + nReserved,
                    std::get<2>(currentBalance) + nFrozen
            );

            balances[propertyId] = currentBalance;
        }
    }

    for (auto const& item : balances) {
        uint32_t propertyId = item.first;
        std::tuple<int64_t, int64_t, int64_t> balance = item.second;

        CMPSPInfo::Entry property;
        if (!pDbSpInfo->getSP(propertyId, property)) {
            continue; // token wasn't found in the DB
        }

        int64_t nAvailable = std::get<0>(balance);
        int64_t nReserved = std::get<1>(balance);
        int64_t nFrozen = std::get<2>(balance);

        UniValue objBalance(UniValue::VOBJ);
        objBalance.pushKV("propertyid", (uint64_t) propertyId);
        objBalance.pushKV("name", property.name);
        objBalance.pushKV("ticker", property.ticker);

        if (property.isDivisible()) {
            objBalance.pushKV("balance", FormatDivisibleMP(nAvailable));
            objBalance.pushKV("reserved", FormatDivisibleMP(nReserved));
            objBalance.pushKV("frozen", FormatDivisibleMP(nFrozen));
        } else {
            objBalance.pushKV("balance", FormatIndivisibleMP(nAvailable));
            objBalance.pushKV("reserved", FormatIndivisibleMP(nReserved));
            objBalance.pushKV("frozen", FormatIndivisibleMP(nFrozen));
        }

        response.push_back(objBalance);
    }
#endif

    return response;
}

static UniValue gettokenbalances(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            "gettokenbalances\n"
            "\nReturns a list of the total token balances.\n"
            "\nResult:\n"
            "[                           (array of JSON objects)\n"
            "  {\n"
            "    \"propertyid\" : n,         (number) the property identifier\n"
            "    \"name\" : \"name\",            (string) the name of the token\n"
            "    \"balance\" : \"n.nnnnnnnn\",   (string) the total available balance for the token\n"
            "    \"reserved\" : \"n.nnnnnnnn\"   (string) the total amount reserved by sell offers and accepts\n"
            "    \"frozen\" : \"n.nnnnnnnn\"     (string) the total amount frozen by the issuer (applies to managed properties only)\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokenbalances", "")
            + HelpExampleRpc("gettokenbalances", "")
        );

    UniValue response(UniValue::VARR);

#ifdef ENABLE_WALLET
    if (!pwalletMain) {
        return response;
    }

    for (std::set<uint32_t>::iterator it = global_wallet_property_list.begin() ; it != global_wallet_property_list.end(); ++it) {
        uint32_t propertyId = *it;
        bool addToken = false;

        CMPSPInfo::Entry property;
        if (!pDbSpInfo->getSP(propertyId, property)) {
            continue; // token wasn't found in the DB
        }

        int64_t available = global_balance_money[propertyId];
        int64_t reserved = global_balance_reserved[propertyId];
        int64_t frozen = global_balance_frozen[propertyId];

        UniValue objBalance(UniValue::VOBJ);
        objBalance.pushKV("propertyid", (uint64_t) propertyId);
        objBalance.pushKV("name", property.name);
        objBalance.pushKV("ticker", property.ticker);

        if (property.isDivisible()) {
            objBalance.pushKV("balance", FormatDivisibleMP(available));
            objBalance.pushKV("reserved", FormatDivisibleMP(reserved));
            objBalance.pushKV("frozen", FormatDivisibleMP(frozen));
        } else {
            objBalance.pushKV("balance", FormatIndivisibleMP(available));
            objBalance.pushKV("reserved", FormatIndivisibleMP(reserved));
            objBalance.pushKV("frozen", FormatIndivisibleMP(frozen));
        }

        UniValue addresses(UniValue::VARR);

        for (std::string address : global_token_addresses[propertyId]) {
            UniValue objAddrBalance(UniValue::VOBJ);

            objAddrBalance.pushKV("address", address);

            uint64_t addr_balance = 0;
            uint64_t addr_reserved = 0;
            uint64_t addr_frozen = 0;

            addr_balance += GetAvailableTokenBalance(address, propertyId);
            addr_reserved += GetTokenBalance(address, propertyId, SELLOFFER_RESERVE);
            addr_reserved += GetTokenBalance(address, propertyId, METADEX_RESERVE);
            addr_reserved += GetTokenBalance(address, propertyId, ACCEPT_RESERVE);
            addr_frozen += GetFrozenTokenBalance(address, propertyId);
            
            if (property.isDivisible()) {
                objAddrBalance.pushKV("balance", FormatDivisibleMP(addr_balance));
                objAddrBalance.pushKV("reserved", FormatDivisibleMP(addr_reserved));
                objAddrBalance.pushKV("frozen", FormatDivisibleMP(addr_frozen));
            } else {
                objAddrBalance.pushKV("balance", FormatIndivisibleMP(addr_balance));
                objAddrBalance.pushKV("reserved", FormatIndivisibleMP(addr_reserved));
                objAddrBalance.pushKV("frozen", FormatIndivisibleMP(addr_frozen));
            }

            addresses.push_back(objAddrBalance);

            if (addr_balance > 0 || addr_reserved > 0 || addr_frozen)
                addToken = true;
        }

        objBalance.pushKV("addresses", addresses);

        if (addToken)
            response.push_back(objBalance);
    }

#endif

    return response;
}

static UniValue getwalletaddresstokenbalances(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            "getwalletaddresstokenbalances ( includewatchonly )\n"
            "\nReturns a list of all token balances for every wallet address.\n"
            "\nArguments:\n"
            "1. includewatchonly     (boolean, optional) include balances of watchonly addresses (default: false)\n"
            "\nResult:\n"
            "[                           (array of JSON objects)\n"
            "  {\n"
            "    \"address\" : \"address\",      (string) the address linked to the following balances\n"
            "    \"balances\" :\n"
            "    [\n"
            "      {\n"
            "        \"propertyid\" : n,         (number) the property identifier\n"
            "        \"name\" : \"name\",            (string) the name of the token\n"
            "        \"balance\" : \"n.nnnnnnnn\",   (string) the available balance for the token\n"
            "        \"reserved\" : \"n.nnnnnnnn\"   (string) the amount reserved by sell offers and accepts\n"
            "        \"frozen\" : \"n.nnnnnnnn\"     (string) the amount frozen by the issuer (applies to managed properties only)\n"
            "      },\n"
            "      ...\n"
            "    ]\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getwalletaddresstokenbalances", "")
            + HelpExampleRpc("getwalletaddresstokenbalances", "")
        );

    bool fIncludeWatchOnly = false;
    if (request.params.size() > 0) {
        fIncludeWatchOnly = request.params[0].get_bool();
    }

    UniValue response(UniValue::VARR);

#ifdef ENABLE_WALLET
    if (!pwalletMain) {
        return response;
    }

    std::set<std::string> addresses = getWalletAddresses(request, fIncludeWatchOnly);

    LOCK(cs_tally);
    for (const std::string& address : addresses) {
        CMPTally* addressTally = getTally(address);
        if (nullptr == addressTally) {
            continue; // address doesn't have tokens
        }

        UniValue arrBalances(UniValue::VARR);
        uint32_t propertyId = 0;
        addressTally->init();

        while (0 != (propertyId = addressTally->next())) {
            CMPSPInfo::Entry property;
            if (!pDbSpInfo->getSP(propertyId, property)) {
                continue; // token wasn't found in the DB
            }

            UniValue objBalance(UniValue::VOBJ);
            objBalance.pushKV("propertyid", (uint64_t) propertyId);
            objBalance.pushKV("name", property.name);
            objBalance.pushKV("ticker", property.ticker);

            bool nonEmptyBalance = BalanceToJSON(address, propertyId, objBalance, property.isDivisible());

            if (nonEmptyBalance) {
                arrBalances.push_back(objBalance);
            }
        }

        if (arrBalances.size() > 0) {
            UniValue objEntry(UniValue::VOBJ);
            objEntry.pushKV("address", address);
            objEntry.pushKV("balances", arrBalances);
            response.push_back(objEntry);
        }
    }
#endif

    return response;
}

static UniValue gettoken(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "gettoken ticker\n"
            "\nReturns details for about the tokens or smart property to lookup.\n"
            "\nArguments:\n"
            "1. ticker         (string, required) the ticker of the token\n"
            "\nResult:\n"
            "{\n"
            "  \"propertyid\" : n,                (number) the identifier\n"
            "  \"name\" : \"name\",                 (string) the name of the tokens\n"
            "  \"category\" : \"category\",         (string) the category used for the tokens\n"
            "  \"subcategory\" : \"subcategory\",   (string) the subcategory used for the tokens\n"
            "  \"data\" : \"information\",          (string) additional information or a description\n"
            "  \"url\" : \"uri\",                   (string) an URI, for example pointing to a website\n"
            "  \"divisible\" : true|false,        (boolean) whether the tokens are divisible\n"
            "  \"issuer\" : \"address\",            (string) the Bitcoin address of the issuer on record\n"
            "  \"creationtxid\" : \"hash\",         (string) the hex-encoded creation transaction hash\n"
            "  \"fixedissuance\" : true|false,    (boolean) whether the token supply is fixed\n"
            "  \"managedissuance\" : true|false,    (boolean) whether the token supply is managed\n"
            "  \"totaltokens\" : \"n.nnnnnnnn\"     (string) the total number of tokens in existence\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gettoken", "TOKEN")
            + HelpExampleRpc("gettoken", "TOKEN")
        );

    uint32_t propertyId = pDbSpInfo->findSPByTicker(request.params[0].get_str());
    if (!propertyId)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token not found");

    RequireExistingProperty(propertyId);

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!pDbSpInfo->getSP(propertyId, sp)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
        }
    }
    int64_t nTotalTokens = getTotalTokens(propertyId);
    std::string strCreationHash = sp.txid.GetHex();
    std::string strTotalTokens = FormatMP(propertyId, nTotalTokens);

    UniValue response(UniValue::VOBJ);
    response.pushKV("propertyid", (uint64_t) propertyId);
    PropertyToJSON(sp, response); // name, category, subcategory, data, url, divisible
    response.pushKV("issuer", sp.issuer);
    response.pushKV("creationtxid", strCreationHash);
    response.pushKV("fixedissuance", sp.fixed);
    response.pushKV("managedissuance", sp.manual);
    if (sp.manual) {
        int currentBlock = GetHeight();
        LOCK(cs_tally);
        response.pushKV("freezingenabled", isFreezingEnabled(propertyId, currentBlock));
    }
    response.pushKV("totaltokens", strTotalTokens);

    return response;
}

static UniValue listtokens(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "listtokens\n"
            "\nLists all tokens or smart properties.\n"
            "\nResult:\n"
            "[                                (array of JSON objects)\n"
            "  {\n"
            "    \"propertyid\" : n,                (number) the identifier of the tokens\n"
            "    \"name\" : \"name\",                 (string) the name of the tokens\n"
            "    \"category\" : \"category\",         (string) the category used for the tokens\n"
            "    \"subcategory\" : \"subcategory\",   (string) the subcategory used for the tokens\n"
            "    \"data\" : \"information\",          (string) additional information or a description\n"
            "    \"url\" : \"uri\",                   (string) an URI, for example pointing to a website\n"
            "    \"divisible\" : true|false         (boolean) whether the tokens are divisible\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listtokens", "")
            + HelpExampleRpc("listtokens", "")
        );

    UniValue response(UniValue::VARR);

    LOCK(cs_tally);

    uint32_t nextSPID = pDbSpInfo->peekNextSPID(1);
    for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++) {
        CMPSPInfo::Entry sp;
        if (pDbSpInfo->getSP(propertyId, sp)) {
            UniValue propertyObj(UniValue::VOBJ);
            propertyObj.pushKV("propertyid", (uint64_t) propertyId);
            PropertyToJSON(sp, propertyObj); // name, category, subcategory, data, url, divisible

            response.push_back(propertyObj);
        }
    }

    uint32_t nextTestSPID = pDbSpInfo->peekNextSPID(2);
    for (uint32_t propertyId = TEST_ECO_PROPERTY_1; propertyId < nextTestSPID; propertyId++) {
        CMPSPInfo::Entry sp;
        if (pDbSpInfo->getSP(propertyId, sp)) {
            UniValue propertyObj(UniValue::VOBJ);
            propertyObj.pushKV("propertyid", (uint64_t) propertyId);
            PropertyToJSON(sp, propertyObj); // name, category, subcategory, data, url, divisible

            response.push_back(propertyObj);
        }
    }

    return response;
}

static UniValue gettokencrowdsale(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw runtime_error(
            "gettokencrowdsale ticker ( verbose )\n"
            "\nReturns information about a crowdsale.\n"
            "\nArguments:\n"
            "1. ticker               (string, required) the token ticker of the crowdsale\n"
            "2. verbose              (boolean, optional) list crowdsale participants (default: false)\n"
            "\nResult:\n"
            "{\n"
            "  \"propertyid\" : n,                     (number) the identifier of the crowdsale\n"
            "  \"name\" : \"name\",                      (string) the name of the tokens issued via the crowdsale\n"
            "  \"active\" : true|false,                (boolean) whether the crowdsale is still active\n"
            "  \"issuer\" : \"address\",                 (string) the Bitcoin address of the issuer on record\n"
            "  \"propertyiddesired\" : n,              (number) the identifier of the tokens eligible to participate in the crowdsale\n"
            "  \"tokensperunit\" : \"n.nnnnnnnn\",       (string) the amount of tokens granted per unit invested in the crowdsale\n"
            "  \"earlybonus\" : n,                     (number) an early bird bonus for participants in percent per week\n"
            "  \"percenttoissuer\" : n,                (number) a percentage of tokens that will be granted to the issuer\n"
            "  \"starttime\" : nnnnnnnnnn,             (number) the start time of the of the crowdsale as Unix timestamp\n"
            "  \"deadline\" : nnnnnnnnnn,              (number) the deadline of the crowdsale as Unix timestamp\n"
            "  \"amountraised\" : \"n.nnnnnnnn\",        (string) the amount of tokens invested by participants\n"
            "  \"tokensissued\" : \"n.nnnnnnnn\",        (string) the total number of tokens issued via the crowdsale\n"
            "  \"issuerbonustokens\" : \"n.nnnnnnnn\",   (string) the amount of tokens granted to the issuer as bonus\n"
            "  \"addedissuertokens\" : \"n.nnnnnnnn\",   (string) the amount of issuer bonus tokens not yet emitted\n"
            "  \"closedearly\" : true|false,           (boolean) whether the crowdsale ended early (if not active)\n"
            "  \"maxtokens\" : true|false,             (boolean) whether the crowdsale ended early due to reaching the limit of max. issuable tokens (if not active)\n"
            "  \"endedtime\" : nnnnnnnnnn,             (number) the time when the crowdsale ended (if closed early)\n"
            "  \"closetx\" : \"hash\",                   (string) the hex-encoded hash of the transaction that closed the crowdsale (if closed manually)\n"
            "  \"participanttransactions\": [          (array of JSON objects) a list of crowdsale participations (if verbose=true)\n"
            "    {\n"
            "      \"txid\" : \"hash\",                      (string) the hex-encoded hash of participation transaction\n"
            "      \"amountsent\" : \"n.nnnnnnnn\",          (string) the amount of tokens invested by the participant\n"
            "      \"participanttokens\" : \"n.nnnnnnnn\",   (string) the tokens granted to the participant\n"
            "      \"issuertokens\" : \"n.nnnnnnnn\"         (string) the tokens granted to the issuer as bonus\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokencrowdsale", "TOKEN true")
            + HelpExampleRpc("gettokencrowdsale", "TOKEN, true")
        );

    std::string ticker = ParseText(request.params[0]);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(ticker);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

    bool showVerbose = (request.params.size() > 1) ? request.params[1].get_bool() : false;

    RequireExistingProperty(propertyId);
    RequireCrowdsale(propertyId);

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!pDbSpInfo->getSP(propertyId, sp)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
        }
    }

    const uint256& creationHash = sp.txid;

    CTransaction tx;
    uint256 hashBlock;
    if (!GetTransaction(creationHash, tx, hashBlock)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
    }

    UniValue response(UniValue::VOBJ);
    bool active = isCrowdsaleActive(propertyId);
    std::map<uint256, std::vector<int64_t> > database;

    if (active) {
        bool crowdFound = false;

        LOCK(cs_tally);

        for (CrowdMap::const_iterator it = my_crowds.begin(); it != my_crowds.end(); ++it) {
            const CMPCrowd& crowd = it->second;
            if (propertyId == crowd.getPropertyId()) {
                crowdFound = true;
                database = crowd.getDatabase();
            }
        }
        if (!crowdFound) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Crowdsale is flagged active but cannot be retrieved");
        }
    } else {
        database = sp.historicalData;
    }

    int64_t tokensIssued = getTotalTokens(propertyId);
    const std::string& txidClosed = sp.txid_close.GetHex();

    int64_t startTime = -1;
    if (!hashBlock.IsNull() && GetBlockIndex(hashBlock)) {
        startTime = GetBlockIndex(hashBlock)->nTime;
    }

    // note the database is already deserialized here and there is minimal performance penalty to iterate recipients to calculate amountRaised
    int64_t amountRaised = 0;
    int64_t amountIssuerTokens = 0;
    uint16_t propertyIdType = isPropertyDivisible(propertyId) ? TOKEN_PROPERTY_TYPE_DIVISIBLE : TOKEN_PROPERTY_TYPE_INDIVISIBLE;
    uint16_t desiredIdType = isPropertyDivisible(sp.property_desired) ? TOKEN_PROPERTY_TYPE_DIVISIBLE : TOKEN_PROPERTY_TYPE_INDIVISIBLE;
    std::map<std::string, UniValue> sortMap;
    for (std::map<uint256, std::vector<int64_t> >::const_iterator it = database.begin(); it != database.end(); it++) {
        UniValue participanttx(UniValue::VOBJ);
        std::string txid = it->first.GetHex();
        amountRaised += it->second.at(0);
        amountIssuerTokens += it->second.at(3);
        participanttx.pushKV("txid", txid);
        participanttx.pushKV("amountsent", FormatByType(it->second.at(0), desiredIdType));
        participanttx.pushKV("participanttokens", FormatByType(it->second.at(2), propertyIdType));
        participanttx.pushKV("issuertokens", FormatByType(it->second.at(3), propertyIdType));
        std::string sortKey = strprintf("%d-%s", it->second.at(1), txid);
        sortMap.insert(std::make_pair(sortKey, participanttx));
    }

    response.pushKV("propertyid", (uint64_t) propertyId);
    response.pushKV("name", sp.name);
    response.pushKV("ticker", sp.ticker);
    response.pushKV("active", active);
    response.pushKV("issuer", sp.issuer);
    response.pushKV("propertyiddesired", (uint64_t) sp.property_desired);

    CMPSPInfo::Entry desiredproperty;
    pDbSpInfo->getSP(sp.property_desired, desiredproperty);
    response.pushKV("tokendesiredname", desiredproperty.name);
    response.pushKV("tokendesired", desiredproperty.ticker);

    response.pushKV("tokensperunit", FormatMP(propertyId, sp.num_tokens));
    response.pushKV("earlybonus", sp.early_bird);
    response.pushKV("percenttoissuer", sp.percentage);
    response.pushKV("starttime", startTime);
    response.pushKV("deadline", sp.deadline);
    response.pushKV("amountraised", FormatMP(sp.property_desired, amountRaised));
    response.pushKV("tokensissued", FormatMP(propertyId, tokensIssued));
    response.pushKV("issuerbonustokens", FormatMP(propertyId, amountIssuerTokens));
    response.pushKV("addedissuertokens", FormatMP(propertyId, sp.missedTokens));

    // TODO: return fields every time?
    if (!active) response.pushKV("closedearly", sp.close_early);
    if (!active) response.pushKV("maxtokens", sp.max_tokens);
    if (sp.close_early) response.pushKV("endedtime", sp.timeclosed);
    if (sp.close_early && !sp.max_tokens) response.pushKV("closetx", txidClosed);

    if (showVerbose) {
        UniValue participanttxs(UniValue::VARR);
        for (std::map<std::string, UniValue>::iterator it = sortMap.begin(); it != sortMap.end(); ++it) {
            participanttxs.push_back(it->second);
        }
        response.pushKV("participanttransactions", participanttxs);
    }

    return response;
}

static UniValue gettokenactivecrowdsales(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "gettokenactivecrowdsales\n"
            "\nLists currently active crowdsales.\n"
            "\nResult:\n"
            "[                                 (array of JSON objects)\n"
            "  {\n"
            "    \"propertyid\" : n,                 (number) the identifier of the crowdsale\n"
            "    \"name\" : \"name\",                  (string) the name of the tokens issued via the crowdsale\n"
            "    \"issuer\" : \"address\",             (string) the Bitcoin address of the issuer on record\n"
            "    \"propertyiddesired\" : n,          (number) the identifier of the tokens eligible to participate in the crowdsale\n"
            "    \"tokensperunit\" : \"n.nnnnnnnn\",   (string) the amount of tokens granted per unit invested in the crowdsale\n"
            "    \"earlybonus\" : n,                 (number) an early bird bonus for participants in percent per week\n"
            "    \"percenttoissuer\" : n,            (number) a percentage of tokens that will be granted to the issuer\n"
            "    \"starttime\" : nnnnnnnnnn,         (number) the start time of the of the crowdsale as Unix timestamp\n"
            "    \"deadline\" : nnnnnnnnnn           (number) the deadline of the crowdsale as Unix timestamp\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokenactivecrowdsales", "")
            + HelpExampleRpc("gettokenactivecrowdsales", "")
        );

    UniValue response(UniValue::VARR);

    LOCK2(cs_main, cs_tally);

    for (CrowdMap::const_iterator it = my_crowds.begin(); it != my_crowds.end(); ++it) {
        const CMPCrowd& crowd = it->second;
        uint32_t propertyId = crowd.getPropertyId();

        CMPSPInfo::Entry sp;
        if (!pDbSpInfo->getSP(propertyId, sp)) {
            continue;
        }

        const uint256& creationHash = sp.txid;

        CTransaction tx;
        uint256 hashBlock;
        if (!GetTransaction(creationHash, tx, hashBlock)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
        }

        int64_t startTime = -1;
        if (!hashBlock.IsNull() && GetBlockIndex(hashBlock)) {
            startTime = GetBlockIndex(hashBlock)->nTime;
        }

        UniValue responseObj(UniValue::VOBJ);
        responseObj.pushKV("propertyid", (uint64_t) propertyId);
        responseObj.pushKV("name", sp.name);
        responseObj.pushKV("ticker", sp.ticker);
        responseObj.pushKV("issuer", sp.issuer);
        responseObj.pushKV("propertyiddesired", (uint64_t) sp.property_desired);

        CMPSPInfo::Entry desiredproperty;
        pDbSpInfo->getSP(sp.property_desired, desiredproperty);
        responseObj.pushKV("tokendesiredname", desiredproperty.name);
        responseObj.pushKV("tokendesired", desiredproperty.ticker);

        responseObj.pushKV("tokensperunit", FormatMP(propertyId, sp.num_tokens));
        responseObj.pushKV("earlybonus", sp.early_bird);
        responseObj.pushKV("percenttoissuer", sp.percentage);
        responseObj.pushKV("starttime", startTime);
        responseObj.pushKV("deadline", sp.deadline);
        response.push_back(responseObj);
    }

    return response;
}

static UniValue gettokengrants(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "gettokengrants ticker\n"
            "\nReturns information about granted and revoked units of managed tokens.\n"
            "\nArguments:\n"
            "1. ticker            (string, required) the ticker of managed token to lookup\n"
            "\nResult:\n"
            "{\n"
            "  \"propertyid\" : n,               (number) the identifier of the managed tokens\n"
            "  \"name\" : \"name\",                (string) the name of the tokens\n"
            "  \"issuer\" : \"address\",           (string) the Bitcoin address of the issuer on record\n"
            "  \"creationtxid\" : \"hash\",        (string) the hex-encoded creation transaction hash\n"
            "  \"totaltokens\" : \"n.nnnnnnnn\",   (string) the total number of tokens in existence\n"
            "  \"issuances\": [                  (array of JSON objects) a list of the granted and revoked tokens\n"
            "    {\n"
            "      \"txid\" : \"hash\",                (string) the hash of the transaction that granted tokens\n"
            "      \"grant\" : \"n.nnnnnnnn\"          (string) the number of tokens granted by this transaction\n"
            "    },\n"
            "    {\n"
            "      \"txid\" : \"hash\",                (string) the hash of the transaction that revoked tokens\n"
            "      \"grant\" : \"n.nnnnnnnn\"          (string) the number of tokens revoked by this transaction\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokengrants", "TOKEN")
            + HelpExampleRpc("gettokengrants", "TOKEN")
        );

    std::string ticker = ParseText(request.params[0]);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(ticker);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (false == pDbSpInfo->getSP(propertyId, sp)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
        }
    }
    UniValue response(UniValue::VOBJ);
    const uint256& creationHash = sp.txid;
    int64_t totalTokens = getTotalTokens(propertyId);

    // TODO: sort by height?

    UniValue issuancetxs(UniValue::VARR);
    std::map<uint256, std::vector<int64_t> >::const_iterator it;
    for (it = sp.historicalData.begin(); it != sp.historicalData.end(); it++) {
        const std::string& txid = it->first.GetHex();
        int64_t grantedTokens = it->second.at(0);
        int64_t revokedTokens = it->second.at(1);

        if (grantedTokens > 0) {
            UniValue granttx(UniValue::VOBJ);
            granttx.pushKV("txid", txid);
            granttx.pushKV("grant", FormatMP(propertyId, grantedTokens));
            issuancetxs.push_back(granttx);
        }

        if (revokedTokens > 0) {
            UniValue revoketx(UniValue::VOBJ);
            revoketx.pushKV("txid", txid);
            revoketx.pushKV("revoke", FormatMP(propertyId, revokedTokens));
            issuancetxs.push_back(revoketx);
        }
    }

    response.pushKV("propertyid", (uint64_t) propertyId);
    response.pushKV("name", sp.name);
    response.pushKV("ticker", sp.ticker);
    response.pushKV("issuer", sp.issuer);
    response.pushKV("creationtxid", creationHash.GetHex());
    response.pushKV("totaltokens", FormatMP(propertyId, totalTokens));
    response.pushKV("issuances", issuancetxs);

    return response;
}

static UniValue gettokenorderbook(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw runtime_error(
            "gettokenorderbook ticker ( ticker )\n"
            "\nList active offers on the distributed token exchange.\n"
            "\nArguments:\n"
            "1. ticker           (string, required) filter orders by token ticker for sale\n"
            "2. ticker           (string, optional) filter orders by token ticker desired\n"
            "\nResult:\n"
            "[                                              (array of JSON objects)\n"
            "  {\n"
            "    \"address\" : \"address\",                         (string) the Bitcoin address of the trader\n"
            "    \"txid\" : \"hash\",                               (string) the hex-encoded hash of the transaction of the order\n"
            "    \"ecosystem\" : \"main\"|\"test\",                   (string) the ecosytem in which the order was made (if \"cancel-ecosystem\")\n"
            "    \"propertyidforsale\" : n,                       (number) the identifier of the tokens put up for sale\n"
            "    \"propertyidforsaleisdivisible\" : true|false,   (boolean) whether the tokens for sale are divisible\n"
            "    \"amountforsale\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially offered\n"
            "    \"amountremaining\" : \"n.nnnnnnnn\",              (string) the amount of tokens still up for sale\n"
            "    \"propertyiddesired\" : n,                       (number) the identifier of the tokens desired in exchange\n"
            "    \"propertyiddesiredisdivisible\" : true|false,   (boolean) whether the desired tokens are divisible\n"
            "    \"amountdesired\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially desired\n"
            "    \"amounttofill\" : \"n.nnnnnnnn\",                 (string) the amount of tokens still needed to fill the offer completely\n"
            "    \"action\" : n,                                  (number) the action of the transaction: (1) \"trade\", (2) \"cancel-price\", (3) \"cancel-pair\", (4) \"cancel-ecosystem\"\n"
            "    \"block\" : nnnnnn,                              (number) the index of the block that contains the transaction\n"
            "    \"blocktime\" : nnnnnnnnnn                       (number) the timestamp of the block that contains the transaction\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokenorderbook", "TOKEN")
            + HelpExampleRpc("gettokenorderbook", "TOKEN")
        );

    bool filterDesired = (request.params.size() > 1);
    std::string tickerForSale = ParseText(request.params[0]);

    uint32_t propertyIdForSale = pDbSpInfo->findSPByTicker(tickerForSale);
    if (propertyIdForSale == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

    uint32_t propertyIdDesired = 0;

    RequireExistingProperty(propertyIdForSale);

    if (filterDesired) {
        std::string tickerDesired = ParseText(request.params[1]);

        propertyIdDesired = pDbSpInfo->findSPByTicker(tickerDesired);
        if (propertyIdDesired == 0)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

        RequireExistingProperty(propertyIdDesired);
        RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
        RequireDifferentIds(propertyIdForSale, propertyIdDesired);
    }

    std::vector<CMPMetaDEx> vecMetaDexObjects;
    {
        LOCK(cs_tally);
        for (md_PropertiesMap::const_iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
            const md_PricesMap& prices = my_it->second;
            for (md_PricesMap::const_iterator it = prices.begin(); it != prices.end(); ++it) {
                const md_Set& indexes = it->second;
                for (md_Set::const_iterator it = indexes.begin(); it != indexes.end(); ++it) {
                    const CMPMetaDEx& obj = *it;
                    if (obj.getProperty() != propertyIdForSale) continue;
                    if (!filterDesired || obj.getDesProperty() == propertyIdDesired) vecMetaDexObjects.push_back(obj);
                }
            }
        }
    }

    UniValue response(UniValue::VARR);
    MetaDexObjectsToJSON(vecMetaDexObjects, response);
    return response;
}

static UniValue gettokentradehistoryforaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw runtime_error(
            "gettokentradehistoryforaddress \"address\" ( count ticker )\n"
            "\nRetrieves the history of orders on the distributed exchange for the supplied address.\n"
            "\nArguments:\n"
            "1. address              (string, required) address to retrieve history for\n"
            "2. count                (number, optional) number of orders to retrieve (default: 10)\n"
            "3. ticker               (string, optional) filter by token transacted (default: no filter)\n"
            "\nResult:\n"
            "[                                              (array of JSON objects)\n"
            "  {\n"
            "    \"txid\" : \"hash\",                               (string) the hex-encoded hash of the transaction of the order\n"
            "    \"sendingaddress\" : \"address\",                  (string) the Bitcoin address of the trader\n"
            "    \"ismine\" : true|false,                         (boolean) whether the order involes an address in the wallet\n"
            "    \"confirmations\" : nnnnnnnnnn,                  (number) the number of transaction confirmations\n"
            "    \"fee\" : \"n.nnnnnnnn\",                          (string) the transaction fee in bitcoins\n"
            "    \"blocktime\" : nnnnnnnnnn,                      (number) the timestamp of the block that contains the transaction\n"
            "    \"valid\" : true|false,                          (boolean) whether the transaction is valid\n"
            "    \"version\" : n,                                 (number) the transaction version\n"
            "    \"type_int\" : n,                                (number) the transaction type as number\n"
            "    \"type\" : \"type\",                               (string) the transaction type as string\n"
            "    \"propertyidforsale\" : n,                       (number) the identifier of the tokens put up for sale\n"
            "    \"propertyidforsaleisdivisible\" : true|false,   (boolean) whether the tokens for sale are divisible\n"
            "    \"amountforsale\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially offered\n"
            "    \"propertyiddesired\" : n,                       (number) the identifier of the tokens desired in exchange\n"
            "    \"propertyiddesiredisdivisible\" : true|false,   (boolean) whether the desired tokens are divisible\n"
            "    \"amountdesired\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially desired\n"
            "    \"unitprice\" : \"n.nnnnnnnnnnn...\"               (string) the unit price (shown in the property desired)\n"
            "    \"status\" : \"status\"                            (string) the status of the order (\"open\", \"cancelled\", \"filled\", ...)\n"
            "    \"canceltxid\" : \"hash\",                         (string) the hash of the transaction that cancelled the order (if cancelled)\n"
            "    \"matches\": [                                   (array of JSON objects) a list of matched orders and executed trades\n"
            "      {\n"
            "        \"txid\" : \"hash\",                               (string) the hash of the transaction that was matched against\n"
            "        \"block\" : nnnnnn,                              (number) the index of the block that contains this transaction\n"
            "        \"address\" : \"address\",                         (string) the Bitcoin address of the other trader\n"
            "        \"amountsold\" : \"n.nnnnnnnn\",                   (string) the number of tokens sold in this trade\n"
            "        \"amountreceived\" : \"n.nnnnnnnn\"                (string) the number of tokens traded in exchange\n"
            "      },\n"
            "      ...\n"
            "    ]\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nNote:\n"
            "The documentation only covers the output for a trade, but there are also cancel transactions with different properties.\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokentradehistoryforaddress", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\"")
            + HelpExampleRpc("gettokentradehistoryforaddress", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\"")
        );

    std::string address = ParseAddress(request.params[0]);
    uint64_t count = (request.params.size() > 1) ? request.params[1].get_int64() : 10;
    uint32_t propertyId = 0;

    if (request.params.size() > 2) {
        std::string ticker = ParseText(request.params[2]);

        propertyId = pDbSpInfo->findSPByTicker(ticker);
        if (propertyId == 0)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

        RequireExistingProperty(propertyId);
    }

    // Obtain a sorted vector of txids for the address trade history
    std::vector<uint256> vecTransactions;
    {
        LOCK(cs_tally);
        pDbTradeList->getTradesForAddress(address, vecTransactions, propertyId);
    }

    // Populate the address trade history into JSON objects until we have processed count transactions
    UniValue response(UniValue::VARR);
    uint32_t processed = 0;
    for(std::vector<uint256>::reverse_iterator it = vecTransactions.rbegin(); it != vecTransactions.rend(); ++it) {
        UniValue txobj(UniValue::VOBJ);
        int populateResult = populateRPCTransactionObject(*it, txobj, "", true);
        if (0 == populateResult) {
            response.push_back(txobj);
            processed++;
            if (processed >= count) break;
        }
    }

    return response;
}

static UniValue gettokentradehistoryforpair(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw runtime_error(
            "gettokentradehistoryforpair ticker ticker ( count )\n"
            "\nRetrieves the history of trades on the distributed token exchange for the specified market.\n"
            "\nArguments:\n"
            "1. ticker               (string, required) the first side of the traded pair\n"
            "2. ticker               (string, required) the second side of the traded pair\n"
            "3. count                (number, optional) number of trades to retrieve (default: 10)\n"
            "\nResult:\n"
            "[                                      (array of JSON objects)\n"
            "  {\n"
            "    \"block\" : nnnnnn,                      (number) the index of the block that contains the trade match\n"
            "    \"unitprice\" : \"n.nnnnnnnnnnn...\" ,     (string) the unit price used to execute this trade (received/sold)\n"
            "    \"inverseprice\" : \"n.nnnnnnnnnnn...\",   (string) the inverse unit price (sold/received)\n"
            "    \"sellertxid\" : \"hash\",                 (string) the hash of the transaction of the seller\n"
            "    \"address\" : \"address\",                 (string) the Bitcoin address of the seller\n"
            "    \"amountsold\" : \"n.nnnnnnnn\",           (string) the number of tokens sold in this trade\n"
            "    \"amountreceived\" : \"n.nnnnnnnn\",       (string) the number of tokens traded in exchange\n"
            "    \"matchingtxid\" : \"hash\",               (string) the hash of the transaction that was matched against\n"
            "    \"matchingaddress\" : \"address\"          (string) the Bitcoin address of the other party of this trade\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokentradehistoryforpair", "TOKEN DESIRED 500")
            + HelpExampleRpc("gettokentradehistoryforpair", "TOKEN, DESIRED, 500")
        );

    // obtain property identifiers for pair & check valid parameters
    std::string tokenTickerSideA = ParseText(request.params[0]);
    uint32_t propertyIdSideA = pDbSpInfo->findSPByTicker(tokenTickerSideA);
    if (propertyIdSideA == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

    std::string tokenTickerSideB = ParseText(request.params[1]);
    uint32_t propertyIdSideB = pDbSpInfo->findSPByTicker(tokenTickerSideB);
    if (propertyIdSideB == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

    uint64_t count = (request.params.size() > 2) ? request.params[2].get_int64() : 10;

    RequireExistingProperty(propertyIdSideA);
    RequireExistingProperty(propertyIdSideB);
    RequireSameEcosystem(propertyIdSideA, propertyIdSideB);
    RequireDifferentIds(propertyIdSideA, propertyIdSideB);

    // request pair trade history from trade db
    UniValue response(UniValue::VARR);
    LOCK(cs_tally);
    pDbTradeList->getTradesForPair(propertyIdSideA, propertyIdSideB, response, count);
    return response;
}

static UniValue gettokenactivedexsells(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            "gettokenactivedexsells ( address )\n"
            "\nReturns currently active offers on the distributed exchange.\n"
            "\nArguments:\n"
            "1. address              (string, optional) address filter (default: include any)\n"
            "\nResult:\n"
            "[                                   (array of JSON objects)\n"
            "  {\n"
            "    \"txid\" : \"hash\",                    (string) the hash of the transaction of this offer\n"
            "    \"propertyid\" : n,                   (number) the identifier of the tokens for sale\n"
            "    \"seller\" : \"address\",               (string) the Bitcoin address of the seller\n"
            "    \"amountavailable\" : \"n.nnnnnnnn\",   (string) the number of tokens still listed for sale and currently available\n"
            "    \"rapidsdesired\" : \"n.nnnnnnnn\",    (string) the number of bitcoins desired in exchange\n"
            "    \"unitprice\" : \"n.nnnnnnnn\" ,        (string) the unit price (RPD/token)\n"
            "    \"timelimit\" : nn,                   (number) the time limit in blocks a buyer has to pay following a successful accept\n"
            "    \"minimumfee\" : \"n.nnnnnnnn\",        (string) the minimum mining fee a buyer has to pay to accept this offer\n"
            "    \"amountaccepted\" : \"n.nnnnnnnn\",    (string) the number of tokens currently reserved for pending \"accept\" orders\n"
            "    \"accepts\": [                        (array of JSON objects) a list of pending \"accept\" orders\n"
            "      {\n"
            "        \"buyer\" : \"address\",                (string) the Bitcoin address of the buyer\n"
            "        \"block\" : nnnnnn,                   (number) the index of the block that contains the \"accept\" order\n"
            "        \"blocksleft\" : nn,                  (number) the number of blocks left to pay\n"
            "        \"amount\" : \"n.nnnnnnnn\"             (string) the amount of tokens accepted and reserved\n"
            "        \"amounttopay\" : \"n.nnnnnnnn\"        (string) the amount in bitcoins needed finalize the trade\n"
            "      },\n"
            "      ...\n"
            "    ]\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokenactivedexsells", "")
            + HelpExampleRpc("gettokenactivedexsells", "")
        );

    std::string addressFilter;

    if (request.params.size() > 0) {
        addressFilter = ParseAddressOrEmpty(request.params[0]);
    }

    UniValue response(UniValue::VARR);

    int curBlock = GetHeight();

    LOCK(cs_tally);

    for (OfferMap::iterator it = my_offers.begin(); it != my_offers.end(); ++it) {
        const CMPOffer& selloffer = it->second;
        const std::string& sellCombo = it->first;
        std::string seller = sellCombo.substr(0, sellCombo.size() - 2);

        // filtering
        if (!addressFilter.empty() && seller != addressFilter) continue;

        std::string txid = selloffer.getHash().GetHex();
        uint32_t propertyId = selloffer.getProperty();
        int64_t minFee = selloffer.getMinFee();
        uint8_t timeLimit = selloffer.getBlockTimeLimit();
        int64_t sellOfferAmount = selloffer.getOfferAmountOriginal(); //badly named - "Original" implies off the wire, but is amended amount
        int64_t sellRapidsDesired = selloffer.getRPDDesiredOriginal(); //badly named - "Original" implies off the wire, but is amended amount
        int64_t amountAvailable = GetTokenBalance(seller, propertyId, SELLOFFER_RESERVE);
        int64_t amountAccepted = GetTokenBalance(seller, propertyId, ACCEPT_RESERVE);

        // TODO: no math, and especially no rounding here (!)
        // TODO: no math, and especially no rounding here (!)
        // TODO: no math, and especially no rounding here (!)

        CMPSPInfo::Entry property;
        pDbSpInfo->getSP(propertyId, property);

        // calculate unit price and updated amount of bitcoin desired
        double unitPriceFloat = 0.0;
        if ((sellOfferAmount > 0) && (sellRapidsDesired > 0)) {
            unitPriceFloat = (double) sellRapidsDesired / (double) sellOfferAmount; // divide by zero protection
            if (!isPropertyDivisible(propertyId)) {
                unitPriceFloat /= 100000000.0;
            }
        }
        int64_t unitPrice = rounduint64(unitPriceFloat * COIN);
        int64_t bitcoinDesired = calculateDesiredRPD(sellOfferAmount, sellRapidsDesired, amountAvailable);

        UniValue responseObj(UniValue::VOBJ);
        responseObj.pushKV("txid", txid);
        responseObj.pushKV("propertyid", (uint64_t) propertyId);
        responseObj.pushKV("name", property.name);
        responseObj.pushKV("ticker", property.ticker);
        responseObj.pushKV("seller", seller);
        responseObj.pushKV("amountavailable", FormatMP(propertyId, amountAvailable));
        responseObj.pushKV("rapidsdesired", FormatDivisibleMP(bitcoinDesired));
        responseObj.pushKV("unitprice", FormatDivisibleMP(unitPrice));
        responseObj.pushKV("timelimit", timeLimit);
        responseObj.pushKV("minimumfee", FormatDivisibleMP(minFee));

        // display info about accepts related to sell
        responseObj.pushKV("amountaccepted", FormatMP(propertyId, amountAccepted));
        UniValue acceptsMatched(UniValue::VARR);
        for (AcceptMap::const_iterator ait = my_accepts.begin(); ait != my_accepts.end(); ++ait) {
            UniValue matchedAccept(UniValue::VOBJ);
            const CMPAccept& accept = ait->second;
            const std::string& acceptCombo = ait->first;

            // does this accept match the sell?
            if (accept.getHash() == selloffer.getHash()) {
                // split acceptCombo out to get the buyer address
                std::string buyer = acceptCombo.substr((acceptCombo.find("+") + 1), (acceptCombo.size()-(acceptCombo.find("+") + 1)));
                int blockOfAccept = accept.getAcceptBlock();
                int blocksLeftToPay = (blockOfAccept + selloffer.getBlockTimeLimit()) - curBlock;
                int64_t amountAccepted = accept.getAcceptAmountRemaining();
                // TODO: don't recalculate!
                int64_t amountToPayInRPD = calculateDesiredRPD(accept.getOfferAmountOriginal(), accept.getRPDDesiredOriginal(), amountAccepted);
                matchedAccept.pushKV("buyer", buyer);
                matchedAccept.pushKV("block", blockOfAccept);
                matchedAccept.pushKV("blocksleft", blocksLeftToPay);
                matchedAccept.pushKV("amount", FormatMP(propertyId, amountAccepted));
                matchedAccept.pushKV("amounttopay", FormatDivisibleMP(amountToPayInRPD));
                acceptsMatched.push_back(matchedAccept);
            }
        }
        responseObj.pushKV("accepts", acceptsMatched);

        // add sell object into response array
        response.push_back(responseObj);
    }

    return response;
}

static UniValue listblocktokentransactions(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "listblocktokentransactions index\n"
            "\nLists all Token transactions in a block.\n"
            "\nArguments:\n"
            "1. index                (number, required) the block height or block index\n"
            "\nResult:\n"
            "[                       (array of string)\n"
            "  \"hash\",                 (string) the hash of the transaction\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listblocktokentransactions", "279007")
            + HelpExampleRpc("listblocktokentransactions", "279007")
        );

    int blockHeight = request.params[0].get_int();

    RequireHeightInChain(blockHeight);

    // next let's obtain the block for this height
    CBlock block;
    {
        LOCK(cs_main);
        CBlockIndex* pBlockIndex = chainActive[blockHeight];

        if (!ReadBlockFromDisk(block, pBlockIndex)) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to read block from disk");
        }
    }

    UniValue response(UniValue::VARR);

    // now we want to loop through each of the transactions in the block and run against CMPTxList::exists
    // those that return positive add to our response array

    LOCK(cs_tally);

    for (const auto tx : block.vtx) {
        if (pDbTransactionList->exists(tx.GetHash())) {
            // later we can add a verbose flag to decode here, but for now callers can send returned txids into gettransaction_MP
            // add the txid into the response as it's an MP transaction
            response.push_back(tx.GetHash().GetHex());
        }
    }

    return response;
}

static UniValue listblockstokentransactions(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "token_listblocktransactions firstblock lastblock\n"
            "\nLists all Token transactions in a given range of blocks.\n"
            "\nNote: the list of transactions is unordered and can contain invalid transactions!\n"
            "\nArguments:\n"
            "1. firstblock           (number, required) the index of the first block to consider\n"
            "2. lastblock            (number, required) the index of the last block to consider\n"
            "\nResult:\n"
            "[                       (array of string)\n"
            "  \"hash\",                 (string) the hash of the transaction\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listblockstokentransactions", "279007 300000")
            + HelpExampleRpc("listblockstokentransactions", "279007, 300000")
        );

    int blockFirst = request.params[0].get_int();
    int blockLast = request.params[1].get_int();

    std::set<uint256> txs;
    UniValue response(UniValue::VARR);

    LOCK(cs_tally);
    {
        pDbTransactionList->GetTokenTxsInBlockRange(blockFirst, blockLast, txs);
    }

    for(const uint256& tx : txs) {
        response.push_back(tx.GetHex());
    }

    return response;
}

static UniValue gettokentransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "gettokentransaction \"txid\"\n"
            "\nGet detailed information about an Token transaction.\n"
            "\nArguments:\n"
            "1. txid                 (string, required) the hash of the transaction to lookup\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\" : \"hash\",                  (string) the hex-encoded hash of the transaction\n"
            "  \"sendingaddress\" : \"address\",     (string) the Bitcoin address of the sender\n"
            "  \"referenceaddress\" : \"address\",   (string) a Bitcoin address used as reference (if any)\n"
            "  \"ismine\" : true|false,            (boolean) whether the transaction involes an address in the wallet\n"
            "  \"confirmations\" : nnnnnnnnnn,     (number) the number of transaction confirmations\n"
            "  \"fee\" : \"n.nnnnnnnn\",             (string) the transaction fee in bitcoins\n"
            "  \"blocktime\" : nnnnnnnnnn,         (number) the timestamp of the block that contains the transaction\n"
            "  \"valid\" : true|false,             (boolean) whether the transaction is valid\n"
            "  \"invalidreason\" : \"reason\",     (string) if a transaction is invalid, the reason \n"
            "  \"version\" : n,                    (number) the transaction version\n"
            "  \"type_int\" : n,                   (number) the transaction type as number\n"
            "  \"type\" : \"type\",                  (string) the transaction type as string\n"
            "  [...]                             (mixed) other transaction type specific properties\n"
            "}\n"
            "\nbExamples:\n"
            + HelpExampleCli("gettokentransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleRpc("gettokentransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    uint256 hash = ParseHashV(request.params[0], "txid");

    UniValue txobj(UniValue::VOBJ);
    int populateResult = populateRPCTransactionObject(hash, txobj, "", false);
    if (populateResult != 0) PopulateFailure(populateResult);

    return txobj;
}

static UniValue listtokentransactions(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 5)
        throw runtime_error(
            "listtokentransactions ( \"address\" count skip startblock endblock )\n"
            "\nList wallet transactions, optionally filtered by an address and block boundaries.\n"
            "\nArguments:\n"
            "1. address              (string, optional) address filter (default: \"*\")\n"
            "2. count                (number, optional) show at most n transactions (default: 10)\n"
            "3. skip                 (number, optional) skip the first n transactions (default: 0)\n"
            "4. startblock           (number, optional) first block to begin the search (default: 0)\n"
            "5. endblock             (number, optional) last block to include in the search (default: 999999999)\n"
            "\nResult:\n"
            "[                                 (array of JSON objects)\n"
            "  {\n"
            "    \"txid\" : \"hash\",                  (string) the hex-encoded hash of the transaction\n"
            "    \"sendingaddress\" : \"address\",     (string) the Bitcoin address of the sender\n"
            "    \"referenceaddress\" : \"address\",   (string) a Bitcoin address used as reference (if any)\n"
            "    \"ismine\" : true|false,            (boolean) whether the transaction involes an address in the wallet\n"
            "    \"confirmations\" : nnnnnnnnnn,     (number) the number of transaction confirmations\n"
            "    \"fee\" : \"n.nnnnnnnn\",             (string) the transaction fee in bitcoins\n"
            "    \"blocktime\" : nnnnnnnnnn,         (number) the timestamp of the block that contains the transaction\n"
            "    \"valid\" : true|false,             (boolean) whether the transaction is valid\n"
            "    \"version\" : n,                    (number) the transaction version\n"
            "    \"type_int\" : n,                   (number) the transaction type as number\n"
            "    \"type\" : \"type\",                  (string) the transaction type as string\n"
            "    [...]                             (mixed) other transaction type specific properties\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listtokentransactions", "")
            + HelpExampleRpc("listtokentransactions", "")
        );

    // obtains parameters - default all wallet addresses & last 10 transactions
    std::string addressParam;
    if (request.params.size() > 0) {
        if (("*" != request.params[0].get_str()) && ("" != request.params[0].get_str())) addressParam = request.params[0].get_str();
    }
    int64_t nCount = 10;
    if (request.params.size() > 1) nCount = request.params[1].get_int64();
    if (nCount < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    int64_t nFrom = 0;
    if (request.params.size() > 2) nFrom = request.params[2].get_int64();
    if (nFrom < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");
    int64_t nStartBlock = 0;
    if (request.params.size() > 3) nStartBlock = request.params[3].get_int64();
    if (nStartBlock < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative start block");
    int64_t nEndBlock = 999999999;
    if (request.params.size() > 4) nEndBlock = request.params[4].get_int64();
    if (nEndBlock < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative end block");

    // obtain a sorted list of Token layer wallet transactions (including STO receipts and pending)
    std::map<std::string,uint256> walletTransactions = FetchWalletTokenTransactions(nFrom+nCount, nStartBlock, nEndBlock);

    // reverse iterate over (now ordered) transactions and populate RPC objects for each one
    UniValue response(UniValue::VARR);
    for (std::map<std::string,uint256>::reverse_iterator it = walletTransactions.rbegin(); it != walletTransactions.rend(); it++) {
        if (nFrom <= 0 && nCount > 0) {
            uint256 txHash = it->second;
            UniValue txobj(UniValue::VOBJ);
            int populateResult = populateRPCTransactionObject(txHash, txobj, addressParam);
            if (0 == populateResult) {
                response.push_back(txobj);
                nCount--;
            }
        }
        nFrom--;
    }

    return response;
}

static UniValue listpendingtokentransactions(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            "listpendingtokentransactions ( \"address\" )\n"
            "\nReturns a list of unconfirmed Token transactions, pending in the memory pool.\n"
            "\nAn optional filter can be provided to only include transactions which involve the given address.\n"
            "\nNote: the validity of pending transactions is uncertain, and the state of the memory pool may "
            "change at any moment. It is recommended to check transactions after confirmation, and pending "
            "transactions should be considered as invalid.\n"
            "\nArguments:\n"
            "1. address              (string, optional) address filter (default: \"\" for no filter)\n"
            "\nResult:\n"
            "[                                 (array of JSON objects)\n"
            "  {\n"
            "    \"txid\" : \"hash\",                  (string) the hex-encoded hash of the transaction\n"
            "    \"sendingaddress\" : \"address\",     (string) the Bitcoin address of the sender\n"
            "    \"referenceaddress\" : \"address\",   (string) a Bitcoin address used as reference (if any)\n"
            "    \"ismine\" : true|false,            (boolean) whether the transaction involes an address in the wallet\n"
            "    \"fee\" : \"n.nnnnnnnn\",             (string) the transaction fee in bitcoins\n"
            "    \"version\" : n,                    (number) the transaction version\n"
            "    \"type_int\" : n,                   (number) the transaction type as number\n"
            "    \"type\" : \"type\",                  (string) the transaction type as string\n"
            "    [...]                             (mixed) other transaction type specific properties\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listpendingtokentransactions", "")
            + HelpExampleRpc("listpendingtokentransactions", "")
        );

    std::string filterAddress;
    if (request.params.size() > 0) {
        filterAddress = ParseAddressOrEmpty(request.params[0]);
    }

    std::vector<uint256> vTxid;
    mempool.queryHashes(vTxid);

    UniValue result(UniValue::VARR);
    for (const uint256& hash : vTxid) {
        if (!IsInMarkerCache(hash)) {
            continue;
        }

        UniValue txObj(UniValue::VOBJ);
        if (populateRPCTransactionObject(hash, txObj, filterAddress, false) == 0) {
            result.push_back(txObj);
        }
    }

    return result;
}

static UniValue gettokensinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw runtime_error(
            "gettokensinfo\n"
            "Returns various state information of the client and protocol.\n"
            "\nResult:\n"
            "{\n"
            "  \"block\" : nnnnnn,                      (number) index of the last processed block\n"
            "  \"blocktime\" : nnnnnnnnnn,              (number) timestamp of the last processed block\n"
            "  \"blocktransactions\" : nnnn,            (number) Token transactions found in the last processed block\n"
            "  \"totaltransactions\" : nnnnnnnn,        (number) Token transactions processed in total\n"
            "  \"alerts\" : [                           (array of JSON objects) active protocol alert (if any)\n"
            "    {\n"
            "      \"alerttypeint\" : n,                    (number) alert type as integer\n"
            "      \"alerttype\" : \"xxx\",                   (string) alert type\n"
            "      \"alertexpiry\" : \"nnnnnnnnnn\",          (string) expiration criteria\n"
            "      \"alertmessage\" : \"xxx\"                 (string) information about the alert\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokensinfo", "")
            + HelpExampleRpc("gettokensinfo", "")
        );

    UniValue infoResponse(UniValue::VOBJ);

    // provide the current block details
    int block = GetHeight();
    int64_t blockTime = GetLatestBlockTime();

    LOCK(cs_tally);

    int blockMPTransactions = pDbTransactionList->getMPTransactionCountBlock(block);
    int totalMPTransactions = pDbTransactionList->getMPTransactionCountTotal();
    int totalMPTrades = pDbTradeList->getMPTradeCountTotal();
    infoResponse.pushKV("block", block);
    infoResponse.pushKV("blocktime", blockTime);
    infoResponse.pushKV("blocktransactions", blockMPTransactions);

    // provide the number of trades completed
    infoResponse.pushKV("totaltrades", totalMPTrades);
    // provide the number of transactions parsed
    infoResponse.pushKV("totaltransactions", totalMPTransactions);

    // handle alerts
    UniValue alerts(UniValue::VARR);
    std::vector<AlertData> tokenAlerts = GetTokenCoreAlerts();
    for (std::vector<AlertData>::iterator it = tokenAlerts.begin(); it != tokenAlerts.end(); it++) {
        AlertData alert = *it;
        UniValue alertResponse(UniValue::VOBJ);
        std::string alertTypeStr;
        switch (alert.alert_type) {
            case 1: alertTypeStr = "alertexpiringbyblock";
            break;
            case 2: alertTypeStr = "alertexpiringbyblocktime";
            break;
            case 3: alertTypeStr = "alertexpiringbyclientversion";
            break;
            default: alertTypeStr = "error";
        }
        alertResponse.pushKV("alerttypeint", alert.alert_type);
        alertResponse.pushKV("alerttype", alertTypeStr);
        alertResponse.pushKV("alertexpiry", FormatIndivisibleMP(alert.alert_expiry));
        alertResponse.pushKV("alertmessage", alert.alert_message);
        alerts.push_back(alertResponse);
    }
    infoResponse.pushKV("alerts", alerts);

    return infoResponse;
}

static UniValue token_getsto(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw runtime_error(
            "token_getsto \"txid\" \"recipientfilter\"\n"
            "\nGet information and recipients of a send-to-owners transaction.\n"
            "\nArguments:\n"
            "1. txid                 (string, required) the hash of the transaction to lookup\n"
            "2. recipientfilter      (string, optional) a filter for recipients (wallet by default, \"*\" for all)\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\" : \"hash\",                (string) the hex-encoded hash of the transaction\n"
            "  \"sendingaddress\" : \"address\",   (string) the Bitcoin address of the sender\n"
            "  \"ismine\" : true|false,          (boolean) whether the transaction involes an address in the wallet\n"
            "  \"confirmations\" : nnnnnnnnnn,   (number) the number of transaction confirmations\n"
            "  \"fee\" : \"n.nnnnnnnn\",           (string) the transaction fee in bitcoins\n"
            "  \"blocktime\" : nnnnnnnnnn,       (number) the timestamp of the block that contains the transaction\n"
            "  \"valid\" : true|false,           (boolean) whether the transaction is valid\n"
            "  \"version\" : n,                  (number) the transaction version\n"
            "  \"type_int\" : n,                 (number) the transaction type as number\n"
            "  \"type\" : \"type\",                (string) the transaction type as string\n"
            "  \"propertyid\" : n,               (number) the identifier of sent tokens\n"
            "  \"divisible\" : true|false,       (boolean) whether the sent tokens are divisible\n"
            "  \"amount\" : \"n.nnnnnnnn\",        (string) the number of tokens sent to owners\n"
            "  \"totalstofee\" : \"n.nnnnnnnn\",   (string) the fee paid by the sender, nominated in OMN or TOMN\n"
            "  \"recipients\": [                 (array of JSON objects) a list of recipients\n"
            "    {\n"
            "      \"address\" : \"address\",          (string) the Bitcoin address of the recipient\n"
            "      \"amount\" : \"n.nnnnnnnn\"         (string) the number of tokens sent to this recipient\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("token_getsto", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" \"*\"")
            + HelpExampleRpc("token_getsto", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\", \"*\"")
        );

    uint256 hash = ParseHashV(request.params[0], "txid");
    std::string filterAddress;
    if (request.params.size() > 1) filterAddress = ParseAddressOrWildcard(request.params[1]);

    UniValue txobj(UniValue::VOBJ);
    int populateResult = populateRPCTransactionObject(hash, txobj, "", true);
    if (populateResult != 0) PopulateFailure(populateResult);

    return txobj;
}

static UniValue gettokentrade(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "gettokentrade \"txid\"\n"
            "\nGet detailed information and trade matches for orders on the distributed token exchange.\n"
            "\nArguments:\n"
            "1. txid                 (string, required) the hash of the order to lookup\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\" : \"hash\",                               (string) the hex-encoded hash of the transaction of the order\n"
            "  \"sendingaddress\" : \"address\",                  (string) the Bitcoin address of the trader\n"
            "  \"ismine\" : true|false,                         (boolean) whether the order involes an address in the wallet\n"
            "  \"confirmations\" : nnnnnnnnnn,                  (number) the number of transaction confirmations\n"
            "  \"fee\" : \"n.nnnnnnnn\",                          (string) the transaction fee in bitcoins\n"
            "  \"blocktime\" : nnnnnnnnnn,                      (number) the timestamp of the block that contains the transaction\n"
            "  \"valid\" : true|false,                          (boolean) whether the transaction is valid\n"
            "  \"version\" : n,                                 (number) the transaction version\n"
            "  \"type_int\" : n,                                (number) the transaction type as number\n"
            "  \"type\" : \"type\",                               (string) the transaction type as string\n"
            "  \"propertyidforsale\" : n,                       (number) the identifier of the tokens put up for sale\n"
            "  \"propertyidforsaleisdivisible\" : true|false,   (boolean) whether the tokens for sale are divisible\n"
            "  \"amountforsale\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially offered\n"
            "  \"propertyiddesired\" : n,                       (number) the identifier of the tokens desired in exchange\n"
            "  \"propertyiddesiredisdivisible\" : true|false,   (boolean) whether the desired tokens are divisible\n"
            "  \"amountdesired\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially desired\n"
            "  \"unitprice\" : \"n.nnnnnnnnnnn...\"               (string) the unit price (shown in the property desired)\n"
            "  \"status\" : \"status\"                            (string) the status of the order (\"open\", \"cancelled\", \"filled\", ...)\n"
            "  \"canceltxid\" : \"hash\",                         (string) the hash of the transaction that cancelled the order (if cancelled)\n"
            "  \"matches\": [                                   (array of JSON objects) a list of matched orders and executed trades\n"
            "    {\n"
            "      \"txid\" : \"hash\",                               (string) the hash of the transaction that was matched against\n"
            "      \"block\" : nnnnnn,                              (number) the index of the block that contains this transaction\n"
            "      \"address\" : \"address\",                         (string) the Bitcoin address of the other trader\n"
            "      \"amountsold\" : \"n.nnnnnnnn\",                   (string) the number of tokens sold in this trade\n"
            "      \"amountreceived\" : \"n.nnnnnnnn\"                (string) the number of tokens traded in exchange\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "}\n"
            "\nNote:\n"
            "The documentation only covers the output for a trade, but there are also cancel transactions with different properties.\n"
            "\nExamples:\n"
            + HelpExampleCli("gettokentrade", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleRpc("gettokentrade", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    uint256 hash = ParseHashV(request.params[0], "txid");

    UniValue txobj(UniValue::VOBJ);
    int populateResult = populateRPCTransactionObject(hash, txobj, "", true);
    if (populateResult != 0) PopulateFailure(populateResult);

    return txobj;
}

static UniValue gettokenconsensushash(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw runtime_error(
            "gettokenconsensushash\n"
            "\nReturns the consensus hash for all balances for the current block.\n"
            "\nResult:\n"
            "{\n"
            "  \"block\" : nnnnnn,          (number) the index of the block this consensus hash applies to\n"
            "  \"blockhash\" : \"hash\",      (string) the hash of the corresponding block\n"
            "  \"consensushash\" : \"hash\"   (string) the consensus hash for the block\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("gettokenconsensushash", "")
            + HelpExampleRpc("gettokenconsensushash", "")
        );

    LOCK(cs_main); // TODO - will this ensure we don't take in a new block in the couple of ms it takes to calculate the consensus hash?

    int block = GetHeight();

    CBlockIndex* pblockindex = chainActive[block];
    uint256 blockHash = pblockindex->GetBlockHash();

    uint256 consensusHash = GetConsensusHash();

    UniValue response(UniValue::VOBJ);
    response.pushKV("block", block);
    response.pushKV("blockhash", blockHash.GetHex());
    response.pushKV("consensushash", consensusHash.GetHex());

    return response;
}

static UniValue gettokenmetadexhash(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            "gettokenmetadexhash ticker\n"
            "\nReturns a hash of the current state of the MetaDEx (default) or orderbook.\n"
            "\nArguments:\n"
            "1. ticker                       (string, optional) hash orderbook (only trades selling token with ticker)\n"
            "\nResult:\n"
            "{\n"
            "  \"block\" : nnnnnn,          (number) the index of the block this hash applies to\n"
            "  \"blockhash\" : \"hash\",    (string) the hash of the corresponding block\n"
            "  \"propertyid\" : nnnnnn,     (number) the market this hash applies to (or 0 for all markets)\n"
            "  \"metadexhash\" : \"hash\"   (string) the hash for the state of the MetaDEx/orderbook\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("gettokenmetadexhash", "TOKEN")
            + HelpExampleRpc("gettokenmetadexhash", "TOKEN")
        );

    LOCK(cs_main);

    uint32_t propertyId = 0;
    if (request.params.size() > 0) {
        std::string ticker = ParseText(request.params[0]);

        propertyId = pDbSpInfo->findSPByTicker(ticker);
        if (propertyId == 0)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

        RequireExistingProperty(propertyId);
    }

    int block = GetHeight();
    CBlockIndex* pblockindex = chainActive[block];
    uint256 blockHash = pblockindex->GetBlockHash();

    uint256 metadexHash = GetMetaDExHash(propertyId);

    UniValue response(UniValue::VOBJ);
    response.pushKV("block", block);
    response.pushKV("blockhash", blockHash.GetHex());
    response.pushKV("propertyid", (uint64_t)propertyId);
    response.pushKV("metadexhash", metadexHash.GetHex());

    return response;
}

static UniValue gettokenbalanceshash(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "gettokenbalanceshash ticker\n"
            "\nReturns a hash of the balances for the property.\n"
            "\nArguments:\n"
            "1. ticker                  (string, required) the ticker to hash balances for\n"
            "\nResult:\n"
            "{\n"
            "  \"block\" : nnnnnn,          (number) the index of the block this hash applies to\n"
            "  \"blockhash\" : \"hash\",    (string) the hash of the corresponding block\n"
            "  \"propertyid\" : nnnnnn,     (number) the property id of the hashed balances\n"
            "  \"balanceshash\" : \"hash\"  (string) the hash for the balances\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("gettokenbalanceshash", "TOKEN")
            + HelpExampleRpc("gettokenbalanceshash", "TOKEN")
        );

    LOCK(cs_main);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(request.params[0].get_str());
    if (!propertyId)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token not found");

    RequireExistingProperty(propertyId);

    int block = GetHeight();
    CBlockIndex* pblockindex = chainActive[block];
    uint256 blockHash = pblockindex->GetBlockHash();

    uint256 balancesHash = GetBalancesHash(propertyId);

    UniValue response(UniValue::VOBJ);
    response.pushKV("block", block);
    response.pushKV("blockhash", blockHash.GetHex());
    response.pushKV("propertyid", (uint64_t)propertyId);
    response.pushKV("balanceshash", balancesHash.GetHex());

    return response;
}

static const CRPCCommand commands[] =
{ //  category                  name                            actor (function)                       okSafeMode
  //  ------------------------- ------------------------------- -------------------------------------- ----------
    { "tokens (data retrieval)", "gettokensinfo",                   &gettokensinfo,                    true  },
    { "tokens (data retrieval)", "getalltokenbalances",             &getalltokenbalances,              false },
    { "tokens (data retrieval)", "gettokenbalance",                 &gettokenbalance,                  false },
    { "tokens (data retrieval)", "gettokentransaction",             &gettokentransaction,              false },
    { "tokens (data retrieval)", "gettoken",                        &gettoken,                         false },
    { "tokens (data retrieval)", "listtokens",                      &listtokens,                       false },
    { "tokens (data retrieval)", "gettokencrowdsale",               &gettokencrowdsale,                false },
    { "tokens (data retrieval)", "gettokengrants",                  &gettokengrants,                   false },
    { "tokens (data retrieval)", "gettokenactivedexsells",          &gettokenactivedexsells,           false },
    { "tokens (data retrieval)", "gettokenactivecrowdsales",        &gettokenactivecrowdsales,         false },
    { "tokens (data retrieval)", "gettokenorderbook",               &gettokenorderbook,                false },
    { "tokens (data retrieval)", "gettokentrade",                   &gettokentrade,                    false },
    // { "tokens (data retrieval)", "token_getsto",                    &token_getsto,                     false },
    { "tokens (data retrieval)", "listblocktokentransactions",      &listblocktokentransactions,       false },
    { "tokens (data retrieval)", "listblockstokentransactions",     &listblockstokentransactions,      false },
    { "tokens (data retrieval)", "listpendingtokentransactions",    &listpendingtokentransactions,     false },
    { "tokens (data retrieval)", "getalltokenbalancesforaddress",   &getalltokenbalancesforaddress,    false },
    { "tokens (data retrieval)", "gettokentradehistoryforaddress",  &gettokentradehistoryforaddress,   false },
    { "tokens (data retrieval)", "gettokentradehistoryforpair",     &gettokentradehistoryforpair,      false },
    { "tokens (data retrieval)", "gettokenconsensushash",           &gettokenconsensushash,            false },
    { "tokens (data retrieval)", "gettokenpayload",                 &gettokenpayload,                  false },
    { "tokens (data retrieval)", "gettokenseedblocks",              &gettokenseedblocks,               false },
    { "tokens (data retrieval)", "gettokenmetadexhash",             &gettokenmetadexhash,              false },
    // { "tokens (data retrieval)", "token_getfeecache",               &token_getfeecache,                false },
    // { "tokens (data retrieval)", "token_getfeetrigger",             &token_getfeetrigger,              false },
    // { "tokens (data retrieval)", "token_getfeedistribution",        &token_getfeedistribution,         false },
    // { "tokens (data retrieval)", "token_getfeedistributions",       &token_getfeedistributions,        false },
    { "tokens (data retrieval)", "gettokenbalanceshash",            &gettokenbalanceshash,             false },
#ifdef ENABLE_WALLET
    { "tokens (data retrieval)", "listtokentransactions",           &listtokentransactions,            false },
    // { "tokens (data retrieval)", "token_getfeeshare",               &token_getfeeshare,                false },
    // { "tokens (configuration)",  "token_setautocommit",             &token_setautocommit,              true  },
    { "tokens (data retrieval)", "getwallettokenbalances",          &getwallettokenbalances,           false },
    { "tokens (data retrieval)", "getwalletaddresstokenbalances",   &getwalletaddresstokenbalances,    false },
    { "tokens (data retrieval)", "gettokenbalances",                &gettokenbalances,                 false },
#endif
};

void RegisterTokenDataRetrievalRPCCommands()
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
