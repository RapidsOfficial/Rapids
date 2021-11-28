// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sendmpdialog.h"
#include "ui_sendmpdialog.h"

#include "qt/rapids/qtutils.h"

#include "tokencore_qtutils.h"

#include "clientmodel.h"
#include "walletmodel.h"

#include "platformstyle.h"

#include "tokencore/createpayload.h"
#include "tokencore/errors.h"
#include "tokencore/tokencore.h"
#include "tokencore/parse_string.h"
#include "tokencore/pending.h"
#include "tokencore/sp.h"
#include "tokencore/tally.h"
#include "tokencore/utilsbitcoin.h"
#include "tokencore/wallettxbuilder.h"
#include "tokencore/walletutils.h"

#include "amount.h"
#include "base58.h"
#include "main.h"
#include "sync.h"
#include "uint256.h"
#include "wallet/wallet.h"

#include <stdint.h>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <QDateTime>
#include <QDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QString>
#include <QWidget>

#include "platformstyle.h"

using std::ostringstream;

using namespace mastercore;

SendMPDialog::SendMPDialog(QWidget *parent) :
    ui(new Ui::SendMPDialog),
    clientModel(0),
    walletModel(0),
    QDialog(parent)
    // platformStyle(platformStyle)
{
    ui->setupUi(this);

    // CSS
    this->setStyleSheet(parent->styleSheet());
    // setCssProperty(ui->scrollArea, "container");
    setCssProperty(ui->balanceLabel, "text-subtitle", false);
    setCssProperty(ui->sendFromLabel, "text-subtitle", false);
    setCssProperty(ui->sendToLabel, "text-subtitle", false);
    setCssProperty(ui->amountLabel, "text-subtitle", false);
    setCssProperty(ui->globalBalanceLabel, "text-subtitle", false);
    setCssBtnPrimary(ui->sendButton);
    setCssBtnSecondary(ui->clearButton);
    initCssEditLine(ui->sendToLineEdit, true);
    initCssEditLine(ui->amountLineEdit, true);
    setCssProperty(ui->sendFromComboBox, "btn-combo-address");
    setCssProperty(ui->propertyComboBox, "btn-combo-address");
    ui->sendFromComboBox->setView(new QListView());
    ui->propertyComboBox->setView(new QListView());

    ui->copyButton->setLayoutDirection(Qt::RightToLeft);
    setCssProperty(ui->copyButton, "btn-secundary-copy");

    // Use platformStyle instead of ifdef Q_OS_MAC to hide icons on Mac
    // if (!platformStyle->getImagesOnButtons()) {
    //     ui->clearButton->setIcon(QIcon());
    //     ui->sendButton->setIcon(QIcon());
    // } else {
    //     // Use platformStyle to color the icons to match the rest of the UI
    //     ui->clearButton->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    //     ui->sendButton->setIcon(platformStyle->SingleColorIcon(":/icons/send"));
    // }

#if QT_VERSION >= 0x040700 // populate placeholder text
    ui->sendToLineEdit->setPlaceholderText("Enter address");
    ui->amountLineEdit->setPlaceholderText("Enter amount");
#endif

    // connect actions
    connect(ui->propertyComboBox, SIGNAL(activated(int)), this, SLOT(propertyComboBoxChanged(int)));
    connect(ui->sendFromComboBox, SIGNAL(activated(int)), this, SLOT(sendFromComboBoxChanged(int)));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clearButtonClicked()));
    connect(ui->sendButton, SIGNAL(clicked()), this, SLOT(sendButtonClicked()));
    connect(ui->copyButton, SIGNAL(clicked()), this, SLOT(onCopyClicked()));

    // initial update
    // balancesUpdated();
}

SendMPDialog::~SendMPDialog()
{
    delete ui;
}

void SendMPDialog::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model != NULL) {
        connect(model, SIGNAL(refreshTokenBalance()), this, SLOT(balancesUpdated()));
        connect(model, SIGNAL(reinitTokenState()), this, SLOT(balancesUpdated()));
    }
}

void SendMPDialog::setWalletModel(WalletModel *model)
{
    // use wallet model to get visibility into RPD balance changes for fees
    this->walletModel = model;
    if (model != NULL) {
       connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(updateFrom()));
    }
}

