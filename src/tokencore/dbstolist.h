#ifndef TOKENCORE_DBSTOLIST_H
#define TOKENCORE_DBSTOLIST_H

#include "tokencore/dbbase.h"

#include "uint256.h"

#include <univalue.h>

#include <boost/filesystem/path.hpp>

#include <stdint.h>

#include <string>

/** LevelDB based storage for STO recipients.
 */
class CMPSTOList : public CDBBase
{
public:
    CMPSTOList(const boost::filesystem::path& path, bool fWipe);
    virtual ~CMPSTOList();

    void getRecipients(const uint256 txid, std::string filterAddress, UniValue* recipientArray, uint64_t* total, uint64_t* numRecipients);
    std::string getMySTOReceipts(std::string filterAddress);
    
    /**
     * This function deletes records of STO receivers above/equal to a specific block from the STO database.
     *
     * Returns the number of records changed.
     */
    int deleteAboveBlock(int blockNum);
    void printStats();
    void printAll();
    bool exists(std::string address);
    void recordSTOReceive(std::string, const uint256&, int, unsigned int, uint64_t);
};

namespace mastercore
{
    //! LevelDB based storage for STO recipients
    extern CMPSTOList* pDbStoList;
}

#endif // TOKENCORE_DBSTOLIST_H
