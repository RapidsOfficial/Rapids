#ifndef TOKENCORE_WALLETCACHE_H
#define TOKENCORE_WALLETCACHE_H

class uint256;

#include <vector>

namespace mastercore
{
/** Updates the cache and returns whether any wallet addresses were changed */
int WalletCacheUpdate();
}

#endif // TOKENCORE_WALLETCACHE_H
