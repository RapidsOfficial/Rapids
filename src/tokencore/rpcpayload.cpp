#include "tokencore/rpcpayload.h"

#include "tokencore/createpayload.h"
#include "tokencore/rpcvalues.h"
#include "tokencore/rpcrequirements.h"
#include "tokencore/tokencore.h"
#include "tokencore/sp.h"
#include "tokencore/tx.h"

#include "rpc/server.h"
#include "utilstrencodings.h"

#include <univalue.h>

using std::runtime_error;
using namespace mastercore;

static UniValue createtokenpayloadsimplesend(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "createtokenpayloadsimplesend ticker \"amount\"\n"

            "\nCreate the payload for a simple send transaction.\n"

            "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n"

            "\nArguments:\n"
            "1. ticker               (string, required) the ticker of the token to send\n"
            "2. amount               (string, required) the amount to send\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("createtokenpayloadsimplesend", "TICKER \"100.0\"")
            + HelpExampleRpc("createtokenpayloadsimplesend", "TICKER, \"100.0\"")
        );

    std::string ticker = ParseText(request.params[0]);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(ticker);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

static UniValue token_createpayload_sendall(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "token_createpayload_sendall ecosystem\n"

            "\nCreate the payload for a send all transaction.\n"

            "\nArguments:\n"
            "1. ecosystem              (number, required) the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)\n"

            "\nResult:\n"
            "\"payload\"               (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_sendall", "2")
            + HelpExampleRpc("token_createpayload_sendall", "2")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_SendAll(ecosystem);

    return HexStr(payload.begin(), payload.end());
}

static UniValue token_createpayload_dexsell(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 6)
        throw runtime_error(
            "token_createpayload_dexsell propertyidforsale \"amountforsale\" \"amountdesired\" paymentwindow minacceptfee action\n"

            "\nCreate a payload to place, update or cancel a sell offer on the traditional distributed TOKEN/RPD exchange.\n"

            "\nArguments:\n"

            "1. propertyidforsale    (number, required) the identifier of the tokens to list for sale (must be 1 for OMN or 2 for TOMN)\n"
            "2. amountforsale        (string, required) the amount of tokens to list for sale\n"
            "3. amountdesired        (string, required) the amount of bitcoins desired\n"
            "4. paymentwindow        (number, required) a time limit in blocks a buyer has to pay following a successful accepting order\n"
            "5. minacceptfee         (string, required) a minimum mining fee a buyer has to pay to accept the offer\n"
            "6. action               (number, required) the action to take (1 for new offers, 2 to update\", 3 to cancel)\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_dexsell", "1 \"1.5\" \"0.75\" 25 \"0.0005\" 1")
            + HelpExampleRpc("token_createpayload_dexsell", "1, \"1.5\", \"0.75\", 25, \"0.0005\", 1")
        );

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    uint8_t action = ParseDExAction(request.params[5]);

    int64_t amountForSale = 0; // depending on action
    int64_t amountDesired = 0; // depending on action
    uint8_t paymentWindow = 0; // depending on action
    int64_t minAcceptFee = 0;  // depending on action

    if (action <= CMPTransaction::UPDATE) { // actions 3 permit zero values, skip check
        amountForSale = ParseAmount(request.params[1], isPropertyDivisible(propertyIdForSale));
        amountDesired = ParseAmount(request.params[2], true); // RPD is divisible
        paymentWindow = ParseDExPaymentWindow(request.params[3]);
        minAcceptFee = ParseDExFee(request.params[4]);
    }

    std::vector<unsigned char> payload = CreatePayload_DExSell(propertyIdForSale, amountForSale, amountDesired, paymentWindow, minAcceptFee, action);

    return HexStr(payload.begin(), payload.end());
}

