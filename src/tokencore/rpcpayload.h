#ifndef TOKENCORE_RPCPAYLOAD_H
#define TOKENCORE_RPCPAYLOAD_H

#include <univalue.h>

UniValue createtokenpayloadsimplesend(const UniValue& params, bool fHelp);
UniValue token_createpayload_sendall(const UniValue& params, bool fHelp);
UniValue token_createpayload_dexsell(const UniValue& params, bool fHelp);
UniValue token_createpayload_dexaccept(const UniValue& params, bool fHelp);
UniValue token_createpayload_sto(const UniValue& params, bool fHelp);
UniValue createtokenpayloadissuancefixed(const UniValue& params, bool fHelp);
UniValue createtokenpayloadissuancecrowdsale(const UniValue& params, bool fHelp);
UniValue createtokenpayloadissuancemanaged(const UniValue& params, bool fHelp);
UniValue createtokenpayloadclosecrowdsale(const UniValue& params, bool fHelp);
UniValue createtokenpayloadgrant(const UniValue& params, bool fHelp);
UniValue createtokenpayloadrevoke(const UniValue& params, bool fHelp);
UniValue createtokenpayloadchangeissuer(const UniValue& params, bool fHelp);
UniValue token_createpayload_trade(const UniValue& params, bool fHelp);
UniValue token_createpayload_canceltradesbyprice(const UniValue& params, bool fHelp);
UniValue token_createpayload_canceltradesbypair(const UniValue& params, bool fHelp);
UniValue token_createpayload_cancelalltrades(const UniValue& params, bool fHelp);

void RegisterTokenPayloadCreationRPCCommands();

#endif // TOKENCORE_RPCPAYLOAD_H
