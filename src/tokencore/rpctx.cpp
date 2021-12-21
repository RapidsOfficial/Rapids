/**
 * @file rpctx.cpp
 *
 * This file contains RPC calls for creating and sending Token transactions.
 */

#include "tokencore/rpctx.h"

#include "tokencore/createpayload.h"
#include "tokencore/dex.h"
#include "tokencore/errors.h"
#include "tokencore/tokencore.h"
#include "tokencore/pending.h"
#include "tokencore/rpcrequirements.h"
#include "tokencore/rpcvalues.h"
#include "tokencore/sp.h"
#include "tokencore/tx.h"
#include "tokencore/wallettxbuilder.h"
#include "tokencore/utilsbitcoin.h"

#include "init.h"
#include "main.h"
#include "rpc/server.h"
#include "sync.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <univalue.h>

#include <stdint.h>
#include <stdexcept>
#include <string>

using std::runtime_error;
using namespace mastercore;


static UniValue sendtokenfunded(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 5)
        throw runtime_error(
            "sendtokenfunded \"fromaddress\" \"toaddress\" tokenname \"amount\" \"feeaddress\"\n"

            "\nCreates and sends a funded simple send transaction.\n"

            "\nAll bitcoins from the sender are consumed and if there are bitcoins missing, they are taken from the specified fee source. Change is sent to the fee source!\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send the tokens from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. tokenname            (string, required) the name of token to send\n"
            "4. amount               (string, required) the amount to send\n"
            "5. feeaddress           (string, required) the address that is used for change and to pay for fees, if needed\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokenfunded", "\"1DFa5bT6KMEr6ta29QJouainsjaNBsJQhH\" \"15cWrfuvMxyxGst2FisrQcvcpF48x6sXoH\" TOKEN \"100.0\" \"15Jhzz4omEXEyFKbdcccJwuVPea5LqsKM1\"")
            + HelpExampleRpc("sendtokenfunded", "\"1DFa5bT6KMEr6ta29QJouainsjaNBsJQhH\", \"15cWrfuvMxyxGst2FisrQcvcpF48x6sXoH\", TOKEN, \"100.0\", \"15Jhzz4omEXEyFKbdcccJwuVPea5LqsKM1\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    std::string name = ParseText(request.params[2]);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));
    std::string feeAddress = ParseAddress(request.params[4]);

    // perform checks
    RequireExistingProperty(propertyId);
    RequireBalance(fromAddress, propertyId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    // create the raw transaction
    uint256 retTxid;
    int result = CreateFundedTransaction(fromAddress, toAddress, feeAddress, payload, retTxid);
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    }

    return retTxid.ToString();
}

static UniValue sendtokenfundedall(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "sendtokenfundedall \"fromaddress\" \"toaddress\" ecosystem \"feeaddress\"\n"

            "\nCreates and sends a transaction that transfers all available tokens in the given ecosystem to the recipient.\n"

            "\nAll bitcoins from the sender are consumed and if there are bitcoins missing, they are taken from the specified fee source. Change is sent to the fee source!\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to the tokens send from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. ecosystem            (number, required) the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)\n"
            "4. feeaddress           (string, required) the address that is used for change and to pay for fees, if needed\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokenfundedall", "\"1DFa5bT6KMEr6ta29QJouainsjaNBsJQhH\" \"15cWrfuvMxyxGst2FisrQcvcpF48x6sXoH\" 1 \"15Jhzz4omEXEyFKbdcccJwuVPea5LqsKM1\"")
            + HelpExampleRpc("sendtokenfundedall", "\"1DFa5bT6KMEr6ta29QJouainsjaNBsJQhH\", \"15cWrfuvMxyxGst2FisrQcvcpF48x6sXoH\", 1, \"15Jhzz4omEXEyFKbdcccJwuVPea5LqsKM1\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint8_t ecosystem = ParseEcosystem(request.params[2]);
    std::string feeAddress = ParseAddress(request.params[3]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendAll(ecosystem);

    // create the raw transaction
    uint256 retTxid;
    int result = CreateFundedTransaction(fromAddress, toAddress, feeAddress, payload, retTxid);
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    }

    return retTxid.ToString();
}

static UniValue sendtokenrawtx(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 5)
        throw runtime_error(
            "sendtokenrawtx \"fromaddress\" \"rawtransaction\" ( \"referenceaddress\" \"redeemaddress\" \"referenceamount\" )\n"
            "\nBroadcasts a raw RPDx transaction.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. rawtransaction       (string, required) the hex-encoded raw transaction\n"
            "3. referenceaddress     (string, optional) a reference address (none by default)\n"
            "4. redeemaddress        (string, optional) an address that can spent the transaction dust (sender by default)\n"
            "5. referenceamount      (string, optional) a bitcoin amount that is sent to the receiver (minimal by default)\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtokenrawtx", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\" \"000000000000000100000000017d7840\" \"1EqTta1Rt8ixAA32DuC29oukbsSWU62qAV\"")
            + HelpExampleRpc("sendtokenrawtx", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\", \"000000000000000100000000017d7840\", \"1EqTta1Rt8ixAA32DuC29oukbsSWU62qAV\"")
        );

    std::string fromAddress = ParseAddress(request.params[0]);
    std::vector<unsigned char> data = ParseHexV(request.params[1], "raw transaction");
    std::string toAddress = (request.params.size() > 2) ? ParseAddressOrEmpty(request.params[2]): "";
    std::string redeemAddress = (request.params.size() > 3) ? ParseAddressOrEmpty(request.params[3]): "";
    int64_t referenceAmount = (request.params.size() > 4) ? ParseAmount(request.params[4], true): 0;

    //some sanity checking of the data supplied?
    uint256 newTX;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, redeemAddress, referenceAmount, data, newTX, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return newTX.GetHex();
        }
    }
}

