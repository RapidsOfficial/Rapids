// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/server.h"

#include "base58.h"
#include "fs.h"
#include "init.h"
#include "main.h"
#include "random.h"
#include "sync.h"
#include "guiinterface.h"
#include "util.h"
#include "utilstrencodings.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif // ENABLE_WALLET

#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string/case_conv.hpp> // for to_upper()

#include <univalue.h>


static bool fRPCRunning = false;
static bool fRPCInWarmup = true;
static std::string rpcWarmupStatus("RPC server started");
static RecursiveMutex cs_rpcWarmup;

/* Timer-creating functions */
static RPCTimerInterface* timerInterface = NULL;
/* Map of name to timer.
 * @note Can be changed to std::unique_ptr when C++11 */
static std::map<std::string, boost::shared_ptr<RPCTimerBase> > deadlineTimers;

static struct CRPCSignals
{
    boost::signals2::signal<void ()> Started;
    boost::signals2::signal<void ()> Stopped;
    boost::signals2::signal<void (const CRPCCommand&)> PreCommand;
    boost::signals2::signal<void (const CRPCCommand&)> PostCommand;
} g_rpcSignals;

void RPCServer::OnStarted(std::function<void ()> slot)
{
    g_rpcSignals.Started.connect(slot);
}

void RPCServer::OnStopped(std::function<void ()> slot)
{
    g_rpcSignals.Stopped.connect(slot);
}

void RPCServer::OnPreCommand(std::function<void (const CRPCCommand&)> slot)
{
    g_rpcSignals.PreCommand.connect(std::bind(slot, std::placeholders::_1));
}

void RPCServer::OnPostCommand(std::function<void (const CRPCCommand&)> slot)
{
    g_rpcSignals.PostCommand.connect(std::bind(slot, std::placeholders::_1));
}

