#ifndef TOKENCORE_TOKENCORE_H
#define TOKENCORE_TOKENCORE_H

class CBlockIndex;
class CCoinsView;
class CCoinsViewCache;
class CTransaction;
class Coin;

#include "tokencore/log.h"
#include "tokencore/tally.h"

#include <script/standard.h>

#include "sync.h"
#include "uint256.h"
#include "util.h"

#include <univalue.h>

#include <stdint.h>

#include <map>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>

int const MAX_STATE_HISTORY = 50;
int const STORE_EVERY_N_BLOCK = 10000;

#define TEST_ECO_PROPERTY_1 (0x80000003UL)

#define RPD_PROPERTY_ID 0

// increment this value to force a refresh of the state (similar to --startclean)
#define DB_VERSION 7

// could probably also use: int64_t maxInt64 = std::numeric_limits<int64_t>::max();
// maximum numeric values from the spec:
#define MAX_INT_8_BYTES (9223372036854775807UL)

// maximum size of string fields
#define SP_STRING_FIELD_LEN 256

// RPDx Transaction (Packet) Version
#define MP_TX_PKT_V0  0
#define MP_TX_PKT_V1  1
#define MP_TX_PKT_V2  2


// Transaction types, from the spec
enum TransactionType {
  TOKEN_TYPE_SIMPLE_SEND                =  0,
  TOKEN_TYPE_RESTRICTED_SEND            =  2,
  TOKEN_TYPE_SEND_TO_OWNERS             =  3,
  TOKEN_TYPE_SEND_ALL                   =  4,
  TOKEN_TYPE_SEND_TO_MANY               =  5,
  TOKEN_TYPE_SAVINGS_MARK               = 10,
  TOKEN_TYPE_SAVINGS_COMPROMISED        = 11,
  TOKEN_TYPE_RATELIMITED_MARK           = 12,
  TOKEN_TYPE_AUTOMATIC_DISPENSARY       = 15,
  TOKEN_TYPE_TRADE_OFFER                = 20,
  TOKEN_TYPE_ACCEPT_OFFER_RPD           = 22,
  TOKEN_TYPE_METADEX_TRADE              = 25,
  TOKEN_TYPE_METADEX_CANCEL_PRICE       = 26,
  TOKEN_TYPE_METADEX_CANCEL_PAIR        = 27,
  TOKEN_TYPE_METADEX_CANCEL_ECOSYSTEM   = 28,
  TOKEN_TYPE_NOTIFICATION               = 31,
  TOKEN_TYPE_OFFER_ACCEPT_A_BET         = 40,
  TOKEN_TYPE_CREATE_PROPERTY_FIXED      = 50,
  TOKEN_TYPE_CREATE_PROPERTY_VARIABLE   = 51,
  TOKEN_TYPE_PROMOTE_PROPERTY           = 52,
  TOKEN_TYPE_CLOSE_CROWDSALE            = 53,
  TOKEN_TYPE_CREATE_PROPERTY_MANUAL     = 54,
  TOKEN_TYPE_GRANT_PROPERTY_TOKENS      = 55,
  TOKEN_TYPE_REVOKE_PROPERTY_TOKENS     = 56,
  TOKEN_TYPE_CHANGE_ISSUER_ADDRESS      = 70,
  TOKEN_TYPE_ENABLE_FREEZING            = 71,
  TOKEN_TYPE_DISABLE_FREEZING           = 72,
  TOKEN_TYPE_RAPIDS_PAYMENT             = 80,
  TOKEN_TYPE_FREEZE_PROPERTY_TOKENS     = 185,
  TOKEN_TYPE_UNFREEZE_PROPERTY_TOKENS   = 186,
  TOKENCORE_MESSAGE_TYPE_DEACTIVATION  = 65533,
  TOKENCORE_MESSAGE_TYPE_ACTIVATION    = 65534,
  TOKENCORE_MESSAGE_TYPE_ALERT         = 65535
};

#define TOKEN_PROPERTY_TYPE_INDIVISIBLE             1
#define TOKEN_PROPERTY_TYPE_DIVISIBLE               2
#define TOKEN_PROPERTY_TYPE_INDIVISIBLE_REPLACING   65
#define TOKEN_PROPERTY_TYPE_DIVISIBLE_REPLACING     66
#define TOKEN_PROPERTY_TYPE_INDIVISIBLE_APPENDING   129
#define TOKEN_PROPERTY_TYPE_DIVISIBLE_APPENDING     130

#define PKT_RETURNED_OBJECT    (1000)

#define PKT_ERROR             ( -9000)
#define DEX_ERROR_SELLOFFER   (-10000)
#define DEX_ERROR_ACCEPT      (-20000)
#define DEX_ERROR_PAYMENT     (-30000)
// Smart Properties
#define PKT_ERROR_SP          (-40000)
#define PKT_ERROR_CROWD       (-45000)
// Send To Owners
#define PKT_ERROR_STO         (-50000)
#define PKT_ERROR_SEND        (-60000)
#define PKT_ERROR_TRADEOFFER  (-70000)
#define PKT_ERROR_METADEX     (-80000)
#define METADEX_ERROR         (-81000)
#define PKT_ERROR_TOKENS      (-82000)
#define PKT_ERROR_SEND_ALL    (-83000)
#define PKT_ERROR_SEND_MANY   (-84000)

#define TOKEN_PROPERTY_RPD   0
#define TOKEN_PROPERTY_MSC   1
#define TOKEN_PROPERTY_TMSC  2