static UniValue sendtoken(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 4 || request.params.size() > 6)
        throw runtime_error(
            "sendtoken \"fromaddress\" \"toaddress\" name \"amount\" ( \"redeemaddress\" \"referenceamount\" )\n"

            "\nCreate and broadcast a simple send transaction.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. name                 (string, required) the name of the token to send\n"
            "4. amount               (string, required) the amount to send\n"
            "5. redeemaddress        (string, optional) an address that can spend the transaction dust (sender by default)\n"
            "6. referenceamount      (string, optional) a bitcoin amount that is sent to the receiver (minimal by default)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtoken", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" TOKEN \"100.0\"")
            + HelpExampleRpc("sendtoken", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", TOKEN, \"100.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    std::string name = ParseText(request.params[2]);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));
    std::string redeemAddress = (request.params.size() > 4 && !ParseText(request.params[4]).empty()) ? ParseAddress(request.params[4]): "";
    int64_t referenceAmount = (request.params.size() > 5) ? ParseAmount(request.params[5], true): 0;

    // perform checks
    RequireExistingProperty(propertyId);
    RequireBalance(fromAddress, propertyId, amount);
    RequireSaneReferenceAmount(referenceAmount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, redeemAddress, referenceAmount, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, TOKEN_TYPE_SIMPLE_SEND, propertyId, amount);
            return txid.GetHex();
        }
    }
}

static UniValue sendalltokens(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 3 || request.params.size() > 5)
        throw runtime_error(
            "sendalltokens \"fromaddress\" \"toaddress\" ecosystem ( \"redeemaddress\" \"referenceamount\" )\n"

            "\nTransfers all available tokens in the given ecosystem to the recipient.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. ecosystem            (number, required) the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)\n"
            "4. redeemaddress        (string, optional) an address that can spend the transaction dust (sender by default)\n"
            "5. referenceamount      (string, optional) a bitcoin amount that is sent to the receiver (minimal by default)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendalltokens", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 2")
            + HelpExampleRpc("sendalltokens", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 2")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint8_t ecosystem = ParseEcosystem(request.params[2]);
    std::string redeemAddress = (request.params.size() > 3 && !ParseText(request.params[3]).empty()) ? ParseAddress(request.params[3]): "";
    int64_t referenceAmount = (request.params.size() > 4) ? ParseAmount(request.params[4], true): 0;

    // perform checks
    RequireSaneReferenceAmount(referenceAmount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendAll(ecosystem);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, redeemAddress, referenceAmount, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            // TODO: pending
            return txid.GetHex();
        }
    }
}

static UniValue sendtokendexsell(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 7)
        throw runtime_error(
            "sendtokendexsell \"fromaddress\" tokenforsale \"amountforsale\" \"amountdesired\" paymentwindow minacceptfee action\n"

            "\nPlace, update or cancel a sell offer on the traditional distributed TOKEN/RPD exchange.\n"

            "\nArguments:\n"

            "1. fromaddress          (string, required) the address to send from\n"
            "2. tokenforsale         (string, required) the name of token to list for sale\n"
            "3. amountforsale        (string, required) the amount of tokens to list for sale\n"
            "4. amountdesired        (string, required) the amount of Rapids desired\n"
            "5. paymentwindow        (number, required) a time limit in blocks a buyer has to pay following a successful accepting order\n"
            "6. minacceptfee         (string, required) a minimum mining fee a buyer has to pay to accept the offer\n"
            "7. action               (number, required) the action to take (1 for new offers, 2 to update\", 3 to cancel)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tokenforsale", "\"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" TOKEN \"1.5\" \"0.75\" 25 \"0.0005\" 1")
            + HelpExampleRpc("tokenforsale", "\"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", TOKEN, \"1.5\", \"0.75\", 25, \"0.0005\", 1")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string tokenForSale = ParseText(request.params[1]);
    int64_t amountForSale = 0; // depending on action
    int64_t amountDesired = 0; // depending on action
    uint8_t paymentWindow = 0; // depending on action
    int64_t minAcceptFee = 0;  // depending on action
    uint8_t action = ParseDExAction(request.params[6]);

    uint32_t propertyIdForSale = pDbSpInfo->findSPByName(tokenForSale);
    if (propertyIdForSale == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    // perform conversions
    if (action <= CMPTransaction::UPDATE) { // actions 3 permit zero values, skip check
        amountForSale = ParseAmount(request.params[2], isPropertyDivisible(propertyIdForSale));
        amountDesired = ParseAmount(request.params[3], true); // RPD is divisible
        paymentWindow = ParseDExPaymentWindow(request.params[4]);
        minAcceptFee = ParseDExFee(request.params[5]);
    }

    // perform checks
    switch (action) {
        case CMPTransaction::NEW:
        {
            RequireBalance(fromAddress, propertyIdForSale, amountForSale);
            RequireNoOtherDExOffer(fromAddress);
            break;
        }
        case CMPTransaction::UPDATE:
        {
            RequireBalance(fromAddress, propertyIdForSale, amountForSale);
            RequireMatchingDExOffer(fromAddress, propertyIdForSale);
            break;
        }
        case CMPTransaction::CANCEL:
        {
            RequireMatchingDExOffer(fromAddress, propertyIdForSale);
            break;
        }
    }

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DExSell(propertyIdForSale, amountForSale, amountDesired, paymentWindow, minAcceptFee, action);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            bool fSubtract = (action <= CMPTransaction::UPDATE); // no pending balances for cancels
            PendingAdd(txid, fromAddress, TOKEN_TYPE_TRADE_OFFER, propertyIdForSale, amountForSale, fSubtract);
            return txid.GetHex();
        }
    }
}

