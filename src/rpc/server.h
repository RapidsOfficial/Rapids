// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPCSERVER_H
#define BITCOIN_RPCSERVER_H

#include "amount.h"
#include "zpiv/zerocoin.h"
#include "rpc/protocol.h"
#include "uint256.h"

#include <list>
#include <map>
#include <stdint.h>
#include <string>

#include <univalue.h>

class CRPCCommand;

namespace RPCServer
{
    void OnStarted(std::function<void ()> slot);
    void OnStopped(std::function<void ()> slot);
    void OnPreCommand(std::function<void (const CRPCCommand&)> slot);
    void OnPostCommand(std::function<void (const CRPCCommand&)> slot);
}

class CBlockIndex;
class CNetAddr;

class JSONRPCRequest
{
public:
    UniValue id;
    std::string strMethod;
    UniValue params;
    bool fHelp;
    std::string URI;
    std::string authUser;

    JSONRPCRequest() { id = NullUniValue; params = NullUniValue; fHelp = false; }
    void parse(const UniValue& valRequest);
};

/** Query whether RPC is running */
bool IsRPCRunning();

/**
 * Set the RPC warmup status.  When this is done, all RPC calls will error out
 * immediately with RPC_IN_WARMUP.
 */
void SetRPCWarmupStatus(const std::string& newStatus);
/* Mark warmup as done.  RPC calls will be processed from now on.  */
void SetRPCWarmupFinished();

/* returns the current warmup state.  */
bool RPCIsInWarmup(std::string* statusOut);

/**
 * Type-check arguments; throws JSONRPCError if wrong type given. Does not check that
 * the right number of arguments are passed, just that any passed are the correct type.
 * Use like:  RPCTypeCheck(params, boost::assign::list_of(str_type)(int_type)(obj_type));
 */
void RPCTypeCheck(const UniValue& params,
                  const std::list<UniValue::VType>& typesExpected, bool fAllowNull=false);

/**
 * Check for expected keys/value types in an Object.
 * Use like: RPCTypeCheckObj(object, boost::assign::map_list_of("name", str_type)("value", int_type));
 */
void RPCTypeCheckObj(const UniValue& o,
                  const std::map<std::string, UniValue::VType>& typesExpected, bool fAllowNull=false, bool fStrict=false);

/** Opaque base class for timers returned by NewTimerFunc.
 * This provides no methods at the moment, but makes sure that delete
 * cleans up the whole state.
 */
class RPCTimerBase
{
public:
    virtual ~RPCTimerBase() {}
};

/**
* RPC timer "driver".
 */
class RPCTimerInterface
{
public:
    virtual ~RPCTimerInterface() {}
    /** Implementation name */
    virtual const char *Name() = 0;
    /** Factory function for timers.
     * RPC will call the function to create a timer that will call func in *millis* milliseconds.
     * @note As the RPC mechanism is backend-neutral, it can use different implementations of timers.
     * This is needed to cope with the case in which there is no HTTP server, but
     * only GUI RPC console, and to break the dependency of pcserver on httprpc.
     */
    virtual RPCTimerBase* NewTimer(std::function<void(void)>& func, int64_t millis) = 0;
};

/** Set factory function for timers */
void RPCSetTimerInterface(RPCTimerInterface *iface);
/** Set factory function for timers, but only if unset */
void RPCSetTimerInterfaceIfUnset(RPCTimerInterface *iface);
/** Unset factory function for timers */
void RPCUnsetTimerInterface(RPCTimerInterface *iface);

/**
 * Run func nSeconds from now.
 * Overrides previous timer <name> (if any).
 */
void RPCRunLater(const std::string& name, std::function<void(void)> func, int64_t nSeconds);

typedef UniValue(*rpcfn_type)(const JSONRPCRequest& jsonRequest);

class CRPCCommand
{
public:
    std::string category;
    std::string name;
    rpcfn_type actor;
    bool okSafeMode;
};

/**
 * PIVX RPC command dispatcher.
 */
class CRPCTable
{
private:
    std::map<std::string, const CRPCCommand*> mapCommands;

public:
    CRPCTable();
    const CRPCCommand* operator[](const std::string& name) const;
    std::string help(std::string name) const;

    /**
     * Execute a method.
     * @param request The JSONRPCRequest to execute
     * @returns Result of the call.
     * @throws an exception (UniValue) when an error happens.
     */
    UniValue execute(const JSONRPCRequest &request) const;

    /**
    * Returns a list of registered commands
    * @returns List of registered commands.
    */
    std::vector<std::string> listCommands() const;

    /**
     * Appends a CRPCCommand to the dispatch table.
     * Returns false if RPC server is already running (dump concurrency protection).
     * Commands cannot be overwritten (returns false).
     */
    bool appendCommand(const std::string& name, const CRPCCommand* pcmd);
};

