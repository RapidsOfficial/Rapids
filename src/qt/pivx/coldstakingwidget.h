// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COLDSTAKINGWIDGET_H
#define COLDSTAKINGWIDGET_H

#include "qt/pivx/pwidget.h"
#include "qt/pivx/furabstractlistitemdelegate.h"
#include "qt/pivx/txviewholder.h"
#include "qt/pivx/tooltipmenu.h"
#include "qt/pivx/sendmultirow.h"
#include "qt/pivx/coldstakingmodel.h"
#include "transactiontablemodel.h"
#include "coincontroldialog.h"

#include <QLabel>
#include <QWidget>

class PIVXGUI;
class WalletModel;
class CSDelegationHolder;

namespace Ui {
class ColdStakingWidget;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class ColdStakingWidget : public PWidget
{
    Q_OBJECT

public:
    explicit ColdStakingWidget(PIVXGUI* parent);
    ~ColdStakingWidget();

    void loadWalletModel() override;
private slots:
    void changeTheme(bool isLightTheme, QString &theme) override;
    void handleAddressClicked(const QModelIndex &index);
    void onCoinControlClicked();
    void onColdStakeClicked();
    void updateDisplayUnit();
    void showList(bool show);
    void onSendClicked();
    void onDelegateSelected(bool delegate);
    void onEditClicked();
    void onDeleteClicked();
    void onCopyClicked();
    void onTxArrived(const QString& hash);

private:
    Ui::ColdStakingWidget *ui;
    FurAbstractListItemDelegate *delegate = nullptr;
    TransactionTableModel* txModel = nullptr;
    ColdStakingModel* csModel = nullptr;
    CSDelegationHolder *txHolder = nullptr;
    CoinControlDialog *coinControlDialog = nullptr;

    TooltipMenu* menu = nullptr;
    SendMultiRow *sendMultiRow = nullptr;
    bool isShowingDialog;

    // Cached index
    QModelIndex index;

    int nDisplayUnit;
    void clearAll();

    void showAddressGenerationDialog(bool isPaymentRequest);
};

#endif // COLDSTAKINGWIDGET_H
