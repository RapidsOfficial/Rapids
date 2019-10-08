// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COLDSTAKINGMODEL_H
#define COLDSTAKINGMODEL_H

#include <QAbstractTableModel>
#include "transactiontablemodel.h"
#include "addresstablemodel.h"
#include "walletmodel.h"

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
        TOTAL_STACKEABLE_AMOUNT = 5
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool whitelist(const QModelIndex& modelIndex);
    bool blacklist(const QModelIndex& index);
    void updateCSList();


private:
    WalletModel* model;
    TransactionTableModel* tableModel;
    AddressTableModel* addressTableModel;

};

#endif // COLDSTAKINGMODEL_H
