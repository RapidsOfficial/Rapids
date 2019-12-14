// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/guitransactionsutils.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"

namespace GuiTransactionsUtils {
    void ProcessSendCoinsReturn(PWidget *parent, const WalletModel::SendCoinsReturn &sendCoinsReturn,
                                WalletModel *walletModel, const QString &msgArg, bool fPrepare) {
        bool fAskForUnlock = false;

        QPair<QString, CClientUIInterface::MessageBoxFlags> msgParams;
        // Default to a warning message, override if error message is needed
        msgParams.second = CClientUIInterface::MSG_WARNING;

        // This comment is specific to SendCoinsDialog usage of WalletModel::SendCoinsReturn.
        // WalletModel::TransactionCommitFailed is used only in WalletModel::sendCoins()
        // all others are used only in WalletModel::prepareTransaction()
        switch (sendCoinsReturn.status) {
            case WalletModel::InvalidAddress:
                msgParams.first = parent->translate("The recipient address is not valid, please recheck.");
                break;
            case WalletModel::InvalidAmount:
                msgParams.first = parent->translate("The amount to pay must be larger than 0.");
                break;
            case WalletModel::AmountExceedsBalance:
                msgParams.first = parent->translate("The amount exceeds your balance.");
                break;
            case WalletModel::AmountWithFeeExceedsBalance:
                msgParams.first = parent->translate(
                        "The total exceeds your balance when the %1 transaction fee is included.").arg(msgArg);
                break;
            case WalletModel::DuplicateAddress:
                msgParams.first = parent->translate(
                        "Duplicate address found, can only send to each address once per send operation.");
                break;
            case WalletModel::TransactionCreationFailed:
                msgParams.first = parent->translate("Transaction creation failed!");
                msgParams.second = CClientUIInterface::MSG_ERROR;
                break;
            case WalletModel::TransactionCommitFailed:
                msgParams.first = parent->translate(
                        "The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
                msgParams.second = CClientUIInterface::MSG_ERROR;
                break;
            case WalletModel::AnonymizeOnlyUnlocked:
                // Unlock is only need when the coins are send
                if (!fPrepare)
                    fAskForUnlock = true;
                else
                    msgParams.first = parent->translate("Error: The wallet is unlocked for staking only. Fully unlock the wallet to send the transaction.");
                break;

            case WalletModel::InsaneFee:
                msgParams.first = parent->translate(
                        "A fee %1 times higher than %2 per kB is considered an insanely high fee.").arg(10000).arg(
                        BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                     ::minRelayTxFee.GetFeePerK()));
                break;
                // included to prevent a compiler warning.
            case WalletModel::OK:
            default:
                return;
        }

        // Unlock wallet if it wasn't fully unlocked already
        if (fAskForUnlock) {
            walletModel->requestUnlock(AskPassphraseDialog::Context::Unlock_Full, false);
            if (walletModel->getEncryptionStatus() != WalletModel::Unlocked) {
                msgParams.first = parent->translate(
                        "Error: The wallet was unlocked only to anonymize coins. Unlock canceled.");
            } else {
                // Wallet unlocked
                return;
            }
        }

        parent->emitMessage(parent->translate("Send Coins"), msgParams.first, msgParams.second, 0);
    }

}
