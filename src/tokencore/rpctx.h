#ifndef TOKENCORE_RPCTX
#define TOKENCORE_RPCTX

#include <univalue.h>

UniValue sendtokenrawtx(const UniValue& params, bool fHelp);
UniValue sendtoken(const UniValue& params, bool fHelp);
UniValue sendalltokens(const UniValue& params, bool fHelp);
UniValue sendtokendexsell(const UniValue& params, bool fHelp);
UniValue sendtokendexaccept(const UniValue& params, bool fHelp);
UniValue sendtokenissuancecrowdsale(const UniValue& params, bool fHelp);
UniValue sendtokenissuancefixed(const UniValue& params, bool fHelp);
UniValue sendtokenissuancemanaged(const UniValue& params, bool fHelp);
UniValue token_sendsto(const UniValue& params, bool fHelp);
UniValue sendtokengrant(const UniValue& params, bool fHelp);
UniValue sendtokenrevoke(const UniValue& params, bool fHelp);
UniValue sendtokenclosecrowdsale(const UniValue& params, bool fHelp);
UniValue trade_MP(const UniValue& params, bool fHelp);
UniValue token_sendtrade(const UniValue& params, bool fHelp);
UniValue token_sendcanceltradesbyprice(const UniValue& params, bool fHelp);
UniValue token_sendcanceltradesbypair(const UniValue& params, bool fHelp);
UniValue token_sendcancelalltrades(const UniValue& params, bool fHelp);
UniValue sendtokenchangeissuer(const UniValue& params, bool fHelp);
UniValue token_sendactivation(const UniValue& params, bool fHelp);
UniValue token_sendalert(const UniValue& params, bool fHelp);

void RegisterTokenTransactionCreationRPCCommands();

#endif // TOKENCORE_RPCTX
