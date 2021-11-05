#ifndef OMNICORE_RPCRAWTX_H
#define OMNICORE_RPCRAWTX_H

#include <univalue.h>

UniValue decodetransaction(const UniValue& params, bool fHelp);
UniValue createrawtokentxopreturn(const UniValue& params, bool fHelp);
UniValue createrawtokentxmultisig(const UniValue& params, bool fHelp);
UniValue createrawtokentxinput(const UniValue& params, bool fHelp);
UniValue createrawtokentxreference(const UniValue& params, bool fHelp);
UniValue createrawtokentxchange(const UniValue& params, bool fHelp);

void RegisterOmniRawTransactionRPCCommands();

#endif // OMNICORE_RPCRAWTX_H