// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/coldstakingmodel.h"
#include "uint256.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include <iostream>
#include "addressbook.h"

ColdStakingModel::ColdStakingModel(WalletModel* _model,
                                   TransactionTableModel* _tableModel,
                                   AddressTableModel* _addressTableModel,
                                   QObject *parent) : QAbstractTableModel(parent), model(_model), tableModel(_tableModel), addressTableModel(_addressTableModel){
    updateCSList();
}

void ColdStakingModel::updateCSList() {
    refresh();
    emit dataChanged(index(0, 0, QModelIndex()), index(rowCount(), COLUMN_COUNT, QModelIndex()) );
}

void ColdStakingModel::refresh() {
    cachedDelegations.clear();
    // First get all of the p2cs utxo inside the wallet
    std::vector<COutput> utxoList;
    pwalletMain->GetAvailableP2CSCoins(utxoList);

    if (!utxoList.empty()) {
        // Parse the COutput into CSDelegations
        for (const auto& utxo : utxoList) {

            const auto *wtx = utxo.tx;
            uint256 hash = wtx->GetHash();
            int64_t nTime = wtx->GetComputedTxTime();

            TransactionRecord record(hash, nTime, wtx->GetTotalSize());

            // First parse the record
            if (wtx->IsCoinStake()) {
                if (isminetype mine = pwalletMain->IsMine(wtx->vout[1])) {
                    if (wtx->HasP2CSOutputs()) {
                        TransactionRecord::loadHotOrColdStakeOrContract(pwalletMain, *wtx, record);
                    }
                }
            } else if (wtx->HasP2CSOutputs()) {
                // Delegation record.
                TransactionRecord::loadHotOrColdStakeOrContract(pwalletMain, *wtx, record, true);
            } else if (wtx->HasP2CSInputs()){
                TransactionRecord::loadUnlockColdStake(pwalletMain, *wtx, record);
            }

            // Once the record is parsed, load the cached map
            checkForDelegations(record, pwalletMain, cachedDelegations);
        }
    }
}

int ColdStakingModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return cachedDelegations.size();
}

int ColdStakingModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return COLUMN_COUNT;
}


QVariant ColdStakingModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
            return QVariant();

    int row = index.row();
    CSDelegation rec = cachedDelegations[row];
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case DELEGATED_ADDRESS:
                return QString::fromStdString(rec.delegatedAddress);
            case DELEGATED_ADDRESS_LABEL:
                return addressTableModel->labelForAddress(QString::fromStdString(rec.delegatedAddress));
            case IS_WHITELISTED:
                return addressTableModel->purposeForAddress(rec.delegatedAddress).compare(AddressBook::AddressBookPurpose::DELEGATOR) == 0;
            case IS_WHITELISTED_STRING:
                return (addressTableModel->purposeForAddress(rec.delegatedAddress) == AddressBook::AddressBookPurpose::DELEGATOR ? "Staking" : "Not staking");
            case TOTAL_STACKEABLE_AMOUNT_STR:
                return GUIUtil::formatBalance(rec.cachedTotalAmount);
            case TOTAL_STACKEABLE_AMOUNT:
                return qint64(rec.cachedTotalAmount);
            case IS_RECEIVED_DELEGATION:
                return !rec.isSpendable;
        }
    }

    return QVariant();
}

bool ColdStakingModel::whitelist(const QModelIndex& modelIndex) {
    QString address = modelIndex.data(Qt::DisplayRole).toString();
    int idx = modelIndex.row();
    beginRemoveRows(QModelIndex(), idx, idx);
    bool ret = model->whitelistAddressFromColdStaking(address);
    endRemoveRows();
    emit dataChanged(index(idx, 0, QModelIndex()), index(idx, COLUMN_COUNT, QModelIndex()) );
    return ret;
}

bool ColdStakingModel::blacklist(const QModelIndex& modelIndex) {
    QString address = modelIndex.data(Qt::DisplayRole).toString();
    int idx = modelIndex.row();
    beginRemoveRows(QModelIndex(), idx, idx);
    bool ret = model->blacklistAddressFromColdStaking(address);
    endRemoveRows();
    emit dataChanged(index(idx, 0, QModelIndex()), index(idx, COLUMN_COUNT, QModelIndex()) );
    return ret;
}


void ColdStakingModel::checkForDelegations(const TransactionRecord& record, const CWallet* wallet, QList<CSDelegation>& cachedDelegations) {
    CSDelegation delegation(false, record.address);
    delegation.isSpendable = record.type == TransactionRecord::P2CSDelegationSent || record.type == TransactionRecord::StakeDelegated;

    // Append only stakeable utxo and not every output of the record
    const QString& hashTxId = record.getTxID();
    const CWalletTx *tx = wallet->GetWalletTx(record.hash);

    if (!tx)
        return;

    for (int i = 0; i < (int) tx->vout.size(); ++i) {
        auto out =  tx->vout[i];
        if (out.scriptPubKey.IsPayToColdStaking()) {
            {
                LOCK(cs_main);
                CCoinsViewCache &view = *pcoinsTip;
                if (view.IsOutputAvailable(record.hash, i)) {
                    delegation.cachedTotalAmount += out.nValue;
                    delegation.delegatedUtxo.insert(hashTxId, i);
                }
            }
        }
    }

    // If there are no available p2cs delegations then don't try to add them
    if (delegation.delegatedUtxo.isEmpty())
        return;

    int index = cachedDelegations.indexOf(delegation);
    if (index == -1) {
        cachedDelegations.append(delegation);
    } else {
        CSDelegation& del = cachedDelegations[index];
        del.delegatedUtxo.unite(delegation.delegatedUtxo);
        del.cachedTotalAmount += delegation.cachedTotalAmount;
    }
}