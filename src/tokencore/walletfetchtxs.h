#ifndef TOKENCORE_WALLETFETCHTXS_H
#define TOKENCORE_WALLETFETCHTXS_H

class uint256;

#include <map>
#include <string>

namespace mastercore
{
/** Returns an ordered list of Token transactions that are relevant to the wallet. */
std::map<std::string, uint256> FetchWalletTokenTransactions(unsigned int count, int startBlock = 0, int endBlock = 999999999);
}

#endif // TOKENCORE_WALLETFETCHTXS_H
