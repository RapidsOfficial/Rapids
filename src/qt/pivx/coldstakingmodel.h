// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COLDSTAKINGMODEL_H
#define COLDSTAKINGMODEL_H

#include <QAbstractTableModel>
#include "amount.h"
#include "transactiontablemodel.h"
#include "addresstablemodel.h"
#include "transactionrecord.h"
#include "walletmodel.h"

class CSDelegation {
public:
    CSDelegation(bool _isWhitelisted, std::string _delegatedAddress) : isWhitelisted(_isWhitelisted), delegatedAddress(_delegatedAddress), cachedTotalAmount(0) {}

    bool isWhitelisted;
    std::string delegatedAddress;
    /// Map of txId --> index num for stakeable utxo delegations
    QMap<QString, int> delegatedUtxo;
    // Sum of all delegations to this staking address
    CAmount cachedTotalAmount;

    // coin owner side, set to true if it can be spend
    bool isSpendable;

    bool operator==(const CSDelegation& obj) {
        return obj.delegatedAddress == delegatedAddress;
    }
};

class ColdStakingModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit ColdStakingModel(WalletModel* model, TransactionTableModel* _tableModel, AddressTableModel* _addressTableModel, QObject *parent = nullptr);
    ~ColdStakingModel() override{}

    enum ColumnIndex {
        DELEGATED_ADDRESS = 0,
        DELEGATED_ADDRESS_LABEL = 1,
        IS_WHITELISTED = 2,
        IS_WHITELISTED_STRING = 3,
        DELEGATED_UTXO_IDS = 4,
        TOTAL_STACKEABLE_AMOUNT_STR = 5,
        TOTAL_STACKEABLE_AMOUNT = 6,
        IS_RECEIVED_DELEGATION = 7,
        COLUMN_COUNT = 8
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool whitelist(const QModelIndex& modelIndex);
    bool blacklist(const QModelIndex& index);
    void updateCSList();

    void refresh();


private:
    WalletModel* model = nullptr;
    TransactionTableModel* tableModel = nullptr;
    AddressTableModel* addressTableModel = nullptr;

    /**
     * List with all of the grouped delegations received by this wallet
     */
    QList<CSDelegation> cachedDelegations;

    void checkForDelegations(const TransactionRecord& record, const CWallet* wallet, QList<CSDelegation>& cachedDelegations);
};

#endif // COLDSTAKINGMODEL_H
