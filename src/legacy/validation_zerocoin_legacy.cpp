// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.
#include "legacy/validation_zerocoin_legacy.h"

#include "libzerocoin/CoinSpend.h"
#include "wallet/wallet.h"
#include "zpivchain.h"

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