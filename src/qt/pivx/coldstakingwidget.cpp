// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/coldstakingwidget.h"
#include "qt/pivx/forms/ui_coldstakingwidget.h"
#include "qt/pivx/qtutils.h"
#include "guiutil.h"
#include "qt/pivx/denomgenerationdialog.h"
#include "qt/pivx/txviewholder.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "coincontroldialog.h"
#include "coincontrol.h"
#include "zpiv/accumulators.h"

#define DECORATION_SIZE 65
#define NUM_ITEMS 3

ColdStakingWidget::ColdStakingWidget(PIVXGUI* parent) :
    PWidget(parent),
    ui(new Ui::ColdStakingWidget)
{
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    setCssProperty(ui->left, "container");
    ui->left->setContentsMargins(0,20,0,0);
    setCssProperty(ui->right, "container-right");
    ui->right->setContentsMargins(20,10,20,20);

    /* Light Font */
    QFont fontLight;
    fontLight.setWeight(QFont::Light);

    /* Title */
    ui->labelTitle->setText(tr("Cold Staking"));
    setCssTitleScreen(ui->labelTitle);
    ui->labelTitle->setFont(fontLight);

    /* Button Group */
    ui->pushLeft->setText(tr("Staker"));
    ui->pushRight->setText(tr("Delegation"));
    setCssProperty(ui->pushLeft, "btn-check-left");
    setCssProperty(ui->pushRight, "btn-check-right");

    /* Subtitle */
    ui->labelSubtitle1->setText(tr("You can delegate your PIVs and let a hot node (24/7 online node)\nstake in your behalf, keeping the keys in a secure place offline."));
    setCssSubtitleScreen(ui->labelSubtitle1);

    ui->labelSubtitle2->setText(tr("Delegate or Accept PIV delegation"));
    setCssSubtitleScreen(ui->labelSubtitle2);
    ui->labelSubtitle2->setContentsMargins(0,2,0,0);

    ui->labelEditTitle->setText(tr("Add the delegator address"));
    setCssProperty(ui->labelEditTitle, "text-title");
    sendMultiRow = new SendMultiRow(this);
    ui->containerSend->layout()->addWidget(sendMultiRow);
    //connect(sendMultiRow, &SendMultiRow::onContactsClicked, this, &SendWidget::onContactsClicked);
    //connect(sendMultiRow, &SendMultiRow::onMenuClicked, this, &SendWidget::onMenuClicked);
    //connect(sendMultiRow, &SendMultiRow::onValueChanged, this, &SendWidget::onValueChanged);

    // List
    ui->labelListHistory->setText(tr("Delegated balance history"));
    setCssProperty(ui->labelListHistory, "text-title");

    //ui->emptyContainer->setVisible(false);
    setCssProperty(ui->pushImgEmpty, "img-empty-transactions");
    ui->labelEmpty->setText(tr("No delegations yet"));
    setCssProperty(ui->labelEmpty, "text-empty");

    ui->btnCoinControl->setTitleClassAndText("btn-title-grey", "Coin Control");
    ui->btnCoinControl->setSubTitleClassAndText("text-subtitle", "Select PIV outputs to delegate.");

    connect(ui->btnCoinControl, SIGNAL(clicked()), this, SLOT(onCoinControlClicked()));

    onDelegateSelected(true);
    ui->pushRight->setChecked(true);
    connect(ui->pushLeft, &QPushButton::clicked, [this](){onDelegateSelected(false);});
    connect(ui->pushRight,  &QPushButton::clicked, [this](){onDelegateSelected(true);});

    // List
    setCssProperty(ui->listView, "container");
    txHolder = new TxViewHolder(isLightTheme());
    delegate = new FurAbstractListItemDelegate(
                DECORATION_SIZE,
                txHolder,
                this
    );

    ui->listView->setItemDelegate(delegate);
    ui->listView->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listView->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listView->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listView->setSelectionBehavior(QAbstractItemView::SelectRows);
}

void ColdStakingWidget::loadWalletModel(){
    if(walletModel) {
        sendMultiRow->setWalletModel(this->walletModel);

        txModel = walletModel->getTransactionTableModel();
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setDynamicSortFilter(true);
        filter->setSortCaseSensitivity(Qt::CaseInsensitive);
        filter->setFilterCaseSensitivity(Qt::CaseInsensitive);
        filter->setSortRole(Qt::EditRole);
        filter->setOnlyColdStakes(true);
        filter->setSourceModel(txModel);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);
        txHolder->setDisplayUnit(walletModel->getOptionsModel()->getDisplayUnit());
        txHolder->setFilter(filter);
        ui->listView->setModel(filter);

        updateDisplayUnit();
        // Show list
        showList(filter->rowCount() > 0);
    }

}

void ColdStakingWidget::onDelegateSelected(bool delegate){
    if(delegate){
        ui->btnCoinControl->setVisible(true);
    }else{
        ui->btnCoinControl->setVisible(false);
        // change list row for the whitelisted addresses
    }
}

void ColdStakingWidget::updateDisplayUnit() {
    if (walletModel && walletModel->getOptionsModel()) {
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();

        txHolder->setDisplayUnit(nDisplayUnit);
        ui->listView->update();
    }
}

void ColdStakingWidget::showList(bool show){
    ui->emptyContainer->setVisible(!show);
    ui->listView->setVisible(show);
}

void ColdStakingWidget::onSendClicked(){
    if (!walletModel || !walletModel->getOptionsModel())
        return;

}

void ColdStakingWidget::delegateBalance(CAmount value, QString address, QString label){
    inform(tr("Complete me.. delegation."));
}


void ColdStakingWidget::onCoinControlClicked(){
    if(ui->pushRight->isChecked()) {
        if (walletModel->getBalance() > 0) {
            if (!coinControlDialog) {
                coinControlDialog = new CoinControlDialog();
                coinControlDialog->setModel(walletModel);
            }
            coinControlDialog->exec();
            ui->btnCoinControl->setActive(CoinControlDialog::coinControl->HasSelected());
        } else {
            inform(tr("You don't have any PIV to select."));
        }
    }
}


void ColdStakingWidget::changeTheme(bool isLightTheme, QString& theme){
    static_cast<TxViewHolder*>(this->delegate->getRowFactory())->isLightTheme = isLightTheme;
    ui->listView->update();
}

ColdStakingWidget::~ColdStakingWidget(){
    delete ui;
}
