#ifndef TOKENCORE_SP_H
#define TOKENCORE_SP_H

#include "tokencore/dbbase.h"
#include "tokencore/dbspinfo.h"
#include "tokencore/log.h"
#include "tokencore/tokencore.h"

class CBlockIndex;
class uint256;

#include <openssl/sha.h>

#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>


/** A live crowdsale.
 */
class CMPCrowd
{
private:
    uint32_t propertyId;
    int64_t nValue;

    uint32_t property_desired;
    int64_t deadline;
    uint8_t early_bird;
    uint8_t percentage;

    int64_t u_created;
    int64_t i_created;

    uint256 txid; // NOTE: not persisted as it doesnt seem used

    // Schema:
    //   txid -> amount invested, crowdsale deadline, user issued tokens, issuer issued tokens
    std::map<uint256, std::vector<int64_t> > txFundraiserData;

public:
    CMPCrowd();
    CMPCrowd(uint32_t pid, int64_t nv, uint32_t cd, int64_t dl, uint8_t eb, uint8_t per, int64_t uct, int64_t ict);

    uint32_t getPropertyId() const { return propertyId; }

    int64_t getDeadline() const { return deadline; }
    uint32_t getCurrDes() const { return property_desired; }

    void incTokensUserCreated(int64_t amount) { u_created += amount; }
    void incTokensIssuerCreated(int64_t amount) { i_created += amount; }

    int64_t getUserCreated() const { return u_created; }
    int64_t getIssuerCreated() const { return i_created; }

    void insertDatabase(const uint256& txHash, const std::vector<int64_t>& txData);
    std::map<uint256, std::vector<int64_t> > getDatabase() const { return txFundraiserData; }

    std::string toString(const std::string& address) const;
    void print(const std::string& address, FILE* fp = stdout) const;
    void saveCrowdSale(std::ofstream& file, SHA256_CTX* shaCtx, const std::string& addr) const;
};

namespace mastercore
{
typedef std::map<std::string, CMPCrowd> CrowdMap;

//! LevelDB based storage for currencies, smart properties and tokens
extern CMPSPInfo* pDbSpInfo;
//! In-memory collection of active crowdsales
extern CrowdMap my_crowds;

std::string strPropertyType(uint16_t propertyType);
std::string strEcosystem(uint8_t ecosystem);

std::string getPropertyName(uint32_t propertyId);
std::string getPropertyTicker(uint32_t propertyId);
bool isPropertyDivisible(uint32_t propertyId);
bool IsPropertyIdValid(uint32_t propertyId);

CMPCrowd* getCrowd(const std::string& address);

bool isCrowdsaleActive(uint32_t propertyId);
bool isCrowdsalePurchase(const uint256& txid, const std::string& address, int64_t* propertyId, int64_t* userTokens, int64_t* issuerTokens);

/** Calculates missing bonus tokens, which are credited to the crowdsale issuer. */
int64_t GetMissedIssuerBonus(const CMPSPInfo::Entry& sp, const CMPCrowd& crowdsale);

/** Calculates amounts credited for a crowdsale purchase. */
void calculateFundraiser(bool inflateAmount, int64_t amtTransfer, uint8_t bonusPerc,
        int64_t fundraiserSecs, int64_t currentSecs, int64_t numProps, uint8_t issuerPerc, int64_t totalTokens,
        std::pair<int64_t, int64_t>& tokens, bool& close_crowdsale);

void eraseMaxedCrowdsale(const std::string& address, int64_t blockTime, int block);

unsigned int eraseExpiredCrowdsale(const CBlockIndex* pBlockIndex);
}


#endif // TOKENCORE_SP_H