bool IsDeprecatedRPCEnabled(const std::string& method);

extern CRPCTable tableRPC;

/**
 * Utilities: convert hex-encoded Values
 * (throws error if not hex).
 */
extern uint256 ParseHashV(const UniValue& v, std::string strName);
extern uint256 ParseHashO(const UniValue& o, std::string strKey);
extern std::vector<unsigned char> ParseHexV(const UniValue& v, std::string strName);
extern std::vector<unsigned char> ParseHexO(const UniValue& o, std::string strKey);
extern int ParseInt(const UniValue& o, std::string strKey);
extern bool ParseBool(const UniValue& o, std::string strKey);

extern int64_t nWalletUnlockTime;
extern CAmount AmountFromValue(const UniValue& value);
extern UniValue ValueFromAmount(const CAmount& amount);
extern double GetDifficulty(const CBlockIndex* blockindex = NULL);
extern std::string HelpRequiringPassphrase();
extern std::string HelpExampleCli(std::string methodname, std::string args);
extern std::string HelpExampleRpc(std::string methodname, std::string args);

extern void EnsureWalletIsUnlocked(bool fAllowAnonOnly = false);
// Ensure the wallet's existence.
extern void EnsureWallet();
extern UniValue DoZpivSpend(const CAmount nAmount, std::vector<CZerocoinMint>& vMintsSelected, std::string address_str);

extern UniValue getconnectioncount(const JSONRPCRequest& request); // in rpc/net.cpp
extern UniValue getpeerinfo(const JSONRPCRequest& request);
extern UniValue ping(const JSONRPCRequest& request);
extern UniValue addnode(const JSONRPCRequest& request);
extern UniValue disconnectnode(const JSONRPCRequest& request);
extern UniValue getaddednodeinfo(const JSONRPCRequest& request);
extern UniValue getnettotals(const JSONRPCRequest& request);
extern UniValue setban(const JSONRPCRequest& request);
extern UniValue listbanned(const JSONRPCRequest& request);
extern UniValue clearbanned(const JSONRPCRequest& request);

extern UniValue bip38encrypt(const JSONRPCRequest& request);
extern UniValue bip38decrypt(const JSONRPCRequest& request);

extern UniValue getgenerate(const JSONRPCRequest& request); // in rpc/mining.cpp
extern UniValue setgenerate(const JSONRPCRequest& request);
extern UniValue generate(const JSONRPCRequest& request);
extern UniValue getnetworkhashps(const JSONRPCRequest& request);
extern UniValue gethashespersec(const JSONRPCRequest& request);
extern UniValue getmininginfo(const JSONRPCRequest& request);
extern UniValue prioritisetransaction(const JSONRPCRequest& request);
extern UniValue getblocktemplate(const JSONRPCRequest& request);
extern UniValue submitblock(const JSONRPCRequest& request);
extern UniValue estimatefee(const JSONRPCRequest& request);
extern UniValue estimatepriority(const JSONRPCRequest& request);
extern UniValue getaddressinfo(const JSONRPCRequest& request);
extern UniValue getblockchaininfo(const JSONRPCRequest& request);
extern UniValue getnetworkinfo(const JSONRPCRequest& request);
extern UniValue multisend(const JSONRPCRequest& request);
extern UniValue getzerocoinbalance(const JSONRPCRequest& request);
extern UniValue listmintedzerocoins(const JSONRPCRequest& request);
extern UniValue listspentzerocoins(const JSONRPCRequest& request);
extern UniValue listzerocoinamounts(const JSONRPCRequest& request);
extern UniValue mintzerocoin(const JSONRPCRequest& request);
extern UniValue spendzerocoin(const JSONRPCRequest& request);
extern UniValue spendrawzerocoin(const JSONRPCRequest& request);
extern UniValue spendzerocoinmints(const JSONRPCRequest& request);
extern UniValue resetmintzerocoin(const JSONRPCRequest& request);
extern UniValue resetspentzerocoin(const JSONRPCRequest& request);
extern UniValue getarchivedzerocoin(const JSONRPCRequest& request);
extern UniValue importzerocoins(const JSONRPCRequest& request);
extern UniValue exportzerocoins(const JSONRPCRequest& request);
extern UniValue reconsiderzerocoins(const JSONRPCRequest& request);
extern UniValue getspentzerocoinamount(const JSONRPCRequest& request);
extern UniValue setzpivseed(const JSONRPCRequest& request);
extern UniValue getzpivseed(const JSONRPCRequest& request);
extern UniValue generatemintlist(const JSONRPCRequest& request);
extern UniValue searchdzpiv(const JSONRPCRequest& request);
extern UniValue dzpivstate(const JSONRPCRequest& request);