static UniValue token_createpayload_dexaccept(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "token_createpayload_dexaccept propertyid \"amount\"\n"

            "\nCreate the payload for an accept offer for the specified token and amount.\n"

            "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the token to purchase\n"
            "2. amount               (string, required) the amount to accept\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_dexaccept", "1 \"15.0\"")
            + HelpExampleRpc("token_createpayload_dexaccept", "1, \"15.0\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_DExAccept(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

static UniValue token_createpayload_sto(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw runtime_error(
            "token_createpayload_sto propertyid \"amount\" ( distributionproperty )\n"

            "\nCreates the payload for a send-to-owners transaction.\n"

            "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n"

            "\nArguments:\n"
            "1. propertyid             (number, required) the identifier of the tokens to distribute\n"
            "2. amount                 (string, required) the amount to distribute\n"
            "3. distributionproperty   (number, optional) the identifier of the property holders to distribute to\n"
            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_sto", "3 \"5000\"")
            + HelpExampleRpc("token_createpayload_sto", "3, \"5000\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
    uint32_t distributionPropertyId = (request.params.size() > 2) ? ParsePropertyId(request.params[2]) : propertyId;

    std::vector<unsigned char> payload = CreatePayload_SendToOwners(propertyId, amount, distributionPropertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue createtokenpayloadissuancefixed(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 10)
        throw runtime_error(
            "createtokenpayloadissuancefixed ecosystem type previousid \"category\" \"subcategory\" \"name\" \"ticker\" \"url\" \"data\" \"amount\"\n"

            "\nCreates the payload for a new tokens issuance with fixed supply.\n"

            "\nArguments:\n"
            "1. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "4. category             (string, required) a category for the new tokens (can be \"\")\n"
            "5. subcategory          (string, required) a subcategory for the new tokens  (can be \"\")\n"
            "6. name                 (string, required) the name of the new tokens to create\n"
            "7. ticker               (string, required) the ticker of the new tokens to create\n"
            "8. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "9. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "10. amount              (string, required) the number of tokens to create\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("createtokenpayloadissuancefixed", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"TICKER\" \"\" \"\" \"1000000\"")
            + HelpExampleRpc("createtokenpayloadissuancefixed", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"TICKER\", \"\", \"\", \"1000000\"")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string category = ParseText(request.params[3]);
    std::string subcategory = ParseText(request.params[4]);
    std::string name = ParseText(request.params[5]);
    std::string ticker = ParseText(request.params[6]);
    std::string url = ParseText(request.params[7]);
    std::string data = ParseText(request.params[8]);
    int64_t amount = ParseAmount(request.params[9], type);

    RequirePropertyName(name);
    RequirePropertyName(ticker);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(ticker);
    if (propertyId > 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker already exists");

    if (!IsTokenTickerValid(ticker))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token ticker is invalid");

    if (!IsTokenIPFSValid(data))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token IPFS is invalid");

    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(ecosystem, type, previousId, category, subcategory, name, ticker, url, data, amount);

    return HexStr(payload.begin(), payload.end());
}

static UniValue createtokenpayloadissuancecrowdsale(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 13)
        throw runtime_error(
            "createtokenpayloadissuancecrowdsale ecosystem type previousid \"category\" \"subcategory\" \"name\" \"ticker\" \"url\" \"data\" tokensperunit deadline earlybonus issuerpercentage\n"

            "\nCreates the payload for a new tokens issuance with crowdsale.\n"

            "\nArguments:\n"
            "1. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (0 for new crowdsales)\n"
            "4. category             (string, required) a category for the new tokens (can be \"\")\n"
            "5. subcategory          (string, required) a subcategory for the new tokens  (can be \"\")\n"
            "6. name                 (string, required) the name of the new tokens to create\n"
            "7. ticker               (string, required) the ticker of the new tokens to create\n"
            "8. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "9. data                 (string, required) a description for the new tokens (can be \"\")\n"
            
            "10. tokendesired        (string, required) the token ticker eligible to participate in the crowdsale\n"
            
            "11. tokensperunit       (string, required) the amount of tokens granted per unit invested in the crowdsale\n"
            "12. deadline            (number, required) the deadline of the crowdsale as Unix timestamp\n"
            "13. earlybonus          (number, required) an early bird bonus for participants in percent per week\n"
            "14. issuerpercentage    (number, required) a percentage of tokens that will be granted to the issuer\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("createtokenpayloadissuancecrowdsale", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"TICKER\" \"\" \"\" \"RPD\" \"100\" 1483228800 30 2")
            + HelpExampleRpc("createtokenpayloadissuancecrowdsale", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"TICKER\", \"\", \"\", \"RPD\", \"100\", 1483228800, 30, 2")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string category = ParseText(request.params[3]);
    std::string subcategory = ParseText(request.params[4]);
    std::string name = ParseText(request.params[5]);
    std::string ticker = ParseText(request.params[6]);
    std::string url = ParseText(request.params[7]);
    std::string data = ParseText(request.params[8]);

    std::string desiredToken = ParseText(request.params[9]);

    int64_t numTokens = ParseAmount(request.params[10], type);
    int64_t deadline = ParseDeadline(request.params[11]);
    uint8_t earlyBonus = ParseEarlyBirdBonus(request.params[12]);
    uint8_t issuerPercentage = ParseIssuerBonus(request.params[13]);

    RequirePropertyName(name);
    RequirePropertyName(ticker);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(ticker);
    if (propertyId > 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker already exists");

    uint32_t propertyIdDesired = pDbSpInfo->findSPByTicker(desiredToken);
    if (desiredToken != "RPD" && propertyIdDesired == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Desired token not found");

    if (!IsTokenTickerValid(ticker))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token ticker is invalid");

    if (!IsTokenIPFSValid(data))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token IPFS is invalid");

    std::vector<unsigned char> payload = CreatePayload_IssuanceVariable(ecosystem, type, previousId, category, subcategory, name, ticker, url, data, propertyIdDesired, numTokens, deadline, earlyBonus, issuerPercentage);

    return HexStr(payload.begin(), payload.end());
}

static UniValue createtokenpayloadissuancemanaged(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 9)
        throw runtime_error(
            "createtokenpayloadissuancemanaged ecosystem type previousid \"category\" \"subcategory\" \"name\" \"ticker\" \"url\" \"data\"\n"

            "\nCreates the payload for a new tokens issuance with manageable supply.\n"

            "\nArguments:\n"
            "1. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "4. category             (string, required) a category for the new tokens (can be \"\")\n"
            "5. subcategory          (string, required) a subcategory for the new tokens  (can be \"\")\n"
            "6. name                 (string, required) the name of the new tokens to create\n"
            "7. ticker               (string, required) the ticker of the new tokens to create\n"
            "8. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "9. data                 (string, required) a description for the new tokens (can be \"\")\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("createtokenpayloadissuancemanaged", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"TICKER\" \"\" \"\"")
            + HelpExampleRpc("createtokenpayloadissuancemanaged", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"TICKER\", \"\", \"\"")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string category = ParseText(request.params[3]);
    std::string subcategory = ParseText(request.params[4]);
    std::string name = ParseText(request.params[5]);
    std::string ticker = ParseText(request.params[6]);
    std::string url = ParseText(request.params[7]);
    std::string data = ParseText(request.params[8]);

    RequirePropertyName(name);
    RequirePropertyName(ticker);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(ticker);
    if (propertyId > 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker already exists");

    if (!IsTokenTickerValid(ticker))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token ticker is invalid");

    if (!IsTokenIPFSValid(data))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token IPFS is invalid");

    std::vector<unsigned char> payload = CreatePayload_IssuanceManaged(ecosystem, type, previousId, category, subcategory, name, ticker, url, data);

    return HexStr(payload.begin(), payload.end());
}

static UniValue createtokenpayloadclosecrowdsale(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "createtokenpayloadclosecrowdsale ticker\n"

            "\nCreates the payload to manually close a crowdsale.\n"

            "\nArguments:\n"
            "1. ticker               (string, required) the ticker of the crowdsale to close\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("createtokenpayloadclosecrowdsale", "TICKER")
            + HelpExampleRpc("createtokenpayloadclosecrowdsale", "TICKER")
        );

    std::string ticker = ParseText(request.params[0]);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(ticker);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

    std::vector<unsigned char> payload = CreatePayload_CloseCrowdsale(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue createtokenpayloadgrant(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw runtime_error(
            "createtokenpayloadgrant ticker \"amount\" ( \"memo\" )\n"

            "\nCreates the payload to issue or grant new units of managed tokens.\n"

            "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n"

            "\nArguments:\n"
            "1. ticker               (string, required) the ticker of the token to grant\n"
            "2. amount               (string, required) the amount of tokens to create\n"
            "3. memo                 (string, optional) a text note attached to this transaction (none by default)\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("createtokenpayloadgrant", "TICKER \"7000\"")
            + HelpExampleRpc("createtokenpayloadgrant", "TICKER, \"7000\"")
        );

    std::string ticker = ParseText(request.params[0]);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(ticker);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
    std::string memo = (request.params.size() > 2) ? ParseText(request.params[2]): "";

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);

    std::vector<unsigned char> payload = CreatePayload_Grant(propertyId, amount, memo);

    return HexStr(payload.begin(), payload.end());
}

static UniValue createtokenpayloadrevoke(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw runtime_error(
            "createtokenpayloadrevoke ticker \"amount\" ( \"memo\" )\n"

            "\nCreates the payload to revoke units of managed tokens.\n"

            "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n"

            "\nArguments:\n"
            "1. ticker               (string, required) the ticker of the token to revoke\n"
            "2. amount               (string, required) the amount of tokens to revoke\n"
            "3. memo                 (string, optional) a text note attached to this transaction (none by default)\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("createtokenpayloadrevoke", "TICKER \"100\"")
            + HelpExampleRpc("createtokenpayloadrevoke", "TICKER, \"100\"")
        );

    std::string ticker = ParseText(request.params[0]);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(ticker);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
    std::string memo = (request.params.size() > 2) ? ParseText(request.params[2]): "";

    std::vector<unsigned char> payload = CreatePayload_Revoke(propertyId, amount, memo);

    return HexStr(payload.begin(), payload.end());
}

static UniValue createtokenpayloadchangeissuer(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "createtokenpayloadchangeissuer ticker\n"

            "\nCreates the payload to change the issuer on record of the given tokens.\n"

            "\nArguments:\n"
            "1. ticker               (string, required) the ticker of the token\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("createtokenpayloadchangeissuer", "TICKER")
            + HelpExampleRpc("createtokenpayloadchangeissuer", "TICKER")
        );

    std::string ticker = ParseText(request.params[0]);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(ticker);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

    std::vector<unsigned char> payload = CreatePayload_ChangeIssuer(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue token_createpayload_trade(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "token_createpayload_trade propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\"\n"

            "\nCreates the payload to place a trade offer on the distributed token exchange.\n"

            "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n"

            "\nArguments:\n"
            "1. propertyidforsale    (number, required) the identifier of the tokens to list for sale\n"
            "2. amountforsale        (string, required) the amount of tokens to list for sale\n"
            "3. propertiddesired     (number, required) the identifier of the tokens desired in exchange\n"
            "4. amountdesired        (string, required) the amount of tokens desired in exchange\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_trade", "31 \"250.0\" 1 \"10.0\"")
            + HelpExampleRpc("token_createpayload_trade", "31, \"250.0\", 1, \"10.0\"")
        );

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    int64_t amountForSale = ParseAmount(request.params[1], isPropertyDivisible(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(request.params[2]);
    int64_t amountDesired = ParseAmount(request.params[3], isPropertyDivisible(propertyIdDesired));
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_MetaDExTrade(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

    return HexStr(payload.begin(), payload.end());
}

static UniValue token_createpayload_canceltradesbyprice(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "token_createpayload_canceltradesbyprice propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\"\n"

            "\nCreates the payload to cancel offers on the distributed token exchange with the specified price.\n"

            "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n"

            "\nArguments:\n"
            "1. propertyidforsale    (number, required) the identifier of the tokens listed for sale\n"
            "2. amountforsale        (string, required) the amount of tokens to listed for sale\n"
            "3. propertiddesired     (number, required) the identifier of the tokens desired in exchange\n"
            "4. amountdesired        (string, required) the amount of tokens desired in exchange\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_canceltradesbyprice", "31 \"100.0\" 1 \"5.0\"")
            + HelpExampleRpc("token_createpayload_canceltradesbyprice", "31, \"100.0\", 1, \"5.0\"")
        );

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    int64_t amountForSale = ParseAmount(request.params[1], isPropertyDivisible(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(request.params[2]);
    int64_t amountDesired = ParseAmount(request.params[3], isPropertyDivisible(propertyIdDesired));
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPrice(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

    return HexStr(payload.begin(), payload.end());
}

static UniValue token_createpayload_canceltradesbypair(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "token_createpayload_canceltradesbypair propertyidforsale propertiddesired\n"

            "\nCreates the payload to cancel all offers on the distributed token exchange with the given currency pair.\n"

            "\nArguments:\n"
            "1. propertyidforsale    (number, required) the identifier of the tokens listed for sale\n"
            "2. propertiddesired     (number, required) the identifier of the tokens desired in exchange\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_canceltradesbypair", "1 31")
            + HelpExampleRpc("token_createpayload_canceltradesbypair", "1, 31")
        );

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    uint32_t propertyIdDesired = ParsePropertyId(request.params[1]);
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPair(propertyIdForSale, propertyIdDesired);

    return HexStr(payload.begin(), payload.end());
}

static UniValue token_createpayload_cancelalltrades(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "token_createpayload_cancelalltrades ecosystem\n"

            "\nCreates the payload to cancel all offers on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_cancelalltrades", "1")
            + HelpExampleRpc("token_createpayload_cancelalltrades", "1")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelEcosystem(ecosystem);

    return HexStr(payload.begin(), payload.end());
}

static UniValue token_createpayload_enablefreezing(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "token_createpayload_enablefreezing propertyid\n"

            "\nCreates the payload to enable address freezing for a centrally managed property.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_enablefreezing", "3")
            + HelpExampleRpc("token_createpayload_enablefreezing", "3")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_EnableFreezing(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue token_createpayload_disablefreezing(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "token_createpayload_disablefreezing propertyid\n"

            "\nCreates the payload to disable address freezing for a centrally managed property.\n"
            "\nIMPORTANT NOTE:  Disabling freezing for a property will UNFREEZE all frozen addresses for that property!"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_disablefreezing", "3")
            + HelpExampleRpc("token_createpayload_disablefreezing", "3")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_DisableFreezing(propertyId);

    return HexStr(payload.begin(), payload.end());
}


static UniValue token_createpayload_freeze(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "token_createpayload_freeze \"toaddress\" propertyid amount \n"

            "\nCreates the payload to freeze an address for a centrally managed token.\n"

            "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n"

            "\nArguments:\n"
            "1. toaddress            (string, required) the address to freeze tokens for\n"
            "2. propertyid           (number, required) the property to freeze tokens for (must be managed type and have freezing option enabled)\n"
            "3. amount               (string, required) the amount of tokens to freeze (note: this is unused - once frozen an address cannot send any transactions)\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_freeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 1 \"100\"")
            + HelpExampleRpc("token_createpayload_freeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 1, \"100\"")
        );

    std::string refAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_FreezeTokens(propertyId, amount, refAddress);

    return HexStr(payload.begin(), payload.end());
}

static UniValue token_createpayload_unfreeze(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "token_createpayload_unfreeze \"toaddress\" propertyid amount \n"

            "\nCreates the payload to unfreeze an address for a centrally managed token.\n"

            "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n"

            "\nArguments:\n"
            "1. toaddress            (string, required) the address to unfreeze tokens for\n"
            "2. propertyid           (number, required) the property to unfreeze tokens for (must be managed type and have freezing option enabled)\n"
            "3. amount               (string, required) the amount of tokens to unfreeze (note: this is unused)\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("token_createpayload_unfreeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 1 \"100\"")
            + HelpExampleRpc("token_createpayload_unfreeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 1, \"100\"")
        );

    std::string refAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_UnfreezeTokens(propertyId, amount, refAddress);

    return HexStr(payload.begin(), payload.end());
}



static UniValue omni_createpayload_sendtomany(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "omni_createpayload_sendtomany\n"
        );

    // uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::string ticker = ParseText(request.params[0]);

    uint32_t propertyId = pDbSpInfo->findSPByTicker(ticker);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this ticker doesn't exists");

    std::vector<std::tuple<uint8_t, uint64_t>> outputValues;

    for (unsigned int idx = 0; idx < request.params[1].size(); idx++) {
        const UniValue& input = request.params[1][idx];
        const UniValue& o = input.get_obj();

        const UniValue& uvOutput = find_value(o, "output");
        const UniValue& uvAmount = find_value(o, "amount");

        uint8_t output = uvOutput.get_int();
        uint64_t amount = ParseAmount(uvAmount, isPropertyDivisible(propertyId));

        outputValues.push_back(std::make_tuple(output, amount));
    }

    std::vector<unsigned char> payload = CreatePayload_SendToMany(
        propertyId,
        outputValues);           

    return HexStr(payload.begin(), payload.end());
}


static const CRPCCommand commands[] =
{ //  category                         name                                      actor (function)                         okSafeMode
  //  -------------------------------- ----------------------------------------- ---------------------------------------- ----------
    { "token layer (payload creation)", "createtokenpayloadsimplesend",          &createtokenpayloadsimplesend,          true },
    { "token layer (payload creation)", "token_createpayload_sendall",             &token_createpayload_sendall,             true },
    { "token layer (payload creation)", "token_createpayload_dexsell",             &token_createpayload_dexsell,             true },
    { "token layer (payload creation)", "token_createpayload_dexaccept",           &token_createpayload_dexaccept,           true },
    { "token layer (payload creation)", "token_createpayload_sto",                 &token_createpayload_sto,                 true },
    { "token layer (payload creation)", "createtokenpayloadgrant",               &createtokenpayloadgrant,               true },
    { "token layer (payload creation)", "createtokenpayloadrevoke",              &createtokenpayloadrevoke,              true },
    { "token layer (payload creation)", "createtokenpayloadchangeissuer",        &createtokenpayloadchangeissuer,        true },
    { "token layer (payload creation)", "token_createpayload_trade",               &token_createpayload_trade,               true },
    { "token layer (payload creation)", "createtokenpayloadissuancefixed",       &createtokenpayloadissuancefixed,       true },
    { "token layer (payload creation)", "createtokenpayloadissuancecrowdsale",   &createtokenpayloadissuancecrowdsale,   true },
    { "token layer (payload creation)", "createtokenpayloadissuancemanaged",     &createtokenpayloadissuancemanaged,     true },
    { "token layer (payload creation)", "createtokenpayloadclosecrowdsale",      &createtokenpayloadclosecrowdsale,      true },
    { "token layer (payload creation)", "token_createpayload_canceltradesbyprice", &token_createpayload_canceltradesbyprice, true },
    { "token layer (payload creation)", "token_createpayload_canceltradesbypair",  &token_createpayload_canceltradesbypair,  true },
    { "token layer (payload creation)", "token_createpayload_cancelalltrades",     &token_createpayload_cancelalltrades,     true },
    { "token layer (payload creation)", "token_createpayload_enablefreezing",      &token_createpayload_enablefreezing,      true },
    { "token layer (payload creation)", "token_createpayload_disablefreezing",     &token_createpayload_disablefreezing,     true },
    { "token layer (payload creation)", "token_createpayload_freeze",              &token_createpayload_freeze,              true },
    { "token layer (payload creation)", "token_createpayload_unfreeze",            &token_createpayload_unfreeze,            true },
    { "token layer (payload creation)", "omni_createpayload_sendtomany",           &omni_createpayload_sendtomany,            true },
};

void RegisterTokenPayloadCreationRPCCommands()
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
