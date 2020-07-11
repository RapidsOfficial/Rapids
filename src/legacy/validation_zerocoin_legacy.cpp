// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.
#include "legacy/validation_zerocoin_legacy.h"

#include "consensus/zerocoin_verify.h"
#include "libzerocoin/CoinSpend.h"
#include "wallet/wallet.h"
#include "zpivchain.h"

bool AcceptToMemoryPoolZerocoin(const CTransaction& tx, CAmount& nValueIn, int chainHeight, CValidationState& state, const Consensus::Params& consensus)
{
    nValueIn = tx.GetZerocoinSpent();

    //Check that txid is not already in the chain
    int nHeightTx = 0;
    if (IsTransactionInChain(tx.GetHash(), nHeightTx))
        return state.Invalid(error("%s : zPIV spend tx %s already in block %d", __func__, tx.GetHash().GetHex(), nHeightTx),
                             REJECT_DUPLICATE, "bad-txns-inputs-spent");

    //Check for double spending of serial #'s
    for (const CTxIn& txIn : tx.vin) {
        // Only allow for public zc spends inputs
        if (!txIn.IsZerocoinPublicSpend())
            return state.Invalid(false, REJECT_INVALID, "bad-zc-spend-notpublic");

        libzerocoin::ZerocoinParams* params = consensus.Zerocoin_Params(false);
        PublicCoinSpend publicSpend(params);
        if (!ZPIVModule::ParseZerocoinPublicSpend(txIn, tx, state, publicSpend)){
            return false;
        }
        if (!ContextualCheckZerocoinSpend(tx, &publicSpend, chainHeight, UINT256_ZERO))
            return state.Invalid(false, REJECT_INVALID, "bad-zc-spend-contextcheck");

        // Check that the version matches the one enforced with SPORK_18
        if (!CheckPublicCoinSpendVersion(publicSpend.getVersion())) {
            return state.Invalid(false, REJECT_INVALID, "bad-zc-spend-version");
        }
    }

    return true;
}

bool DisconnectZerocoinTx(const CTransaction& tx, CAmount& nValueIn, CZerocoinDB* zerocoinDB)
{
    /** UNDO ZEROCOIN DATABASING
         * note we only undo zerocoin databasing in the following statement, value to and from PIVX
         * addresses should still be handled by the typical bitcoin based undo code
         * */
    if (tx.ContainsZerocoins()) {
        libzerocoin::ZerocoinParams *params = Params().GetConsensus().Zerocoin_Params(false);
        if (tx.HasZerocoinSpendInputs()) {
            //erase all zerocoinspends in this transaction
            for (const CTxIn &txin : tx.vin) {
                bool isPublicSpend = txin.IsZerocoinPublicSpend();
                if (txin.scriptSig.IsZerocoinSpend() || isPublicSpend) {
                    CBigNum serial;
                    if (isPublicSpend) {
                        PublicCoinSpend publicSpend(params);
                        CValidationState state;
                        if (!ZPIVModule::ParseZerocoinPublicSpend(txin, tx, state, publicSpend)) {
                            return error("Failed to parse public spend");
                        }
                        serial = publicSpend.getCoinSerialNumber();
                        nValueIn += publicSpend.getDenomination() * COIN;
                    } else {
                        libzerocoin::CoinSpend spend = TxInToZerocoinSpend(txin);
                        serial = spend.getCoinSerialNumber();
                        nValueIn += spend.getDenomination() * COIN;
                    }

                    if (!zerocoinDB->EraseCoinSpend(serial))
                        return error("failed to erase spent zerocoin in block");

                    //if this was our spend, then mark it unspent now
                    if (pwalletMain) {
                        if (pwalletMain->IsMyZerocoinSpend(serial)) {
                            if (!pwalletMain->SetMintUnspent(serial))
                                LogPrintf("%s: failed to automatically reset mint", __func__);
                        }
                    }
                }

            }
        }

        if (tx.HasZerocoinMintOutputs()) {
            //erase all zerocoinmints in this transaction
            for (const CTxOut &txout : tx.vout) {
                if (txout.scriptPubKey.empty() || !txout.IsZerocoinMint())
                    continue;

                libzerocoin::PublicCoin pubCoin(params);
                CValidationState state;
                if (!TxOutToPublicCoin(txout, pubCoin, state))
                    return error("DisconnectBlock(): TxOutToPublicCoin() failed");

                if (!zerocoinDB->EraseCoinMint(pubCoin.getValue()))
                    return error("DisconnectBlock(): Failed to erase coin mint");
            }
        }
    }
    return true;
}

// Legacy Zerocoin DB: used for performance during IBD
// (between Zerocoin_Block_V2_Start and Zerocoin_Block_Last_Checkpoint)
void DataBaseAccChecksum(CBlockIndex* pindex, bool fWrite)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    if (!pindex ||
        !consensus.NetworkUpgradeActive(pindex->nHeight, Consensus::UPGRADE_ZC_V2) ||
        pindex->nHeight > consensus.height_last_ZC_AccumCheckpoint ||
        pindex->nAccumulatorCheckpoint == pindex->pprev->nAccumulatorCheckpoint)
        return;

    uint256 accCurr = pindex->nAccumulatorCheckpoint;
    uint256 accPrev = pindex->pprev->nAccumulatorCheckpoint;
    // add/remove changed checksums to/from DB
    for (int i = (int)libzerocoin::zerocoinDenomList.size()-1; i >= 0; i--) {
        const uint32_t& nChecksum = accCurr.Get32();
        if (nChecksum != accPrev.Get32()) {
            fWrite ?
            zerocoinDB->WriteAccChecksum(nChecksum, libzerocoin::zerocoinDenomList[i], pindex->nHeight) :
            zerocoinDB->EraseAccChecksum(nChecksum, libzerocoin::zerocoinDenomList[i]);
        }
        accCurr >>= 32;
        accPrev >>= 32;
    }
}
