#ifndef TOKENCORE_DBTRANSACTION_H
#define TOKENCORE_DBTRANSACTION_H

#include "tokencore/dbbase.h"

#include "uint256.h"

#include <boost/filesystem/path.hpp>

#include <stdint.h>

#include <string>
#include <vector>

/** LevelDB based storage for storing Token transaction validation and position in block data.
 */
class CTokenTransactionDB : public CDBBase
{
public:
    CTokenTransactionDB(const boost::filesystem::path& path, bool fWipe);
    virtual ~CTokenTransactionDB();

    /** Stores position in block and validation result for a transaction. */
    void RecordTransaction(const uint256& txid, uint32_t posInBlock, int processingResult);

    /** Returns the position of a transaction in a block. */
    uint32_t FetchTransactionPosition(const uint256& txid);

    /** Returns the reason why a transaction is invalid. */
    std::string FetchInvalidReason(const uint256& txid);

private:
    /** Retrieves the serialized transaction details from the DB. */
    std::vector<std::string> FetchTransactionDetails(const uint256& txid);
};

namespace mastercore
{
    //! LevelDB based storage for storing Token transaction validation and position in block data
    extern CTokenTransactionDB* pDbTransaction;
}

#endif // TOKENCORE_DBTRANSACTION_H