void SendMPDialog::updatePropSelector()
{
    LOCK(cs_tally);

    uint32_t nextPropIdMainEco = GetNextPropertyId(true);  // these allow us to end the for loop at the highest existing
    uint32_t nextPropIdTestEco = GetNextPropertyId(false); // property ID rather than a fixed value like 100000 (optimization)
    QString spId = ui->propertyComboBox->itemData(ui->propertyComboBox->currentIndex()).toString();
    ui->propertyComboBox->clear();
    for (unsigned int propertyId = 1; propertyId < nextPropIdMainEco; propertyId++) {
        if ((global_balance_money[propertyId] > 0) || (global_balance_reserved[propertyId] > 0)) {
            std::string spName = getPropertyName(propertyId);
            std::string spId = strprintf("%d", propertyId);
            if(spName.size()>23) spName=spName.substr(0,23) + "...";
            spName += " (#" + spId + ")";
            if (isPropertyDivisible(propertyId)) { spName += " [D]"; } else { spName += " [I]"; }
            ui->propertyComboBox->addItem(spName.c_str(),spId.c_str());
        }
    }
    for (unsigned int propertyId = 2147483647; propertyId < nextPropIdTestEco; propertyId++) {
        if ((global_balance_money[propertyId] > 0) || (global_balance_reserved[propertyId] > 0)) {
            std::string spName = getPropertyName(propertyId);
            std::string spId = strprintf("%d", propertyId);
            if(spName.size()>23) spName=spName.substr(0,23)+"...";
            spName += " (#" + spId + ")";
            if (isPropertyDivisible(propertyId)) { spName += " [D]"; } else { spName += " [I]"; }
            ui->propertyComboBox->addItem(spName.c_str(),spId.c_str());
        }
    }
    int propIdx = ui->propertyComboBox->findData(spId);
    if (propIdx != -1) { ui->propertyComboBox->setCurrentIndex(propIdx); }
}

void SendMPDialog::clearFields()
{
    ui->sendToLineEdit->setText("");
    ui->amountLineEdit->setText("");
}

void SendMPDialog::updateFrom()
{
    // check if this from address has sufficient fees for a send, if not light up warning label
    std::string currentSetFromAddress = ui->sendFromComboBox->currentText().toStdString();
    size_t spacer = currentSetFromAddress.find(" ");
    if (spacer!=std::string::npos) {
        currentSetFromAddress = currentSetFromAddress.substr(0,spacer);
        ui->sendFromComboBox->setEditable(false);
        // QLineEdit *comboDisplay = ui->sendFromComboBox->lineEdit();
        // comboDisplay->setText(QString::fromStdString(currentSetFromAddress));
        // comboDisplay->setReadOnly(true);
    }

    if (currentSetFromAddress.empty()) {
        ui->balanceLabel->setText(QString::fromStdString("Address Balance: N/A"));
        ui->feeWarningLabel->setVisible(false);
    } else {
        // update the balance for the selected address
        QString spId = ui->propertyComboBox->itemData(ui->propertyComboBox->currentIndex()).toString();
        uint32_t propertyId = spId.toUInt();
        if (propertyId > 0) {
            ui->balanceLabel->setText(QString::fromStdString("Address Balance: " + FormatMP(propertyId, GetAvailableTokenBalance(currentSetFromAddress, propertyId)) + getTokenLabel(propertyId)));
        }
        // warning label will be lit if insufficient fees for simple send (16 byte payload)
        if (CheckFee(currentSetFromAddress, 16)) {
            ui->feeWarningLabel->setVisible(false);
        } else {
            ui->feeWarningLabel->setText("WARNING: This address doesn't have enough RPD to pay network fees.");
            ui->feeWarningLabel->setVisible(true);
        }
    }
}