extern UniValue getrawtransaction(const JSONRPCRequest& request); // in rpc/rawtransaction.cpp
extern UniValue createrawtransaction(const JSONRPCRequest& request);
extern UniValue decoderawtransaction(const JSONRPCRequest& request);
extern UniValue decodescript(const JSONRPCRequest& request);
extern UniValue fundrawtransaction(const JSONRPCRequest& request);
extern UniValue signrawtransaction(const JSONRPCRequest& request);
extern UniValue sendrawtransaction(const JSONRPCRequest& request);
extern UniValue createrawzerocoinspend(const JSONRPCRequest& request);

extern UniValue findserial(const JSONRPCRequest& request); // in rpc/blockchain.cpp
extern UniValue getblockcount(const JSONRPCRequest& request);
extern UniValue getbestblockhash(const JSONRPCRequest& request);
extern UniValue waitfornewblock(const JSONRPCRequest& request);
extern UniValue waitforblock(const JSONRPCRequest& request);
extern UniValue waitforblockheight(const JSONRPCRequest& request);
extern UniValue getdifficulty(const JSONRPCRequest& request);
extern UniValue getmempoolinfo(const JSONRPCRequest& request);
extern UniValue getrawmempool(const JSONRPCRequest& request);
extern UniValue getblockhash(const JSONRPCRequest& request);
extern UniValue getblock(const JSONRPCRequest& request);
extern UniValue getblockheader(const JSONRPCRequest& request);
extern UniValue getfeeinfo(const JSONRPCRequest& request);
extern UniValue gettxoutsetinfo(const JSONRPCRequest& request);
extern UniValue gettxout(const JSONRPCRequest& request);
extern UniValue verifychain(const JSONRPCRequest& request);
extern UniValue getchaintips(const JSONRPCRequest& request);
extern UniValue invalidateblock(const JSONRPCRequest& request);
extern UniValue reconsiderblock(const JSONRPCRequest& request);
extern UniValue getblockindexstats(const JSONRPCRequest& request);
extern UniValue getserials(const JSONRPCRequest& request);
extern void validaterange(const UniValue& params, int& heightStart, int& heightEnd, int minHeightStart=1);

// in rpc/masternode.cpp
extern UniValue listmasternodes(const JSONRPCRequest& request);
extern UniValue getmasternodecount(const JSONRPCRequest& request);
extern UniValue createmasternodebroadcast(const JSONRPCRequest& request);
extern UniValue decodemasternodebroadcast(const JSONRPCRequest& request);
extern UniValue relaymasternodebroadcast(const JSONRPCRequest& request);
extern UniValue masternodecurrent(const JSONRPCRequest& request);
extern UniValue startmasternode(const JSONRPCRequest& request);
extern UniValue createmasternodekey(const JSONRPCRequest& request);
extern UniValue getmasternodeoutputs(const JSONRPCRequest& request);
extern UniValue listmasternodeconf(const JSONRPCRequest& request);
extern UniValue getmasternodestatus(const JSONRPCRequest& request);
extern UniValue getmasternodewinners(const JSONRPCRequest& request);
extern UniValue getmasternodescores(const JSONRPCRequest& request);

extern UniValue preparebudget(const JSONRPCRequest& request); // in rpc/budget.cpp
extern UniValue submitbudget(const JSONRPCRequest& request);
extern UniValue mnbudgetvote(const JSONRPCRequest& request);
extern UniValue getbudgetvotes(const JSONRPCRequest& request);
extern UniValue getnextsuperblock(const JSONRPCRequest& request);
extern UniValue getbudgetprojection(const JSONRPCRequest& request);
extern UniValue getbudgetinfo(const JSONRPCRequest& request);
extern UniValue mnbudgetrawvote(const JSONRPCRequest& request);
extern UniValue mnfinalbudget(const JSONRPCRequest& request);
extern UniValue checkbudgets(const JSONRPCRequest& request);

extern UniValue getinfo(const JSONRPCRequest& request); // in rpc/misc.cpp
extern UniValue logging(const JSONRPCRequest& request);
extern UniValue mnsync(const JSONRPCRequest& request);
extern UniValue spork(const JSONRPCRequest& request);
extern UniValue validateaddress(const JSONRPCRequest& request);
extern UniValue createmultisig(const JSONRPCRequest& request);
extern UniValue verifymessage(const JSONRPCRequest& request);
extern UniValue setmocktime(const JSONRPCRequest& request);
extern UniValue getstakingstatus(const JSONRPCRequest& request);

bool StartRPC();
void InterruptRPC();
void StopRPC();
std::string JSONRPCExecBatch(const UniValue& vReq);
void RPCNotifyBlockChange(bool fInitialDownload, const CBlockIndex* pindex);

#endif // BITCOIN_RPCSERVER_H
