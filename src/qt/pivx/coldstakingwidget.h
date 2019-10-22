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
#include "qt/pivx/contactsdropdown.h"
#include "qt/pivx/addressholder.h"
#include "transactiontablemodel.h"
#include "addresstablemodel.h"
#include "addressfilterproxymodel.h"
#include "coincontroldialog.h"

#include <QAction>
#include <QLabel>
#include <QWidget>
#include <QSpacerItem>

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
    void onCopyOwnerClicked();
    void onTxArrived(const QString& hash);
    void onContactsClicked(bool ownerAdd);
    void clearAll();
    void onLabelClicked();
    void onMyStakingAddressesClicked();

private:
    Ui::ColdStakingWidget *ui = nullptr;
    FurAbstractListItemDelegate *delegate = nullptr;
    FurAbstractListItemDelegate *addressDelegate = nullptr;
    TransactionTableModel* txModel = nullptr;
    AddressHolder* addressHolder = nullptr;
    AddressTableModel* addressTableModel = nullptr;
    AddressFilterProxyModel *addressesFilter = nullptr;
    ColdStakingModel* csModel = nullptr;
    CSDelegationHolder *txHolder = nullptr;
    CoinControlDialog *coinControlDialog = nullptr;
    QAction *btnOwnerContact = nullptr;
    QSpacerItem *spacerDiv = nullptr;

    ContactsDropdown *menuContacts = nullptr;
    TooltipMenu* menu = nullptr;
    SendMultiRow *sendMultiRow = nullptr;
    bool isShowingDialog;

    bool isContactOwnerSelected;

    // Cached index
    QModelIndex index;

    int nDisplayUnit;

    void showAddressGenerationDialog(bool isPaymentRequest);
    void onContactsClicked();
};

#endif // COLDSTAKINGWIDGET_H