void SendMPDialog::updateProperty()
{
    // cache currently selected from address & clear address selector
    std::string currentSetFromAddress = ui->sendFromComboBox->currentText().toStdString();
    ui->sendFromComboBox->clear();

    // populate from address selector
    QString spId = ui->propertyComboBox->itemData(ui->propertyComboBox->currentIndex()).toString();
    uint32_t propertyId = spId.toUInt();
    LOCK(cs_tally);
    for (std::unordered_map<std::string, CMPTally>::iterator my_it = mp_tally_map.begin(); my_it != mp_tally_map.end(); ++my_it) {
        std::string address = (my_it->first).c_str();
        uint32_t id = 0;
        bool includeAddress=false;
        (my_it->second).init();
        while (0 != (id = (my_it->second).next())) {
            if(id == propertyId) { includeAddress=true; break; }
        }
        if (!includeAddress) continue; //ignore this address, has never transacted in this propertyId
        if (IsMyAddress(address) != ISMINE_SPENDABLE) continue; // ignore this address, it's not spendable
        if (!GetAvailableTokenBalance(address, propertyId)) continue; // ignore this address, has no available balance to spend
        // ui->sendFromComboBox->addItem(QString::fromStdString(address + " \t" + FormatMP(propertyId, GetAvailableTokenBalance(address, propertyId)) + getTokenLabel(propertyId)));
        ui->sendFromComboBox->addItem(QString::fromStdString(address));
    }

    // attempt to set from address back to cached value
    int fromIdx = ui->sendFromComboBox->findText(QString::fromStdString(currentSetFromAddress), Qt::MatchContains);
    if (fromIdx != -1) { ui->sendFromComboBox->setCurrentIndex(fromIdx); } // -1 means the cached from address doesn't have a balance in the newly selected property

    // populate balance for global wallet
    ui->globalBalanceLabel->setText(QString::fromStdString("Wallet Balance (Available): " + FormatMP(propertyId, global_balance_money[propertyId])));

#if QT_VERSION >= 0x040700
    // update placeholder text
    if (isPropertyDivisible(propertyId)) { ui->amountLineEdit->setPlaceholderText("Enter Divisible Amount"); } else { ui->amountLineEdit->setPlaceholderText("Enter Indivisible Amount"); }
#endif
}

