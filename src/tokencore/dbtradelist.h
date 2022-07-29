#ifndef TOKENCORE_DBTRADELIST_H
#define TOKENCORE_DBTRADELIST_H

#include "tokencore/dbbase.h"

#include "uint256.h"

#include <univalue.h>

#include <boost/filesystem/path.hpp>

#include <stdint.h>

#include <string>
#include <vector>

/** LevelDB based storage for the MetaDEx trade history. Trades are listed with key "txid1+txid2".
 */
class CMPTradeList : public CDBBase
{
public:
    CMPTradeList(const boost::filesystem::path& path, bool fWipe);
    virtual ~CMPTradeList();

    void recordMatchedTrade(const uint256& txid1, const uint256& txid2, const std::string& address1, const std::string& address2, uint32_t prop1, uint32_t prop2, int64_t amount1, int64_t amount2, int blockNum, int64_t fee);
    void recordNewTrade(const uint256& txid, const std::string& address, uint32_t propertyIdForSale, uint32_t propertyIdDesired, int blockNum, int blockIndex);
    int deleteAboveBlock(int blockNum);
    bool exists(const uint256 &txid);
    void printStats();
    void printAll();
    bool getMatchingTrades(const uint256& txid, uint32_t propertyId, UniValue& tradeArray, int64_t& totalSold, int64_t& totalBought);
    void getTradesForAddress(const std::string& address, std::vector<uint256>& vecTransactions, uint32_t propertyIdFilter = 0);
    void getTradesForPair(uint32_t propertyIdSideA, uint32_t propertyIdSideB, UniValue& response, uint64_t count);
    int getMPTradeCountTotal();
};

namespace mastercore
{
    //! LevelDB based storage for the MetaDEx trade history
    extern CMPTradeList* pDbTradeList;
}

#endif // TOKENCORE_DBTRADELIST_H
