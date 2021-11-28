#ifndef TOKENCORE_RPCTXOBJECT_H
#define TOKENCORE_RPCTXOBJECT_H

#include <univalue.h>

#include <string>

class uint256;
class CMPTransaction;
class CTransaction;

int populateRPCTransactionObject(const uint256& txid, UniValue& txobj, std::string filterAddress = "", bool extendedDetails = false, std::string extendedDetailsFilter = "");
int populateRPCTransactionObject(const CTransaction& tx, const uint256& blockHash, UniValue& txobj, std::string filterAddress = "", bool extendedDetails = false, std::string extendedDetailsFilter = "", int blockHeight = 0);

void populateRPCTypeInfo(CMPTransaction& mp_obj, UniValue& txobj, uint32_t txType, bool extendedDetails, std::string extendedDetailsFilter, int confirmations);

void populateRPCTypeSimpleSend(CMPTransaction& tokenObj, UniValue& txobj);
void populateRPCTypeSendToOwners(CMPTransaction& tokenObj, UniValue& txobj, bool extendedDetails, std::string extendedDetailsFilter);
void populateRPCTypeSendAll(CMPTransaction& tokenObj, UniValue& txobj, int confirmations);
void populateRPCTypeTradeOffer(CMPTransaction& tokenObj, UniValue& txobj);
void populateRPCTypeMetaDExTrade(CMPTransaction& tokenObj, UniValue& txobj, bool extendedDetails);
void populateRPCTypeMetaDExCancelPrice(CMPTransaction& tokenObj, UniValue& txobj, bool extendedDetails);
void populateRPCTypeMetaDExCancelPair(CMPTransaction& tokenObj, UniValue& txobj, bool extendedDetails);
void populateRPCTypeMetaDExCancelEcosystem(CMPTransaction& tokenObj, UniValue& txobj, bool extendedDetails);
void populateRPCTypeAcceptOffer(CMPTransaction& tokenObj, UniValue& txobj);
void populateRPCTypeCreatePropertyFixed(CMPTransaction& tokenObj, UniValue& txobj, int confirmations);
void populateRPCTypeCreatePropertyVariable(CMPTransaction& tokenObj, UniValue& txobj, int confirmations);
void populateRPCTypeCreatePropertyManual(CMPTransaction& tokenObj, UniValue& txobj, int confirmations);
void populateRPCTypeCloseCrowdsale(CMPTransaction& tokenObj, UniValue& txobj);
void populateRPCTypeGrant(CMPTransaction& tokenObj, UniValue& txobj);
void populateRPCTypeRevoke(CMPTransaction& tokenOobj, UniValue& txobj);
void populateRPCTypeChangeIssuer(CMPTransaction& tokenObj, UniValue& txobj);
void populateRPCTypeActivation(CMPTransaction& tokenObj, UniValue& txobj);
void populateRPCTypeEnableFreezing(CMPTransaction& tokenObj, UniValue& txobj);
void populateRPCTypeDisableFreezing(CMPTransaction& tokenObj, UniValue& txobj);
void populateRPCTypeFreezeTokens(CMPTransaction& tokenObj, UniValue& txobj);
void populateRPCTypeUnfreezeTokens(CMPTransaction& tokenObj, UniValue& txobj);

void populateRPCExtendedTypeSendToOwners(const uint256 txid, std::string extendedDetailsFilter, UniValue& txobj, uint16_t version);
void populateRPCExtendedTypeMetaDExTrade(const uint256& txid, uint32_t propertyIdForSale, int64_t amountForSale, UniValue& txobj);
void populateRPCExtendedTypeMetaDExCancel(const uint256& txid, UniValue& txobj);

int populateRPCDExPurchases(const CTransaction& wtx, UniValue& purchases, std::string filterAddress);
int populateRPCSendAllSubSends(const uint256& txid, UniValue& subSends);

bool showRefForTx(uint32_t txType);

#endif // TOKENCORE_RPCTXOBJECT_H
