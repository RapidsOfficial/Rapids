// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/coldstakingwidget.h"
#include "qt/pivx/forms/ui_coldstakingwidget.h"
#include "qt/pivx/qtutils.h"
#include "amount.h"
#include "guiutil.h"
#include "qt/pivx/requestdialog.h"
#include "qt/pivx/tooltipmenu.h"
#include "qt/pivx/furlistrow.h"
#include "qt/pivx/txviewholder.h"
#include "qt/pivx/sendconfirmdialog.h"
#include "qt/pivx/guitransactionsutils.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "coincontroldialog.h"
#include "coincontrol.h"
#include "qt/pivx/csrow.h"

#define DECORATION_SIZE 70
#define NUM_ITEMS 3


class CSDelegationHolder : public FurListRow<QWidget*>
{
public:
    CSDelegationHolder();

    explicit CSDelegationHolder(bool _isLightTheme) : FurListRow(), isLightTheme(_isLightTheme){}

    CSRow* createHolder(int pos) override{
        if (!cachedRow) cachedRow = new CSRow();
        return cachedRow;
    }

    void init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const override{
        CSRow *row = static_cast<CSRow*>(holder);
        row->updateState(isLightTheme, isHovered, isSelected);

        QString address = index.data(Qt::DisplayRole).toString();
        QString label = index.sibling(index.row(), ColdStakingModel::DELEGATED_ADDRESS_LABEL).data(Qt::DisplayRole).toString();
        if (label.isEmpty()) {
            label = "Address with no label";
        }
        bool isWhitelisted = index.sibling(index.row(), ColdStakingModel::IS_WHITELISTED).data(Qt::DisplayRole).toBool();
        QString amountStr = index.sibling(index.row(), ColdStakingModel::TOTAL_STACKEABLE_AMOUNT_STR).data(Qt::DisplayRole).toString();
        row->updateView(address, label, isWhitelisted, amountStr);
    }

    QColor rectColor(bool isHovered, bool isSelected) override{
        return getRowColor(isLightTheme, isHovered, isSelected);
    }

    ~CSDelegationHolder() override{}

    bool isLightTheme;
private:
    CSRow *cachedRow = nullptr;
};

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

    setCssProperty(ui->labelSubtitleDescription, "text-title");
    ui->lineEditOwnerAddress->setPlaceholderText(tr("Add owner address"));
    btnOwnerContact = ui->lineEditOwnerAddress->addAction(QIcon("://ic-contact-arrow-down"), QLineEdit::TrailingPosition);
    setCssProperty(ui->lineEditOwnerAddress, "edit-primary-multi-book");
    ui->lineEditOwnerAddress->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->lineEditOwnerAddress);

    ui->labelSubtitle2->setText(tr("Delegate or Accept PIV delegation"));
    setCssSubtitleScreen(ui->labelSubtitle2);
    ui->labelSubtitle2->setContentsMargins(0,2,0,0);
    setCssBtnPrimary(ui->pushButtonSend);

    ui->labelEditTitle->setText(tr("Add the staking address"));
    setCssProperty(ui->labelEditTitle, "text-title");
    sendMultiRow = new SendMultiRow(this);
    sendMultiRow->setOnlyStakingAddressAccepted(true);
    ((QVBoxLayout*)ui->containerSend->layout())->insertWidget(1, sendMultiRow);
    connect(sendMultiRow, &SendMultiRow::onContactsClicked, [this](){ onContactsClicked(false); });

    // List
    ui->labelListHistory->setText(tr("Delegated balance history"));
    setCssProperty(ui->labelListHistory, "text-title");
    setCssProperty(ui->pushImgEmpty, "img-empty-transactions");
    ui->labelEmpty->setText(tr("No delegations yet"));
    setCssProperty(ui->labelEmpty, "text-empty");

    ui->btnCoinControl->setTitleClassAndText("btn-title-grey", "Coin Control");
    ui->btnCoinControl->setSubTitleClassAndText("text-subtitle", "Select PIV outputs to delegate.");

    ui->btnColdStaking->setTitleClassAndText("btn-title-grey", "Create Cold Stake Address");
    ui->btnColdStaking->setSubTitleClassAndText("text-subtitle", "Creates an address to receive coin\ndelegations and be able to stake them.");
    ui->btnColdStaking->layout()->setMargin(0);

    connect(ui->btnCoinControl, SIGNAL(clicked()), this, SLOT(onCoinControlClicked()));
    connect(ui->btnColdStaking, SIGNAL(clicked()), this, SLOT(onColdStakeClicked()));

    onDelegateSelected(true);
    ui->pushRight->setChecked(true);
    connect(ui->pushLeft, &QPushButton::clicked, [this](){onDelegateSelected(false);});
    connect(ui->pushRight,  &QPushButton::clicked, [this](){onDelegateSelected(true);});

    // List
    setCssProperty(ui->listView, "container");
    txHolder = new CSDelegationHolder(isLightTheme());
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

    connect(ui->pushButtonSend, &QPushButton::clicked, this, &ColdStakingWidget::onSendClicked);
    connect(btnOwnerContact, &QAction::triggered, [this](){ onContactsClicked(true); });
    connect(ui->listView, SIGNAL(clicked(QModelIndex)), this, SLOT(handleAddressClicked(QModelIndex)));
}

