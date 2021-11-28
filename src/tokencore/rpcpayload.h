#ifndef TOKENCORE_RPCPAYLOAD_H
#define TOKENCORE_RPCPAYLOAD_H

#include <univalue.h>

UniValue token_createpayload_simplesend(const UniValue& params, bool fHelp);
UniValue token_createpayload_sendall(const UniValue& params, bool fHelp);
UniValue token_createpayload_dexsell(const UniValue& params, bool fHelp);
UniValue token_createpayload_dexaccept(const UniValue& params, bool fHelp);
UniValue token_createpayload_sto(const UniValue& params, bool fHelp);
UniValue token_createpayload_issuancefixed(const UniValue& params, bool fHelp);
UniValue token_createpayload_issuancecrowdsale(const UniValue& params, bool fHelp);
UniValue token_createpayload_issuancemanaged(const UniValue& params, bool fHelp);
UniValue token_createpayload_closecrowdsale(const UniValue& params, bool fHelp);
UniValue token_createpayload_grant(const UniValue& params, bool fHelp);
UniValue token_createpayload_revoke(const UniValue& params, bool fHelp);
UniValue token_createpayload_changeissuer(const UniValue& params, bool fHelp);
UniValue token_createpayload_trade(const UniValue& params, bool fHelp);
UniValue token_createpayload_canceltradesbyprice(const UniValue& params, bool fHelp);
UniValue token_createpayload_canceltradesbypair(const UniValue& params, bool fHelp);
UniValue token_createpayload_cancelalltrades(const UniValue& params, bool fHelp);

void RegisterTokenPayloadCreationRPCCommands();

#endif // TOKENCORE_RPCPAYLOAD_H