static UniValue sendtokendexaccept(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 4 || request.params.size() > 5)
        throw runtime_error(
            "sendtokendexaccept \"fromaddress\" \"toaddress\" tokenname \"amount\" ( override )\n"

            "\nCreate and broadcast an accept offer for the specified token and amount.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the seller\n"
            "3. tokenname            (string, required) the name of token to purchase\n"
            "4. amount               (string, required) the amount to accept\n"
            "5. override             (boolean, optional) override minimum accept fee and payment window checks (use with caution!)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokendexaccept", "\"35URq1NN3xL6GeRKUP6vzaQVcxoJiiJKd8\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" TOKEN \"15.0\"")
            + HelpExampleRpc("sendtokendexaccept", "\"35URq1NN3xL6GeRKUP6vzaQVcxoJiiJKd8\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", TOKEN, \"15.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    std::string name = ParseText(request.params[2]);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));
    bool override = (request.params.size() > 4) ? request.params[4].get_bool(): false;

    // perform checks
    RequireMatchingDExOffer(toAddress, propertyId);

    if (!override) { // reject unsafe accepts - note client maximum tx fee will always be respected regardless of override here
        RequireSaneDExFee(toAddress, propertyId);
        RequireSaneDExPaymentWindow(toAddress, propertyId);
    }

#ifdef ENABLE_WALLET
    // use new 0.10 custom fee to set the accept minimum fee appropriately
    int64_t nMinimumAcceptFee = 0;
    {
        LOCK(cs_tally);
        const CMPOffer* sellOffer = DEx_getOffer(toAddress, propertyId);
        if (sellOffer == nullptr) throw JSONRPCError(RPC_TYPE_ERROR, "Unable to load sell offer from the distributed exchange");
        nMinimumAcceptFee = sellOffer->getMinFee();
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // temporarily update the global transaction fee to pay enough for the accept fee
    CFeeRate payTxFeeOriginal = payTxFee;
    payTxFee = CFeeRate(nMinimumAcceptFee, 225); // TODO: refine!
    // fPayAtLeastCustomFee = true;
#endif

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DExAccept(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, "", 0, payload, txid, rawHex, autoCommit);

#ifdef ENABLE_WALLET
    // set the custom fee back to original
    payTxFee = payTxFeeOriginal;
#endif

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue sendtokenissuancecrowdsale(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 14)
        throw runtime_error(
            "sendtokenissuancecrowdsale \"fromaddress\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\" tokendesired tokensperunit deadline ( earlybonus issuerpercentage )\n"

            "Create new tokens as crowdsale."

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "4. previousid           (number, required) an identifier of a predecessor token (0 for new crowdsales)\n"
            "5. category             (string, required) a category for the new tokens (can be \"\")\n"
            "6. subcategory          (string, required) a subcategory for the new tokens  (can be \"\")\n"
            "7. name                 (string, required) the name of the new tokens to create\n"
            "8. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "9. ipfs                 (string, required) IPFS string for the new tokens (can be \"\")\n"
            "10. tokendesired        (string, required) the token name eligible to participate in the crowdsale\n"
            "11. tokensperunit       (string, required) the amount of tokens granted per unit invested in the crowdsale\n"
            "12. deadline            (number, required) the deadline of the crowdsale as Unix timestamp\n"
            "13. earlybonus          (number, required) an early bird bonus for participants in percent per week\n"
            "14. issuerpercentage    (number, required) a percentage of tokens that will be granted to the issuer\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokenissuancecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" TOKEN \"100\" 1483228800 30 2")
            + HelpExampleRpc("sendtokenissuancecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", TOKEN, \"100\", 1483228800, 30, 2")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint16_t type = ParsePropertyType(request.params[2]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[3]);
    std::string category = ParseText(request.params[4]);
    std::string subcategory = ParseText(request.params[5]);
    std::string name = ParseText(request.params[6]);
    std::string url = ParseText(request.params[7]);
    std::string data = ParseText(request.params[8]);
    std::string desiredName = ParseText(request.params[9]);
    int64_t numTokens = ParseAmount(request.params[10], type);
    int64_t deadline = ParseDeadline(request.params[11]);
    uint8_t earlyBonus = ParseEarlyBirdBonus(request.params[12]);
    uint8_t issuerPercentage = ParseIssuerBonus(request.params[13]);

    // perform checks
    RequirePropertyName(name);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId > 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name already exists");

    uint32_t propertyIdDesired = pDbSpInfo->findSPByName(desiredName);
    if (propertyIdDesired == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    RequireExistingProperty(propertyIdDesired);
    RequireSameEcosystem(ecosystem, propertyIdDesired);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuanceVariable(ecosystem, type, previousId, category, subcategory, name, url, data, propertyIdDesired, numTokens, deadline, earlyBonus, issuerPercentage);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, true);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue sendtokenissuancefixed(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 10)
        throw runtime_error(
            "sendtokenissuancefixed \"fromaddress\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\" \"amount\"\n"

            "\nCreate new tokens with fixed supply.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "4. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "5. category             (string, required) a category for the new tokens (can be \"\")\n"
            "6. subcategory          (string, required) a subcategory for the new tokens  (can be \"\")\n"
            "7. name                 (string, required) the name of the new tokens to create\n"
            "8. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "9. ipfs                 (string, required) IPFS string for the new tokens (can be \"\")\n"
            "10. amount              (string, required) the number of tokens to create\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokenissuancefixed", "\"3Ck2kEGLJtZw9ENj2tameMCtS3HB7uRar3\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" \"1000000\"")
            + HelpExampleRpc("sendtokenissuancefixed", "\"3Ck2kEGLJtZw9ENj2tameMCtS3HB7uRar3\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", \"1000000\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint16_t type = ParsePropertyType(request.params[2]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[3]);
    std::string category = ParseText(request.params[4]);
    std::string subcategory = ParseText(request.params[5]);
    std::string name = ParseText(request.params[6]);
    std::string url = ParseText(request.params[7]);
    std::string data = ParseText(request.params[8]);
    int64_t amount = ParseAmount(request.params[9], type);

    // perform checks
    RequirePropertyName(name);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId > 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name already exists");

    if (!IsTokenNameValid(name))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token name is invalid");

    if (!IsTokenIPFSValid(data))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token IPFS is invalid");

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(ecosystem, type, previousId, category, subcategory, name, url, data, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, true);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue sendtokenissuancemanaged(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 9)
        throw runtime_error(
            "sendtokenissuancemanaged \"fromaddress\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\"\n"

            "\nCreate new tokens with manageable supply.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "4. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "5. category             (string, required) a category for the new tokens (can be \"\")\n"
            "6. subcategory          (string, required) a subcategory for the new tokens  (can be \"\")\n"
            "7. name                 (string, required) the name of the new tokens to create\n"
            "8. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "9. ipfs                 (string, required) IPFS string for the new tokens (can be \"\")\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokenissuancemanaged", "\"RpAVz7YHGFjVrr29iiSmezkvd3SzBbuK7p\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
            + HelpExampleRpc("sendtokenissuancemanaged", "\"RpAVz7YHGFjVrr29iiSmezkvd3SzBbuK7p\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint16_t type = ParsePropertyType(request.params[2]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[3]);
    std::string category = ParseText(request.params[4]);
    std::string subcategory = ParseText(request.params[5]);
    std::string name = ParseText(request.params[6]);
    std::string url = ParseText(request.params[7]);
    std::string data = ParseText(request.params[8]);

    // perform checks
    RequirePropertyName(name);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId > 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name already exists");

    if (!IsTokenNameValid(name))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token name is invalid");

    if (!IsTokenIPFSValid(data))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token IPFS is invalid");

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuanceManaged(ecosystem, type, previousId, category, subcategory, name, url, data);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, true);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue token_sendsto(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 3 || request.params.size() > 5)
        throw runtime_error(
            "token_sendsto \"fromaddress\" propertyid \"amount\" ( \"redeemaddress\" distributionproperty )\n"

            "\nCreate and broadcast a send-to-owners transaction.\n"

            "\nArguments:\n"
            "1. fromaddress            (string, required) the address to send from\n"
            "2. propertyid             (number, required) the identifier of the tokens to distribute\n"
            "3. amount                 (string, required) the amount to distribute\n"
            "4. redeemaddress          (string, optional) an address that can spend the transaction dust (sender by default)\n"
            "5. distributionproperty   (number, optional) the identifier of the property holders to distribute to\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("token_sendsto", "\"32Z3tJccZuqQZ4PhJR2hxHC3tjgjA8cbqz\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 3 \"5000\"")
            + HelpExampleRpc("token_sendsto", "\"32Z3tJccZuqQZ4PhJR2hxHC3tjgjA8cbqz\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 3, \"5000\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));
    std::string redeemAddress = (request.params.size() > 3 && !ParseText(request.params[3]).empty()) ? ParseAddress(request.params[3]): "";
    uint32_t distributionPropertyId = (request.params.size() > 4) ? ParsePropertyId(request.params[4]) : propertyId;

    // perform checks
    RequireBalance(fromAddress, propertyId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendToOwners(propertyId, amount, distributionPropertyId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", redeemAddress, 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, TOKEN_TYPE_SEND_TO_OWNERS, propertyId, amount);
            return txid.GetHex();
        }
    }
}

static UniValue sendtokengrant(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 4 || request.params.size() > 5)
        throw runtime_error(
            "sendtokengrant \"fromaddress\" \"toaddress\" name \"amount\" ( \"memo\" )\n"

            "\nIssue or grant new units of managed tokens.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the receiver of the tokens (sender by default, can be \"\")\n"
            "3. name                 (string, required) the name of the token to grant\n"
            "4. amount               (string, required) the amount of tokens to create\n"
            "5. memo                 (string, optional) a text note attached to this transaction (none by default)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokengrant", "\"RpAVz7YHGFjVrr29iiSmezkvd3SzBbuK7p\" \"\" TOKEN \"7000\"")
            + HelpExampleRpc("sendtokengrant", "\"RpAVz7YHGFjVrr29iiSmezkvd3SzBbuK7p\", \"\", TOKEN, \"7000\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = !ParseText(request.params[1]).empty() ? ParseAddress(request.params[1]): "";
    std::string name = ParseText(request.params[2]);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));
    std::string memo = (request.params.size() > 4) ? ParseText(request.params[4]): "";

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Grant(propertyId, amount, memo);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue sendtokenrevoke(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 3 || request.params.size() > 4)
        throw runtime_error(
            "sendtokenrevoke \"fromaddress\" name \"amount\" ( \"memo\" )\n"

            "\nRevoke units of managed tokens.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to revoke the tokens from\n"
            "2. name                 (string, required) the name of the token to revoke\n"
            "3. amount               (string, required) the amount of tokens to revoke\n"
            "4. memo                 (string, optional) a text note attached to this transaction (none by default)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokenrevoke", "\"RpAVz7YHGFjVrr29iiSmezkvd3SzBbuK7p\" \"\" TOKEN \"100\"")
            + HelpExampleRpc("sendtokenrevoke", "\"RpAVz7YHGFjVrr29iiSmezkvd3SzBbuK7p\", \"\", TOKEN, \"100\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string name = ParseText(request.params[1]);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));
    std::string memo = (request.params.size() > 3) ? ParseText(request.params[3]): "";

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);
    RequireBalance(fromAddress, propertyId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Revoke(propertyId, amount, memo);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue sendtokenclosecrowdsale(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "sendtokenclosecrowdsale \"fromaddress\" name\n"

            "\nManually close a crowdsale.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address associated with the crowdsale to close\n"
            "2. name                 (string, required) the token name of crowdsale to close\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokenclosecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\" TOKEN")
            + HelpExampleRpc("sendtokenclosecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\", TOKEN")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string name = ParseText(request.params[1]);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    // perform checks
    RequireExistingProperty(propertyId);
    RequireCrowdsale(propertyId);
    RequireActiveCrowdsale(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_CloseCrowdsale(propertyId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue sendtokentrade(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 5)
        throw runtime_error(
            "sendtokentrade \"fromaddress\" tokenforsale \"amountforsale\" tokendesired \"amountdesired\"\n"

            "\nPlace a trade offer on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to trade with\n"
            "2. tokenforsale         (string, required) the name of the tokens to list for sale\n"
            "3. amountforsale        (string, required) the amount of tokens to list for sale\n"
            "4. tokendesired         (string, required) the name of the tokens desired in exchange\n"
            "5. amountdesired        (string, required) the amount of tokens desired in exchange\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokentrade", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" TOKEN \"250.0\" DESIRED \"10.0\"")
            + HelpExampleRpc("sendtokentrade", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", TOKEN, \"250.0\", DESIRED, \"10.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string tokenForSale = ParseText(request.params[1]);

    uint32_t propertyIdForSale = pDbSpInfo->findSPByName(tokenForSale);
    if (propertyIdForSale == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    int64_t amountForSale = ParseAmount(request.params[2], isPropertyDivisible(propertyIdForSale));
    std::string tokenDesired = ParseText(request.params[3]);

    uint32_t propertyIdDesired = pDbSpInfo->findSPByName(tokenDesired);
    if (propertyIdDesired == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    int64_t amountDesired = ParseAmount(request.params[4], isPropertyDivisible(propertyIdDesired));

    // perform checks
    RequireExistingProperty(propertyIdForSale);
    RequireExistingProperty(propertyIdDesired);
    RequireBalance(fromAddress, propertyIdForSale, amountForSale);
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExTrade(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, TOKEN_TYPE_METADEX_TRADE, propertyIdForSale, amountForSale);
            return txid.GetHex();
        }
    }
}

static UniValue sendtokencanceltradesbyprice(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 5)
        throw runtime_error(
            "sendtokencanceltradesbyprice \"fromaddress\" tokenforsale \"amountforsale\" tokenesired \"amountdesired\"\n"

            "\nCancel offers on the distributed token exchange with the specified price.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to trade with\n"
            "2. tokenforsale         (string, required) the name of the token listed for sale\n"
            "3. amountforsale        (string, required) the amount of tokens to listed for sale\n"
            "4. tokendesired         (string, required) the name of the token desired in exchange\n"
            "5. amountdesired        (string, required) the amount of tokens desired in exchange\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokencanceltradesbyprice", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" TOKEN \"100.0\" DESIRED \"5.0\"")
            + HelpExampleRpc("sendtokencanceltradesbyprice", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", TOKEN, \"100.0\", DESIRED, \"5.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string tokenForSale = ParseText(request.params[1]);

    uint32_t propertyIdForSale = pDbSpInfo->findSPByName(tokenForSale);
    if (propertyIdForSale == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    int64_t amountForSale = ParseAmount(request.params[2], isPropertyDivisible(propertyIdForSale));
    std::string tokenDesired = ParseText(request.params[3]);

    uint32_t propertyIdDesired = pDbSpInfo->findSPByName(tokenDesired);
    if (propertyIdDesired == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    int64_t amountDesired = ParseAmount(request.params[4], isPropertyDivisible(propertyIdDesired));

    // perform checks
    RequireExistingProperty(propertyIdForSale);
    RequireExistingProperty(propertyIdDesired);
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);
    // TODO: check, if there are matching offers to cancel

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPrice(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, TOKEN_TYPE_METADEX_CANCEL_PRICE, propertyIdForSale, amountForSale, false);
            return txid.GetHex();
        }
    }
}

static UniValue sendtokencanceltradesbypair(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "sendtokencanceltradesbypair \"fromaddress\" tokenforsale tokendesired\n"

            "\nCancel all offers on the distributed token exchange with the given currency pair.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to trade with\n"
            "2. tokenforsale         (string, required) the identifier of the tokens listed for sale\n"
            "3. tokendesired         (string, required) the identifier of the tokens desired in exchange\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokencanceltradesbypair", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" TOKEN DESIRED")
            + HelpExampleRpc("sendtokencanceltradesbypair", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", TOKEN, DESIRED")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string tokenForSale = ParseText(request.params[1]);

    uint32_t propertyIdForSale = pDbSpInfo->findSPByName(tokenForSale);
    if (propertyIdForSale == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    std::string tokenDesired = ParseText(request.params[2]);
    uint32_t propertyIdDesired = pDbSpInfo->findSPByName(tokenDesired);
        if (propertyIdDesired == 0)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    // perform checks
    RequireExistingProperty(propertyIdForSale);
    RequireExistingProperty(propertyIdDesired);
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);
    // TODO: check, if there are matching offers to cancel

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPair(propertyIdForSale, propertyIdDesired);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, TOKEN_TYPE_METADEX_CANCEL_PAIR, propertyIdForSale, 0, false);
            return txid.GetHex();
        }
    }
}

static UniValue sendtokencancelalltrades(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "sendtokencancelalltrades \"fromaddress\" ecosystem\n"

            "\nCancel all offers on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to trade with\n"
            "2. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokencancelalltrades", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 1")
            + HelpExampleRpc("sendtokencancelalltrades", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 1")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);

    // perform checks
    // TODO: check, if there are matching offers to cancel

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelEcosystem(ecosystem);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, TOKEN_TYPE_METADEX_CANCEL_ECOSYSTEM, ecosystem, 0, false);
            return txid.GetHex();
        }
    }
}

static UniValue sendtokenchangeissuer(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "sendtokenchangeissuer \"fromaddress\" \"toaddress\" tokenname\n"

            "\nChange the issuer on record of the given tokens.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address associated with the tokens\n"
            "2. toaddress            (string, required) the address to transfer administrative control to\n"
            "3. name                 (string, required) the name of token\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokenchangeissuer", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" TOKEN")
            + HelpExampleRpc("sendtokenchangeissuer", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", TOKEN")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    std::string name = ParseText(request.params[2]);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    // perform checks
    RequireExistingProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ChangeIssuer(propertyId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue sendtokenenablefreezing(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "sendtokenenablefreezing \"fromaddress\" tokenname\n"

            "\nEnables address freezing for a centrally managed property.\n"

            "\nArguments:\n"
            "1. fromaddress          (string,  required) the issuer of the tokens\n"
            "2. tokenname            (string,  required) the name of token\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokenenablefreezing", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" TOKEN")
            + HelpExampleRpc("sendtokenenablefreezing", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", TOKEN")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string name = ParseText(request.params[1]);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_EnableFreezing(propertyId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue sendtokendisablefreezing(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "sendtokendisablefreezing \"fromaddress\" tokenname\n"

            "\nDisables address freezing for a centrally managed property.\n"
            "\nIMPORTANT NOTE:  Disabling freezing for a property will UNFREEZE all frozen addresses for that property!"

            "\nArguments:\n"
            "1. fromaddress          (string,  required) the issuer of the tokens\n"
            "2. tokenname            (string,  required) the name of token\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("sendtokendisablefreezing", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" TOKEN")
            + HelpExampleRpc("sendtokendisablefreezing", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", TOKEN")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string name = ParseText(request.params[1]);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DisableFreezing(propertyId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue sendtokenfreeze(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "sendtokenfreeze \"fromaddress\" \"toaddress\" tokenname amount \n"
            "\nFreeze an address for a centrally managed token.\n"
            "\nNote: Only the issuer may freeze tokens, and only if the token is of the managed type with the freezing option enabled.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from (must be the issuer of the property)\n"
            "2. toaddress            (string, required) the address to freeze tokens for\n"
            "3. tokenname            (string, required) the name of token to freeze for (must be managed type and have freezing option enabled)\n"
            "4. amount               (number, required) the amount of tokens to freeze (note: this is unused - once frozen an address cannot send any transactions for the property)\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtokenfreeze", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" TOKEN 0")
            + HelpExampleRpc("sendtokenfreeze", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", TOKEN, 0")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string refAddress = ParseAddress(request.params[1]);
    std::string name = ParseText(request.params[2]);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_FreezeTokens(propertyId, amount, refAddress);

    // request the wallet build the transaction (and if needed commit it)
    // Note: no ref address is sent to WalletTxBuilder as the ref address is contained within the payload
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue sendtokenunfreeze(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "sendtokenunfreeze \"fromaddress\" \"toaddress\" tokenname amount \n"
            "\nUnfreezes an address for a centrally managed token.\n"
            "\nNote: Only the issuer may unfreeze tokens.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from (must be the issuer of the property)\n"
            "2. toaddress            (string, required) the address to unfreeze tokens for\n"
            "3. tokenname            (string, required) the name of token to unfreeze for (must be managed type and have freezing option enabled)\n"
            "4. amount               (number, required) the amount of tokens to unfreeze (note: this is unused - once frozen an address cannot send any transactions for the property)\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtokenunfreeze", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" TOKEN 0")
            + HelpExampleRpc("sendtokenunfreeze", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", TOKEN, 0")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string refAddress = ParseAddress(request.params[1]);
    std::string name = ParseText(request.params[2]);

    uint32_t propertyId = pDbSpInfo->findSPByName(name);
    if (propertyId == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Token with this name doesn't exists");

    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_UnfreezeTokens(propertyId, amount, refAddress);

    // request the wallet build the transaction (and if needed commit it)
    // Note: no ref address is sent to WalletTxBuilder as the ref address is contained within the payload
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue token_sendactivation(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "token_sendactivation \"fromaddress\" featureid block minclientversion\n"
            "\nActivate a protocol feature.\n"
            "\nNote: Token Core ignores activations from unauthorized sources.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. featureid            (number, required) the identifier of the feature to activate\n"
            "3. block                (number, required) the activation block\n"
            "4. minclientversion     (number, required) the minimum supported client version\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("token_sendactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1 370000 999")
            + HelpExampleRpc("token_sendactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1, 370000, 999")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint16_t featureId = request.params[1].get_int();
    uint32_t activationBlock = request.params[2].get_int();
    uint32_t minClientVersion = request.params[3].get_int();

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ActivateFeature(featureId, activationBlock, minClientVersion);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue token_senddeactivation(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "token_senddeactivation \"fromaddress\" featureid\n"
            "\nDeactivate a protocol feature.  For Emergency Use Only.\n"
            "\nNote: Token Core ignores deactivations from unauthorized sources.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. featureid            (number, required) the identifier of the feature to activate\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("token_senddeactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1")
            + HelpExampleRpc("token_senddeactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint16_t featureId = request.params[1].get_int64();

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DeactivateFeature(featureId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue token_sendalert(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "token_sendalert \"fromaddress\" alerttype expiryvalue typecheck versioncheck \"message\"\n"
            "\nCreates and broadcasts an Token Core alert.\n"
            "\nNote: Token Core ignores alerts from unauthorized sources.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. alerttype            (number, required) the alert type\n"
            "3. expiryvalue          (number, required) the value when the alert expires (depends on alert type)\n"
            "4. message              (string, required) the user-faced alert message\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("token_sendalert", "")
            + HelpExampleRpc("token_sendalert", "")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    int64_t tempAlertType = request.params[1].get_int64();
    if (tempAlertType < 1 || 65535 < tempAlertType) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Alert type is out of range");
    }
    uint16_t alertType = static_cast<uint16_t>(tempAlertType);
    int64_t tempExpiryValue = request.params[2].get_int64();
    if (tempExpiryValue < 1 || 4294967295LL < tempExpiryValue) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Expiry value is out of range");
    }
    uint32_t expiryValue = static_cast<uint32_t>(tempExpiryValue);
    std::string alertMessage = ParseText(request.params[3]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_TokenCoreAlert(alertType, expiryValue, alertMessage);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue trade_MP(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 6)
        throw runtime_error(
            "trade_MP \"fromaddress\" propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\" action\n"
            "\nNote: this command is depreciated, and was replaced by:\n"
            " - sendtrade_TOKEN\n"
            " - sendcanceltradebyprice_TOKEN\n"
            " - sendcanceltradebypair_TOKEN\n"
            " - sendcanceltradebypair_TOKEN\n"
        );

    UniValue values(UniValue::VARR);
    uint8_t action = ParseMetaDExAction(request.params[5]);

    // Forward to the new commands, based on action value
    switch (action) {
        case CMPTransaction::ADD:
        {
            values.push_back(request.params[0]); // fromAddress
            values.push_back(request.params[1]); // propertyIdForSale
            values.push_back(request.params[2]); // amountForSale
            values.push_back(request.params[3]); // propertyIdDesired
            values.push_back(request.params[4]); // amountDesired
            return sendtokentrade(request);
        }
        case CMPTransaction::CANCEL_AT_PRICE:
        {
            values.push_back(request.params[0]); // fromAddress
            values.push_back(request.params[1]); // propertyIdForSale
            values.push_back(request.params[2]); // amountForSale
            values.push_back(request.params[3]); // propertyIdDesired
            values.push_back(request.params[4]); // amountDesired
            return sendtokencanceltradesbyprice(request);
        }
        case CMPTransaction::CANCEL_ALL_FOR_PAIR:
        {
            values.push_back(request.params[0]); // fromAddress
            values.push_back(request.params[1]); // propertyIdForSale
            values.push_back(request.params[3]); // propertyIdDesired
            return sendtokencanceltradesbypair(request);
        }
        case CMPTransaction::CANCEL_EVERYTHING:
        {
            uint8_t ecosystem = 0;
            if (isMainEcosystemProperty(request.params[1].get_int64())
                    && isMainEcosystemProperty(request.params[3].get_int64())) {
                ecosystem = TOKEN_PROPERTY_MSC;
            }
            if (isTestEcosystemProperty(request.params[1].get_int64())
                    && isTestEcosystemProperty(request.params[3].get_int64())) {
                ecosystem = TOKEN_PROPERTY_TMSC;
            }
            values.push_back(request.params[0]); // fromAddress
            values.push_back(ecosystem);
            return sendtokencancelalltrades(request);
        }
    }

    throw JSONRPCError(RPC_TYPE_ERROR, "Invalid action (1,2,3,4 only)");
}

static const CRPCCommand commands[] =
{ //  category                         name                            actor (function)               okSafeMode
  //  -------------------------------- ------------------------------- ------------------------------ ----------
#ifdef ENABLE_WALLET
    { "tokens (transaction creation)", "sendtokenrawtx",               &sendtokenrawtx,               false },
    { "tokens (transaction creation)", "sendtoken",                    &sendtoken,                    false },
    // { "tokens (transaction creation)", "sendtokendexsell",             &sendtokendexsell,             false },
    // { "tokens (transaction creation)", "sendtokendexaccept",           &sendtokendexaccept,           false },
    { "tokens (transaction creation)", "sendtokenissuancecrowdsale",   &sendtokenissuancecrowdsale,   false },
    { "tokens (transaction creation)", "sendtokenissuancefixed",       &sendtokenissuancefixed,       false },
    { "tokens (transaction creation)", "sendtokenissuancemanaged",     &sendtokenissuancemanaged,     false },
    { "tokens (transaction creation)", "sendtokentrade",               &sendtokentrade,               false },
    { "tokens (transaction creation)", "sendtokencanceltradesbyprice", &sendtokencanceltradesbyprice, false },
    { "tokens (transaction creation)", "sendtokencanceltradesbypair",  &sendtokencanceltradesbypair,  false },
    { "tokens (transaction creation)", "sendtokencancelalltrades",     &sendtokencancelalltrades,     false },
    // { "tokens (transaction creation)", "token_sendsto",                 &token_sendsto,                 false },
    { "tokens (transaction creation)", "sendtokengrant",               &sendtokengrant,               false },
    { "tokens (transaction creation)", "sendtokenrevoke",              &sendtokenrevoke,              false },
    { "tokens (transaction creation)", "sendtokenclosecrowdsale",      &sendtokenclosecrowdsale,      false },
    { "tokens (transaction creation)", "sendtokenchangeissuer",        &sendtokenchangeissuer,        false },
    { "tokens (transaction creation)", "sendalltokens",                &sendalltokens,                false },
    { "tokens (transaction creation)", "sendtokenenablefreezing",      &sendtokenenablefreezing,      false },
    { "tokens (transaction creation)", "sendtokendisablefreezing",     &sendtokendisablefreezing,     false },
    { "tokens (transaction creation)", "sendtokenfreeze",              &sendtokenfreeze,              false },
    { "tokens (transaction creation)", "sendtokenunfreeze",            &sendtokenunfreeze,            false },
    // { "hidden",                            "token_senddeactivation",        &token_senddeactivation,        true  },
    // { "hidden",                            "token_sendactivation",          &token_sendactivation,          false },
    // { "hidden",                            "token_sendalert",               &token_sendalert,               true  },
    { "tokens (transaction creation)", "sendtokenfunded",             &sendtokenfunded,             false },
    { "tokens (transaction creation)", "sendtokenfundedall",          &sendtokenfundedall,          false },
#endif
};

void RegisterTokenTransactionCreationRPCCommands()
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