/** Number formating related functions. */
std::string FormatDivisibleMP(int64_t amount, bool fSign = false);
std::string FormatDivisibleShortMP(int64_t amount);
std::string FormatIndivisibleMP(int64_t amount);
std::string FormatByType(int64_t amount, uint16_t propertyType);
// Note: require initialized state to get divisibility.
std::string FormatMP(uint32_t propertyId, int64_t amount, bool fSign = false);
std::string FormatShortMP(uint32_t propertyId, int64_t amount);

/** Returns the Exodus address. */
const CTxDestination ExodusAddress();

/** Returns the donation address. */
const CTxDestination DonationAddress();

/** Returns the marker for class C transactions. */
const std::vector<unsigned char> GetOmMarker();

//! Used to indicate, whether to automatically commit created transactions
extern bool autoCommit;

//! Global lock for state objects
extern RecursiveMutex cs_tally;

//! Available balances of wallet properties
extern std::map<uint32_t, int64_t> global_balance_money;
//! Reserved balances of wallet propertiess
extern std::map<uint32_t, int64_t> global_balance_reserved;
//! Frozen balances of wallet propertiess
extern std::map<uint32_t, int64_t> global_balance_frozen;

extern std::map<uint32_t, std::list<std::string>> global_token_addresses;

//! Vector containing a list of properties relative to the wallet
extern std::set<uint32_t> global_wallet_property_list;

int64_t GetTokenBalance(const std::string& address, uint32_t propertyId, TallyType ttype);
int64_t GetAvailableTokenBalance(const std::string& address, uint32_t propertyId);
int64_t GetReservedTokenBalance(const std::string& address, uint32_t propertyId);
int64_t GetFrozenTokenBalance(const std::string& address, uint32_t propertyId);

/** Global handler to initialize Token Core. */
int mastercore_init();

/** Global handler to shut down Token Core. */
int mastercore_shutdown();

/** Block and transaction handlers. */
int mastercore_handler_disc_begin(int nBlockNow, CBlockIndex const * pBlockIndex);
int mastercore_handler_disc_end(int nBlockNow, CBlockIndex const * pBlockIndex);
int mastercore_handler_block_begin(int nBlockNow, CBlockIndex const * pBlockIndex);
int mastercore_handler_block_end(int nBlockNow, CBlockIndex const * pBlockIndex, unsigned int);
bool mastercore_handler_tx(const CTransaction& tx, int nBlock, unsigned int idx, const CBlockIndex* pBlockIndex);

/** Scans for marker and if one is found, add transaction to marker cache. */
void TryToAddToMarkerCache(const CTransaction& tx);
/** Removes transaction from marker cache. */
void RemoveFromMarkerCache(const CTransaction& tx);
/** Checks, if transaction is in marker cache. */
bool IsInMarkerCache(const uint256& txHash);

/** Global handler to total wallet balances. */
void CheckWalletUpdate(bool forceUpdate = false);

/** Used to notify that the number of tokens for a property has changed. */
void NotifyTotalTokensChanged(uint32_t propertyId, int block);

int64_t GetRapidsPaymentAmount(const uint256& txid, const std::string& recipient);

namespace mastercore
{
//! In-memory collection of all amounts for all addresses for all properties
extern std::unordered_map<std::string, CMPTally> mp_tally_map;

// TODO: move, rename
extern CCoinsView viewDummy;
extern CCoinsViewCache view;
//! Guards coins view cache
extern RecursiveMutex cs_tx_cache;

/** Returns the encoding class, used to embed a payload. */
int GetEncodingClass(const CTransaction& tx, int nBlock);

/** Determines, whether it is valid to use a Class C transaction for a given payload size. */
bool UseEncodingClassC(size_t nDataSize);

bool isTestEcosystemProperty(uint32_t propertyId);
bool isMainEcosystemProperty(uint32_t propertyId);
uint32_t GetNextPropertyId(bool maineco); // maybe move into sp

CMPTally* getTally(const std::string& address);
bool update_tally_map(const std::string& who, uint32_t propertyId, int64_t amount, TallyType ttype);
int64_t getTotalTokens(uint32_t propertyId, int64_t* n_owners_total = NULL);

std::string strMPProperty(uint32_t propertyId);
std::string strTransactionType(uint16_t txType);
std::string getTokenLabel(uint32_t propertyId);

/**
    NOTE: The following functions are only permitted for properties
          managed by a central issuer that have enabled freezing.
 **/
/** Adds an address and property to the frozenMap **/
void freezeAddress(const std::string& address, uint32_t propertyId);
/** Removes an address and property from the frozenMap **/
void unfreezeAddress(const std::string& address, uint32_t propertyId);
/** Checks whether an address and property are frozen **/
bool isAddressFrozen(const std::string& address, uint32_t propertyId);
/** Adds a property to the freezingEnabledMap **/
void enableFreezing(uint32_t propertyId, int liveBlock);
/** Removes a property from the freezingEnabledMap **/
void disableFreezing(uint32_t propertyId);
/** Checks whether a property has freezing enabled **/
bool isFreezingEnabled(uint32_t propertyId, int block);
/** Clears the freeze state in the event of a reorg **/
void ClearFreezeState();
/** Prints the freeze state **/
void PrintFreezeState();

}

std::string GetUsernameAddress(std::string username);

#endif // TOKENCORE_TOKENCORE_H
