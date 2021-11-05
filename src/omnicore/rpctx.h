#ifndef OMNICORE_RPCTX
#define OMNICORE_RPCTX

#include <univalue.h>

UniValue omni_sendrawtx(const UniValue& params, bool fHelp);
UniValue sendtoken(const UniValue& params, bool fHelp);
UniValue sendalltokens(const UniValue& params, bool fHelp);
UniValue omni_senddexsell(const UniValue& params, bool fHelp);
UniValue omni_senddexaccept(const UniValue& params, bool fHelp);
UniValue omni_sendissuancecrowdsale(const UniValue& params, bool fHelp);
UniValue sendtokenissuancefixed(const UniValue& params, bool fHelp);
UniValue sendtokenissuancemanaged(const UniValue& params, bool fHelp);
UniValue omni_sendsto(const UniValue& params, bool fHelp);
UniValue omni_sendgrant(const UniValue& params, bool fHelp);
UniValue omni_sendrevoke(const UniValue& params, bool fHelp);
UniValue omni_sendclosecrowdsale(const UniValue& params, bool fHelp);
UniValue trade_MP(const UniValue& params, bool fHelp);
UniValue omni_sendtrade(const UniValue& params, bool fHelp);
UniValue omni_sendcanceltradesbyprice(const UniValue& params, bool fHelp);
UniValue omni_sendcanceltradesbypair(const UniValue& params, bool fHelp);
UniValue omni_sendcancelalltrades(const UniValue& params, bool fHelp);
UniValue omni_sendchangeissuer(const UniValue& params, bool fHelp);
UniValue omni_sendactivation(const UniValue& params, bool fHelp);
UniValue omni_sendalert(const UniValue& params, bool fHelp);

void RegisterOmniTransactionCreationRPCCommands();

#endif // OMNICORE_RPCTX
