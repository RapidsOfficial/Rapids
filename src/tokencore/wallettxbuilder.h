#ifndef TOKENCORE_WALLETTXBUILDER_H
#define TOKENCORE_WALLETTXBUILDER_H

class uint256;

#include <stdint.h>
#include <string>
#include <vector>

/**
 * Creates and sends a transaction.
 */
int WalletTxBuilder(
        const std::string& senderAddress,
        const std::string& receiverAddress,
        const std::string& redemptionAddress,
        int64_t referenceAmount,
        const std::vector<unsigned char>& payload,
        uint256& retTxid,
        std::string& retRawTx,
        bool commit,
        bool nDonation = false);

/**
 * Creates and sends a transaction with multiple receivers.
 */
int WalletTxBuilder(
        const std::string& senderAddress,
        const std::vector<std::string>& receiverAddresses,
        const std::string& redemptionAddress,
        int64_t referenceAmount,
        const std::vector<unsigned char>& payload,
        uint256& retTxid,
        std::string& retRawTx,
        bool commit,
        bool nDonation = false);

/**
 * Simulates the creation of a payload to count the required outputs.
 */
int GetDryPayloadOutputCount(
        const std::string& senderAddress,
        const std::string& redemptionAddress,
        const std::vector<unsigned char>& payload);

/**
 * Creates and sends a raw transaction by selecting all coins from the sender
 * and enough coins from a fee source. Change is sent to the fee source!
 */
int CreateFundedTransaction(
        const std::string& senderAddress,
        const std::string& receiverAddress,
        const std::string& feeAddress,
        const std::vector<unsigned char>& payload,
        uint256& retTxid);

#ifdef ENABLE_WALLET
int CreateDExTransaction(const std::string& buyerAddress, const std::string& sellerAddress, const CAmount& nAmount, const uint8_t& royaltiesPercentage, const std::string& royaltiesReceiver, uint256& txid);
#endif

#endif // TOKENCORE_WALLETTXBUILDER_H