void ColdStakingWidget::loadWalletModel(){
    if(walletModel) {
        sendMultiRow->setWalletModel(this->walletModel);
        txModel = walletModel->getTransactionTableModel();
        csModel = new ColdStakingModel(walletModel, txModel, walletModel->getAddressTableModel(), this);
        ui->listView->setModel(csModel);

        connect(txModel, &TransactionTableModel::txArrived, this, &ColdStakingWidget::onTxArrived);

        updateDisplayUnit();

        ui->containerHistoryLabel->setVisible(false);
        ui->emptyContainer->setVisible(false);
        ui->listView->setVisible(false);
    }

}

void ColdStakingWidget::onTxArrived(const QString& hash) {
    if (walletModel->isDelegatedToMe(hash)) {
        csModel->updateCSList();
        if (ui->pushLeft->isChecked())
            showList(true);
    }
}

void ColdStakingWidget::onContactsClicked(bool ownerAdd) {
    isContactOwnerSelected = ownerAdd;
    onContactsClicked();
}

void ColdStakingWidget::onContactsClicked(){

    if(menu && menu->isVisible()){
        menu->hide();
    }

    int contactsSize = isContactOwnerSelected ? walletModel->getAddressTableModel()->sizeSend() : walletModel->getAddressTableModel()->sizeDell();
    if(contactsSize == 0) {
        inform(tr("No contacts available, you can go to the contacts screen and add some there!"));
        return;
    }

    int height;
    int width;
    QPoint pos;

    if (isContactOwnerSelected) {
        height = ui->lineEditOwnerAddress->height();
        width = ui->lineEditOwnerAddress->width();
        pos = ui->containerSend->rect().bottomLeft();
        pos.setY((pos.y() + (height - 12) * 3));
    } else {
        height = sendMultiRow->getEditHeight();
        width = sendMultiRow->getEditWidth();
        pos = sendMultiRow->getEditLineRect().bottomLeft();
        pos.setY((pos.y() + (height - 14) * 4));
    }

    pos.setX(pos.x() + 40);
    height = (contactsSize <= 2) ? height * ( 2 * (contactsSize + 1 )) : height * 4;

    if(!menuContacts){
        menuContacts = new ContactsDropdown(
                width,
                height,
                this
        );
        connect(menuContacts, &ContactsDropdown::contactSelected, [this](QString address, QString label){
            if (isContactOwnerSelected) {
                ui->lineEditOwnerAddress->setText(address);
            } else {
                sendMultiRow->setLabel(label);
                sendMultiRow->setAddress(address);
            }
        });
    }

    if(menuContacts->isVisible()){
        menuContacts->hide();
        return;
    }

    if (isContactOwnerSelected) {
        menuContacts->setWalletModel(walletModel, AddressTableModel::Send);
    } else {
        menuContacts->setWalletModel(walletModel, AddressTableModel::Delegators);
    }

    menuContacts->resizeList(width, height);
    menuContacts->setStyleSheet(this->styleSheet());
    menuContacts->adjustSize();
    menuContacts->move(pos);
    menuContacts->show();
}