void RPCTypeCheck(const UniValue& params,
                  const std::list<UniValue::VType>& typesExpected,
                  bool fAllowNull)
{
    unsigned int i = 0;
    for (UniValue::VType t : typesExpected) {
        if (params.size() <= i)
            break;

        const UniValue& v = params[i];
        if (!((v.type() == t) || (fAllowNull && (v.isNull())))) {
            std::string err = strprintf("Expected type %s, got %s",
                                   uvTypeName(t), uvTypeName(v.type()));
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
        i++;
    }
}

void RPCTypeCheckObj(const UniValue& o,
                  const std::map<std::string, UniValue::VType>& typesExpected,
                  bool fAllowNull,
                  bool fStrict)
{
    for (const PAIRTYPE(std::string, UniValue::VType)& t : typesExpected) {
        const UniValue& v = find_value(o, t.first);
        if (!fAllowNull && v.isNull())
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing %s", t.first));

        if (!((v.type() == t.second) || (fAllowNull && (v.isNull())))) {
            std::string err = strprintf("Expected type %s for %s, got %s",
                                   uvTypeName(t.second), t.first, uvTypeName(v.type()));
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
    }

    if (fStrict) {
        for (const std::string& k : o.getKeys()) {
            if (typesExpected.count(k) == 0) {
                std::string err = strprintf("Unexpected key %s", k);
                throw JSONRPCError(RPC_TYPE_ERROR, err);
            }
        }
    }
}

CAmount AmountFromValue(const UniValue& value)
{
    if (!value.isNum() && !value.isStr())
        throw JSONRPCError(RPC_TYPE_ERROR,"Amount is not a number or string");
    CAmount nAmount;
    if (!ParseFixedPoint(value.getValStr(), 8, &nAmount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    if (!Params().GetConsensus().MoneyRange(nAmount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Amount out of range");
    return nAmount;
}

UniValue ValueFromAmount(const CAmount& amount)
{
    bool sign = amount < 0;
    int64_t n_abs = (sign ? -amount : amount);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    return UniValue(UniValue::VNUM,
            strprintf("%s%d.%08d", sign ? "-" : "", quotient, remainder));
}

uint256 ParseHashV(const UniValue& v, std::string strName)
{
    std::string strHex;
    if (v.isStr())
        strHex = v.get_str();
    if (!IsHex(strHex)) // Note: IsHex("") is false
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName + " must be hexadecimal string (not '" + strHex + "')");
    if (64 != strHex.length())
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("%s must be of length %d (not %d)", strName, 64, strHex.length()));
    uint256 result;
    result.SetHex(strHex);
    return result;
}
uint256 ParseHashO(const UniValue& o, std::string strKey)
{
    return ParseHashV(find_value(o, strKey), strKey);
}
std::vector<unsigned char> ParseHexV(const UniValue& v, std::string strName)
{
    std::string strHex;
    if (v.isStr())
        strHex = v.get_str();
    if (!IsHex(strHex))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName + " must be hexadecimal string (not '" + strHex + "')");
    return ParseHex(strHex);
}
std::vector<unsigned char> ParseHexO(const UniValue& o, std::string strKey)
{
    return ParseHexV(find_value(o, strKey), strKey);
}

int ParseInt(const UniValue& o, std::string strKey)
{
    const UniValue& v = find_value(o, strKey);
    if (v.isNum())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, " + strKey + "is not an int");

    return v.get_int();
}

bool ParseBool(const UniValue& o, std::string strKey)
{
    const UniValue& v = find_value(o, strKey);
    if (v.isBool())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, " + strKey + "is not a bool");

    return v.get_bool();
}


/**
 * Note: This interface may still be subject to change.
 */

std::string CRPCTable::help(std::string strCommand) const
{
    std::string strRet;
    std::string category;
    std::set<rpcfn_type> setDone;
    std::vector<std::pair<std::string, const CRPCCommand*> > vCommands;

    for (const auto& entry : mapCommands)
        vCommands.push_back(std::make_pair(entry.second->category + entry.first, entry.second));
    std::sort(vCommands.begin(), vCommands.end());

    for (const std::pair<std::string, const CRPCCommand*>& command : vCommands) {
        const CRPCCommand* pcmd = command.second;
        std::string strMethod = pcmd->name;
        if ((strCommand != "" || pcmd->category == "hidden") && strMethod != strCommand)
            continue;
        try {
            JSONRPCRequest jreq;
            jreq.fHelp = true;
            rpcfn_type pfn = pcmd->actor;
            if (setDone.insert(pfn).second)
                (*pfn)(jreq);
        } catch (const std::exception& e) {
            // Help text is returned in an exception
            std::string strHelp = std::string(e.what());
            if (strCommand == "") {
                if (strHelp.find('\n') != std::string::npos)
                    strHelp = strHelp.substr(0, strHelp.find('\n'));

                if (category != pcmd->category) {
                    if (!category.empty())
                        strRet += "\n";
                    category = pcmd->category;
                    std::string firstLetter = category.substr(0, 1);
                    boost::to_upper(firstLetter);
                    strRet += "== " + firstLetter + category.substr(1) + " ==\n";
                }
            }
            strRet += strHelp + "\n";
        }
    }
    if (strRet == "")
        strRet = strprintf("help: unknown command: %s\n", strCommand);
    strRet = strRet.substr(0, strRet.size() - 1);
    return strRet;
}

UniValue help(const JSONRPCRequest& jsonRequest)
{
    if (jsonRequest.fHelp || jsonRequest.params.size() > 1)
        throw std::runtime_error(
            "help ( \"command\" )\n"
            "\nList all commands, or get help for a specified command.\n"
            "\nArguments:\n"
            "1. \"command\"     (string, optional) The command to get help on\n"
            "\nResult:\n"
            "\"text\"     (string) The help text\n");

    std::string strCommand;
    if (jsonRequest.params.size() > 0)
        strCommand = jsonRequest.params[0].get_str();

    return tableRPC.help(strCommand);
}


UniValue stop(const JSONRPCRequest& jsonRequest)
{
    // Accept the deprecated and ignored 'detach' boolean argument
    if (jsonRequest.fHelp || jsonRequest.params.size() > 1)
        throw std::runtime_error(
            "stop\n"
            "\nStop RPD server.");
    // Event loop will exit after current HTTP requests have been handled, so
    // this reply will get back to the client.
    StartShutdown();
    return "RPD server stopping";
}


/**
 * Call Table
 */
static const CRPCCommand vRPCCommands[] =
    {
        //  category              name                      actor (function)         okSafeMode
        //  --------------------- ------------------------  -----------------------  ----------
        /* Overall control/query calls */
        {"control", "getinfo", &getinfo, true }, /* uses wallet if enabled */
        {"control", "help", &help, true },
        {"control", "stop", &stop, true },

        /* P2P networking */
        {"network", "getnetworkinfo", &getnetworkinfo, true },
        {"network", "addnode", &addnode, true },
        {"network", "disconnectnode", &disconnectnode, true },
        {"network", "getaddednodeinfo", &getaddednodeinfo, true },
        {"network", "getconnectioncount", &getconnectioncount, true },
        {"network", "getnettotals", &getnettotals, true },
        {"network", "getpeerinfo", &getpeerinfo, true },
        {"network", "ping", &ping, true },
        {"network", "setban", &setban, true },
        {"network", "listbanned", &listbanned, true },
        {"network", "clearbanned", &clearbanned, true },

        /* Block chain and UTXO */
        {"blockchain", "findserial", &findserial, true },
        {"blockchain", "getblockindexstats", &getblockindexstats, true },
        {"blockchain", "getserials", &getserials, true },
        {"blockchain", "getblockchaininfo", &getblockchaininfo, true },
        {"blockchain", "getbestblockhash", &getbestblockhash, true },
        {"blockchain", "getblockcount", &getblockcount, true },
        {"blockchain", "getblock", &getblock, true },
        {"blockchain", "getblockhash", &getblockhash, true },
        {"blockchain", "getblockheader", &getblockheader, false },
        {"blockchain", "getchaintips", &getchaintips, true },
        {"blockchain", "getdifficulty", &getdifficulty, true },
        {"blockchain", "getfeeinfo", &getfeeinfo, true },
        {"blockchain", "getmempoolinfo", &getmempoolinfo, true },
        {"blockchain", "getrawmempool", &getrawmempool, true },
        {"blockchain", "clearmempool", &clearmempool, true },
        {"blockchain", "gettxout", &gettxout, true },
        {"blockchain", "gettxoutsetinfo", &gettxoutsetinfo, true },
        {"blockchain", "invalidateblock", &invalidateblock, true },
        {"blockchain", "reconsiderblock", &reconsiderblock, true },
        {"blockchain", "verifychain", &verifychain, true },

        {"blockchain", "issuanceinfo", &issuanceinfo, true },

        /* Mining */
        {"mining", "getblocktemplate", &getblocktemplate, true },
        {"mining", "getmininginfo", &getmininginfo, true },
        {"mining", "getnetworkhashps", &getnetworkhashps, true },
        {"mining", "prioritisetransaction", &prioritisetransaction, true },
        {"mining", "submitblock", &submitblock, true },

#ifdef ENABLE_WALLET
        /* Coin generation */
        {"generating", "getgenerate", &getgenerate, true },
        {"generating", "gethashespersec", &gethashespersec, true },
        {"generating", "setgenerate", &setgenerate, true },
        {"generating", "generate", &generate, true },
#endif

        /* Raw transactions */
        {"rawtransactions", "createrawtransaction", &createrawtransaction, true },
        {"rawtransactions", "decoderawtransaction", &decoderawtransaction, true },
        {"rawtransactions", "decodescript", &decodescript, true },
        {"rawtransactions", "getrawtransaction", &getrawtransaction, true },
        {"rawtransactions", "fundrawtransaction", &fundrawtransaction, false},
        {"rawtransactions", "sendrawtransaction", &sendrawtransaction, false },
        {"rawtransactions", "signrawtransaction", &signrawtransaction, false }, /* uses wallet if enabled */

        /* Utility functions */
        {"util", "createmultisig", &createmultisig, true },
        {"util", "logging", &logging, true },
        {"util", "validateaddress", &validateaddress, true }, /* uses wallet if enabled */
        {"util", "verifymessage", &verifymessage, true },
        {"util", "estimatefee", &estimatefee, true },
        {"util","estimatesmartfee",       &estimatesmartfee,       true  },

                /* Not shown in help */
        {"hidden", "invalidateblock", &invalidateblock, true },
        {"hidden", "reconsiderblock", &reconsiderblock, true },
        {"hidden", "setmocktime", &setmocktime, true },
        { "hidden",             "waitfornewblock",        &waitfornewblock,        true },
        { "hidden",             "waitforblock",           &waitforblock,           true },
        { "hidden",             "waitforblockheight",     &waitforblockheight,     true },

        /* RPD features */
        {"rapids", "listmasternodes", &listmasternodes, true },
        {"rapids", "getcachedblockhashes", &getcachedblockhashes, true },
        {"rapids", "getmasternodecount", &getmasternodecount, true },
        {"rapids", "createmasternodebroadcast", &createmasternodebroadcast, true },
        {"rapids", "decodemasternodebroadcast", &decodemasternodebroadcast, true },
        {"rapids", "relaymasternodebroadcast", &relaymasternodebroadcast, true },
        {"rapids", "masternodecurrent", &masternodecurrent, true },
        {"rapids", "startmasternode", &startmasternode, true },
        {"rapids", "createmasternodekey", &createmasternodekey, true },
        {"rapids", "getmasternodeoutputs", &getmasternodeoutputs, true },
        {"rapids", "listmasternodeconf", &listmasternodeconf, true },
        {"rapids", "getmasternodestatus", &getmasternodestatus, true },
        {"rapids", "getmasternodewinners", &getmasternodewinners, true },
        {"rapids", "getmasternodescores", &getmasternodescores, true },
        {"rapids", "preparebudget", &preparebudget, true },
        {"rapids", "submitbudget", &submitbudget, true },
        {"rapids", "mnbudgetvote", &mnbudgetvote, true },
        {"rapids", "getbudgetvotes", &getbudgetvotes, true },
        {"rapids", "getnextsuperblock", &getnextsuperblock, true },
        {"rapids", "getbudgetprojection", &getbudgetprojection, true },
        {"rapids", "getbudgetinfo", &getbudgetinfo, true },
        {"rapids", "mnbudgetrawvote", &mnbudgetrawvote, true },
        {"rapids", "mnfinalbudget", &mnfinalbudget, true },
        {"rapids", "checkbudgets", &checkbudgets, true },
        {"rapids", "mnsync", &mnsync, true },
        {"rapids", "spork", &spork, true },

#ifdef ENABLE_WALLET
        /* Wallet */
        {"wallet", "bip38encrypt", &bip38encrypt, true },
        {"wallet", "bip38decrypt", &bip38decrypt, true },
        {"wallet", "getaddressinfo", &getaddressinfo, true },
        {"wallet", "getstakingstatus", &getstakingstatus, false },
        {"wallet", "multisend", &multisend, false },
        // {"zerocoin", "createrawzerocoinspend", &createrawzerocoinspend, false },
        // {"zerocoin", "getzerocoinbalance", &getzerocoinbalance, false },
        // {"zerocoin", "listmintedzerocoins", &listmintedzerocoins, false },
        // {"zerocoin", "listspentzerocoins", &listspentzerocoins, false },
        // {"zerocoin", "listzerocoinamounts", &listzerocoinamounts, false },
        // {"zerocoin", "mintzerocoin", &mintzerocoin, false },
        // {"zerocoin", "spendzerocoin", &spendzerocoin, false },
        // {"zerocoin", "spendrawzerocoin", &spendrawzerocoin, true },
        // {"zerocoin", "spendzerocoinmints", &spendzerocoinmints, false },
        // {"zerocoin", "resetmintzerocoin", &resetmintzerocoin, false },
        // {"zerocoin", "resetspentzerocoin", &resetspentzerocoin, false },
        // {"zerocoin", "getarchivedzerocoin", &getarchivedzerocoin, false },
        // {"zerocoin", "importzerocoins", &importzerocoins, false },
        // {"zerocoin", "exportzerocoins", &exportzerocoins, false },
        // {"zerocoin", "reconsiderzerocoins", &reconsiderzerocoins, false },
        // {"zerocoin", "getspentzerocoinamount", &getspentzerocoinamount, false },
        // {"zerocoin", "getzpivseed", &getzpivseed, false },
        // {"zerocoin", "setzpivseed", &setzpivseed, false },
        // {"zerocoin", "generatemintlist", &generatemintlist, false },
        // {"zerocoin", "searchdzpiv", &searchdzpiv, false },
        // {"zerocoin", "dzpivstate", &dzpivstate, false },

        {"util", "getaddresstxids", &getaddresstxids, true },
        {"util", "getaddressdeltas", &getaddressdeltas, true },
        {"util", "getaddressbalance", &getaddressbalance, true },
        {"util", "getaddressutxos", &getaddressutxos, true },
        {"util", "getaddressmempool", &getaddressmempool, true },
        {"util", "getblockhashes", &getblockhashes, true },
        {"util", "getspentinfo", &getspentinfo, true }

#endif // ENABLE_WALLET
};

CRPCTable::CRPCTable()
{
    unsigned int vcidx;
    for (vcidx = 0; vcidx < (sizeof(vRPCCommands) / sizeof(vRPCCommands[0])); vcidx++) {
        const CRPCCommand* pcmd;

        pcmd = &vRPCCommands[vcidx];
        mapCommands[pcmd->name] = pcmd;
    }
}

const CRPCCommand *CRPCTable::operator[](const std::string &name) const
{
    std::map<std::string, const CRPCCommand*>::const_iterator it = mapCommands.find(name);
    if (it == mapCommands.end())
        return NULL;
    return (*it).second;
}

bool CRPCTable::appendCommand(const std::string& name, const CRPCCommand* pcmd)
{
    if (IsRPCRunning())
        return false;

    // don't allow overwriting for now
    std::map<std::string, const CRPCCommand*>::const_iterator it = mapCommands.find(name);
    if (it != mapCommands.end())
        return false;

    mapCommands[name] = pcmd;
    return true;
}

bool StartRPC()
{
    LogPrint(BCLog::RPC, "Starting RPC\n");
    fRPCRunning = true;
    g_rpcSignals.Started();
    return true;
}

void InterruptRPC()
{
    LogPrint(BCLog::RPC, "Interrupting RPC\n");
    // Interrupt e.g. running longpolls
    fRPCRunning = false;
}

void StopRPC()
{
    LogPrint(BCLog::RPC, "Stopping RPC\n");
    deadlineTimers.clear();
    g_rpcSignals.Stopped();
}

bool IsRPCRunning()
{
    return fRPCRunning;
}

void SetRPCWarmupStatus(const std::string& newStatus)
{
    LOCK(cs_rpcWarmup);
    rpcWarmupStatus = newStatus;
}

void SetRPCWarmupFinished()
{
    LOCK(cs_rpcWarmup);
    assert(fRPCInWarmup);
    fRPCInWarmup = false;
}

bool RPCIsInWarmup(std::string* outStatus)
{
    LOCK(cs_rpcWarmup);
    if (outStatus)
        *outStatus = rpcWarmupStatus;
    return fRPCInWarmup;
}

void JSONRPCRequest::parse(const UniValue& valRequest)
{
    // Parse request
    if (!valRequest.isObject())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Invalid Request object");
    const UniValue& request = valRequest.get_obj();

    // Parse id now so errors from here on will have the id
    id = find_value(request, "id");

    // Parse method
    UniValue valMethod = find_value(request, "method");
    if (valMethod.isNull())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Missing method");
    if (!valMethod.isStr())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Method must be a string");
    strMethod = valMethod.get_str();
    if (strMethod != "getblocktemplate")
        LogPrint(BCLog::RPC, "ThreadRPCServer method=%s\n", SanitizeString(strMethod));

    // Parse params
    UniValue valParams = find_value(request, "params");
    if (valParams.isArray())
        params = valParams.get_array();
    else if (valParams.isNull())
        params = UniValue(UniValue::VARR);
    else
        throw JSONRPCError(RPC_INVALID_REQUEST, "Params must be an array");
}

bool IsDeprecatedRPCEnabled(const std::string& method)
{
    const std::vector<std::string> enabled_methods = mapMultiArgs["-deprecatedrpc"];

    return find(enabled_methods.begin(), enabled_methods.end(), method) != enabled_methods.end();
}

static UniValue JSONRPCExecOne(const UniValue& req)
{
    UniValue rpc_result(UniValue::VOBJ);

    JSONRPCRequest jreq;
    try {
        jreq.parse(req);

        UniValue result = tableRPC.execute(jreq);
        rpc_result = JSONRPCReplyObj(result, NullUniValue, jreq.id);
    } catch (const UniValue& objError) {
        rpc_result = JSONRPCReplyObj(NullUniValue, objError, jreq.id);
    } catch (const std::exception& e) {
        rpc_result = JSONRPCReplyObj(NullUniValue,
            JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
    }

    return rpc_result;
}

std::string JSONRPCExecBatch(const UniValue& vReq)
{
    UniValue ret(UniValue::VARR);
    for (unsigned int reqIdx = 0; reqIdx < vReq.size(); reqIdx++)
        ret.push_back(JSONRPCExecOne(vReq[reqIdx]));

    return ret.write() + "\n";
}

UniValue CRPCTable::execute(const JSONRPCRequest &request) const
{
    // Return immediately if in warmup
    std::string strWarmupStatus;
    if (RPCIsInWarmup(&strWarmupStatus)) {
        throw JSONRPCError(RPC_IN_WARMUP, "RPC in warm-up: " + strWarmupStatus);
    }

    // Find method
    const CRPCCommand* pcmd = tableRPC[request.strMethod];
    if (!pcmd)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");

    g_rpcSignals.PreCommand(*pcmd);

    try {
        // Execute
        return pcmd->actor(request);
    } catch (const std::exception& e) {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }

    g_rpcSignals.PostCommand(*pcmd);
}

std::vector<std::string> CRPCTable::listCommands() const
{
    std::vector<std::string> commandList;
    for (const auto& i : mapCommands) commandList.emplace_back(i.first);
    return commandList;
}

std::string HelpExampleCli(std::string methodname, std::string args)
{
    return "> rapids-cli " + methodname + " " + args + "\n";
}

std::string HelpExampleRpc(std::string methodname, std::string args)
{
    return "> curl --user myusername --data-binary '{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", "
           "\"method\": \"" +
           methodname + "\", \"params\": [" + args + "] }' -H 'content-type: text/plain;' http://127.0.0.1:51473/\n";
}

void RPCSetTimerInterfaceIfUnset(RPCTimerInterface *iface)
{
    if (!timerInterface)
        timerInterface = iface;
}

void RPCSetTimerInterface(RPCTimerInterface *iface)
{
    timerInterface = iface;
}

void RPCUnsetTimerInterface(RPCTimerInterface *iface)
{
    if (timerInterface == iface)
        timerInterface = NULL;
}

void RPCRunLater(const std::string& name, std::function<void(void)> func, int64_t nSeconds)
{
    if (!timerInterface)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "No timer handler registered for RPC");
    deadlineTimers.erase(name);
    LogPrint(BCLog::RPC, "queue run of timer %s in %i seconds (using %s)\n", name, nSeconds, timerInterface->Name());
    deadlineTimers.insert(std::make_pair(name, boost::shared_ptr<RPCTimerBase>(timerInterface->NewTimer(func, nSeconds*1000))));
}

CRPCTable tableRPC;