void SendMPDialog::sendMPTransaction()
{
    // get the property being sent and get divisibility
    QString spId = ui->propertyComboBox->itemData(ui->propertyComboBox->currentIndex()).toString();
    if (spId.toStdString().empty()) {
        QMessageBox::critical( this, "Unable to send transaction",
        "The property selected is not valid.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
        return;
    }
    uint32_t propertyId = spId.toUInt();
    bool divisible = isPropertyDivisible(propertyId);

    // obtain the selected sender address
    std::string strFromAddress = ui->sendFromComboBox->currentText().toStdString();

    // push recipient address into a CTxDestination type and check validity
    CTxDestination fromAddress;
    if (false == strFromAddress.empty()) { fromAddress = DecodeDestination(strFromAddress); }
    if (!IsValidDestination(fromAddress)) {
        QMessageBox::critical( this, "Unable to send transaction",
        "The sender address selected is not valid.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
        return;
    }

    // obtain the entered recipient address
    std::string strRefAddress = ui->sendToLineEdit->text().toStdString();
    // push recipient address into a CTxDestination type and check validity
    CTxDestination refAddress;
    if (false == strRefAddress.empty()) { refAddress = DecodeDestination(strRefAddress); }
    if (!IsValidDestination(refAddress)) {
        QMessageBox::critical( this, "Unable to send transaction",
        "The recipient address entered is not valid.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
        return;
    }

    // warn if we have to truncate the amount due to a decimal amount for an indivisible property, but allow send to continue
    std::string strAmount = ui->amountLineEdit->text().toStdString();
    if (!divisible) {
        size_t pos = strAmount.find(".");
        if (pos!=std::string::npos) {
            std::string tmpStrAmount = strAmount.substr(0,pos);
            std::string strMsgText = "The amount entered contains a decimal however the property being sent is indivisible.\n\nThe amount entered will be truncated as follows:\n";
            strMsgText += "Original amount entered: " + strAmount + "\nAmount that will be sent: " + tmpStrAmount + "\n\n";
            strMsgText += "Do you still wish to proceed with the transaction?";
            QString msgText = QString::fromStdString(strMsgText);
            QMessageBox::StandardButton responseClick;
            responseClick = QMessageBox::question(this, "Amount truncation warning", msgText, QMessageBox::Yes|QMessageBox::No);
            if (responseClick == QMessageBox::No) {
                QMessageBox::critical( this, "Send transaction cancelled",
                "The send transaction has been cancelled.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
                return;
            }
            strAmount = tmpStrAmount;
            ui->amountLineEdit->setText(QString::fromStdString(strAmount));
        }
    }

    // use strToInt64 function to get the amount, using divisibility of the property
    int64_t sendAmount = StrToInt64(strAmount, divisible);
    if (0>=sendAmount) {
        QMessageBox::critical( this, "Unable to send transaction",
        "The amount entered is not valid.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
        return;
    }

    // check if sending address has enough funds
    int64_t balanceAvailable = GetAvailableTokenBalance(EncodeDestination(fromAddress), propertyId); //getMPbalance(EncodeDestination(fromAddress), propertyId, MONEY);
    if (sendAmount>balanceAvailable) {
        QMessageBox::critical( this, "Unable to send transaction",
        "The selected sending address does not have a sufficient balance to cover the amount entered.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
        return;
    }

    // check if wallet is still syncing, as this will currently cause a lockup if we try to send - compare our chain to peers to see if we're up to date
    // Bitcoin Core devs have removed GetNumBlocksOfPeers, switching to a time based best guess scenario
    uint32_t intBlockDate = GetLatestBlockTime();  // uint32, not using time_t for portability
    QDateTime currentDate = QDateTime::currentDateTime();
    int secs = QDateTime::fromTime_t(intBlockDate).secsTo(currentDate);
    if(secs > 90*60) {
        QMessageBox::critical( this, "Unable to send transaction",
        "The client is still synchronizing.  Sending transactions can currently be performed only when the client has completed synchronizing." );
        return;
    }

    // validation checks all look ok, let's throw up a confirmation dialog
    std::string strMsgText = "You are about to send the following transaction, please check the details thoroughly:\n\n";
    std::string propDetails = getPropertyName(propertyId).c_str();
    std::string spNum = strprintf("%d", propertyId);
    propDetails += " (#" + spNum + ")";
    strMsgText += "From: " + EncodeDestination(fromAddress) + "\nTo: " + EncodeDestination(refAddress) + "\nProperty: " + propDetails + "\nAmount that will be sent: ";
    if (divisible) { strMsgText += FormatDivisibleMP(sendAmount); } else { strMsgText += FormatIndivisibleMP(sendAmount); }
    strMsgText += "\n\nAre you sure you wish to send this transaction?";
    QString msgText = QString::fromStdString(strMsgText);
    QMessageBox::StandardButton responseClick;
    responseClick = QMessageBox::question(this, "Confirm send transaction", msgText, QMessageBox::Yes|QMessageBox::No);
    if (responseClick == QMessageBox::No) {
        QMessageBox::critical( this, "Send transaction cancelled",
        "The send transaction has been cancelled.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
        return;
    }

    // unlock the wallet
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if(!ctx.isValid()) {
        QMessageBox::critical( this, "Send transaction failed",
        "The send transaction has been cancelled.\n\nThe wallet unlock process must be completed to send a transaction." );
        return; // unlock wallet was cancelled/failed
    }

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, sendAmount);

    // request the wallet build the transaction (and if needed commit it) - note UI does not support added reference amounts currently
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(EncodeDestination(fromAddress), EncodeDestination(refAddress), "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        QMessageBox::critical( this, "Send transaction failed",
        "The send transaction has failed.\n\nThe error code was: " + QString::number(result) + "\nThe error message was:\n" + QString::fromStdString(error_str(result)));
        return;
    } else {
        if (!autoCommit) {
            PopulateSimpleDialog(rawHex, "Raw Hex (auto commit is disabled)", "Raw transaction hex");
        } else {
            PendingAdd(txid, EncodeDestination(fromAddress), TOKEN_TYPE_SIMPLE_SEND, propertyId, sendAmount);
            PopulateTXSentDialog(txid.GetHex());
        }
    }
    clearFields();
}

void SendMPDialog::sendFromComboBoxChanged(int idx)
{
    updateFrom();
}

void SendMPDialog::propertyComboBoxChanged(int idx)
{
    updateProperty();
    updateFrom();
}

void SendMPDialog::clearButtonClicked()
{
    clearFields();
}

void SendMPDialog::sendButtonClicked()
{
    sendMPTransaction();
}

void SendMPDialog::onCopyClicked()
{
    GUIUtil::setClipboard(ui->sendFromComboBox->currentText());
    inform(tr("Address copied"));
}

void SendMPDialog::inform(const QString& text)
{
    if (!snackBar) snackBar = new SnackBar(nullptr, this);
    snackBar->setText(text);
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

void SendMPDialog::balancesUpdated()
{
    updatePropSelector();
    updateProperty();
    updateFrom();
}