void ColdStakingWidget::onDelegateSelected(bool delegate){
    if(delegate){
        ui->btnCoinControl->setVisible(true);
        ui->containerSend->setVisible(true);
        ui->containerBtn->setVisible(true);
        ui->emptyContainer->setVisible(false);
        ui->listView->setVisible(false);
        ui->containerHistoryLabel->setVisible(false);
        ui->btnColdStaking->setVisible(false);
    }else{
        ui->btnCoinControl->setVisible(false);
        ui->containerSend->setVisible(false);
        ui->containerBtn->setVisible(false);
        ui->btnColdStaking->setVisible(true);
        showList(true);
    }
}

void ColdStakingWidget::updateDisplayUnit() {
    if (walletModel && walletModel->getOptionsModel()) {
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        ui->listView->update();
    }
}

void ColdStakingWidget::showList(bool show){
    ui->emptyContainer->setVisible(!show);
    ui->listView->setVisible(show);
    ui->containerHistoryLabel->setVisible(show);
}

void ColdStakingWidget::onSendClicked(){
    if (!walletModel || !walletModel->getOptionsModel() || !verifyWalletUnlocked())
        return;

    if (!sendMultiRow->validate()) {
        inform(tr("Invalid entry"));
        return;
    }

    SendCoinsRecipient dest = sendMultiRow->getValue();
    dest.isP2CS = true;

    // Amount must be < 10 PIV, check chainparams minColdStakingAmount
    if (dest.amount < (COIN * 10)) {
        inform(tr("Invalid entry, minimum delegable amount is 10 PIV"));
        return;
    }

    QString inputOwner = ui->lineEditOwnerAddress->text();
    bool isOwnerEmpty = inputOwner.isEmpty();
    if (!isOwnerEmpty && !walletModel->validateAddress(inputOwner)) {
        inform(tr("Owner address invalid"));
        return;
    }


    bool isStakingAddressFromThisWallet = walletModel->isMine(dest.address);
    bool isOwnerAddressFromThisWallet = isOwnerEmpty;

    if (!isOwnerAddressFromThisWallet) {
        isOwnerAddressFromThisWallet = walletModel->isMine(inputOwner);
    }

    // Don't try to delegate the balance if both addresses are from this wallet
    if (isStakingAddressFromThisWallet && isOwnerAddressFromThisWallet) {
        inform(tr("Staking address corresponds to this wallet, change it to an external node"));
        return;
    }

    dest.ownerAddress = inputOwner;
    QList<SendCoinsRecipient> recipients;
    recipients.append(dest);

    // Prepare transaction for getting txFee earlier
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus = walletModel->prepareTransaction(currentTransaction, CoinControlDialog::coinControl);

    // process prepareStatus and on error generate message shown to user
    GuiTransactionsUtils::ProcessSendCoinsReturn(
            this,
            prepareStatus,
            walletModel,
            BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                         currentTransaction.getTransactionFee()),
            true
    );

    if (prepareStatus.status != WalletModel::OK) {
        inform(tr("Cannot create transaction."));
        return;
    }

    showHideOp(true);
    TxDetailDialog* dialog = new TxDetailDialog(window);
    dialog->setDisplayUnit(walletModel->getOptionsModel()->getDisplayUnit());
    dialog->setData(walletModel, currentTransaction);
    dialog->adjustSize();
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);

    if(dialog->isConfirm()){
        // now send the prepared transaction
        WalletModel::SendCoinsReturn sendStatus = dialog->getStatus();
        // process sendStatus and on error generate message shown to user
        GuiTransactionsUtils::ProcessSendCoinsReturn(
                this,
                sendStatus,
                walletModel
        );

        if (sendStatus.status == WalletModel::OK) {
            clearAll();
            inform(tr("Coins delegated"));
        }
    }

    dialog->deleteLater();
}

void ColdStakingWidget::clearAll() {
    if (sendMultiRow) sendMultiRow->clear();
    ui->lineEditOwnerAddress->clear();
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

void ColdStakingWidget::onColdStakeClicked() {
    showAddressGenerationDialog(false);
}

void ColdStakingWidget::showAddressGenerationDialog(bool isPaymentRequest) {
    if(walletModel && !isShowingDialog) {
        if (!verifyWalletUnlocked()) return;
        isShowingDialog = true;
        showHideOp(true);
        RequestDialog *dialog = new RequestDialog(window);
        dialog->setWalletModel(walletModel);
        dialog->setPaymentRequest(isPaymentRequest);
        openDialogWithOpaqueBackgroundY(dialog, window, 3.5, 12);
        if (dialog->res == 1){
            inform(tr("URI copied to clipboard"));
        } else if (dialog->res == 2){
            inform(tr("Address copied to clipboard"));
        }
        dialog->deleteLater();
        isShowingDialog = false;
    }
}

void ColdStakingWidget::handleAddressClicked(const QModelIndex &rIndex){
    ui->listView->setCurrentIndex(rIndex);
    QRect rect = ui->listView->visualRect(rIndex);
    QPoint pos = rect.topRight();
    pos.setX(pos.x() - (DECORATION_SIZE * 2));
    pos.setY(pos.y() + (DECORATION_SIZE * 2.45));

    bool adjustSize = false;
    if(!this->menu){
        this->menu = new TooltipMenu(window, this);
        this->menu->setEditBtnText(tr("Stake"));
        this->menu->setDeleteBtnText(tr("Blacklist"));
        this->menu->setCopyBtnText(tr("Info"));
        this->menu->setMinimumHeight(75);
        adjustSize = true;
        connect(this->menu, &TooltipMenu::message, this, &AddressesWidget::message);
        connect(this->menu, SIGNAL(onEditClicked()), this, SLOT(onEditClicked()));
        connect(this->menu, SIGNAL(onDeleteClicked()), this, SLOT(onDeleteClicked()));
        connect(this->menu, SIGNAL(onCopyClicked()), this, SLOT(onCopyClicked()));
    }else {
        this->menu->hide();
    }

    this->index = rIndex;

    bool isReceivedDelegation = rIndex.sibling(rIndex.row(), ColdStakingModel::IS_RECEIVED_DELEGATION).data(Qt::DisplayRole).toBool();

    if (isReceivedDelegation) {
        bool isWhitelisted = rIndex.sibling(rIndex.row(), ColdStakingModel::IS_WHITELISTED).data(
                Qt::DisplayRole).toBool();
        this->menu->setDeleteBtnVisible(isWhitelisted);
        this->menu->setEditBtnVisible(!isWhitelisted);
        this->menu->setMinimumHeight(75);
    } else {
        // owner side
        this->menu->setDeleteBtnVisible(false);
        this->menu->setEditBtnVisible(false);
        this->menu->setMinimumHeight(60);
    }
    if (adjustSize) this->menu->adjustSize();

    menu->move(pos);
    menu->show();
}

void ColdStakingWidget::onEditClicked() {
    // whitelist address
    if (!csModel->whitelist(index)) {
        inform(tr("Whitelist failed, please check the logs"));
        return;
    }
    QString label = index.sibling(index.row(), ColdStakingModel::DELEGATED_ADDRESS_LABEL).data(Qt::DisplayRole).toString();
    if (label.isEmpty()) {
        label = index.data(Qt::DisplayRole).toString();
    }
    inform(label + tr(" staking!"));
}

void ColdStakingWidget::onDeleteClicked() {
    // blacklist address
    if (!csModel->blacklist(index)) {
        inform(tr("Blacklist failed, please check the logs"));
        return;
    }
    QString label = index.sibling(index.row(), ColdStakingModel::DELEGATED_ADDRESS_LABEL).data(Qt::DisplayRole).toString();
    if (label.isEmpty()) {
        label = index.data(Qt::DisplayRole).toString();
    }
    inform(label + tr(" blacklisted from staking"));
}

void ColdStakingWidget::onCopyClicked() {
    // show address info
}

void ColdStakingWidget::changeTheme(bool isLightTheme, QString& theme){
    static_cast<TxViewHolder*>(this->delegate->getRowFactory())->isLightTheme = isLightTheme;
    ui->listView->update();
}

ColdStakingWidget::~ColdStakingWidget(){
    delete ui;
}
