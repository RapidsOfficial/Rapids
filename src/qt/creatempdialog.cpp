// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "creatempdialog.h"
#include "ui_creatempdialog.h"

#include "qt/rapids/qtutils.h"

#include "tokencore_qtutils.h"

#include "clientmodel.h"
#include "walletmodel.h"

#include "platformstyle.h"
#include "utilmoneystr.h"

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
#include "tokencore/tx.h"

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

CreateMPDialog::CreateMPDialog(QWidget *parent) :
    ui(new Ui::CreateMPDialog),
    clientModel(0),
    walletModel(0),
    QDialog(parent)
{
    ui->setupUi(this);

    // CSS
    this->setStyleSheet(parent->styleSheet());

    setCssProperty(ui->balanceLabel, "text-subtitle", false);
    setCssProperty(ui->sendFromLabel, "text-subtitle", false);
    setCssProperty(ui->amountLabel, "text-subtitle", false);
    setCssBtnPrimary(ui->sendButton);
    setCssBtnSecondary(ui->clearButton);
    initCssEditLine(ui->amountLineEdit, true);
    setCssProperty(ui->sendFromComboBox, "btn-combo-address");
    ui->sendFromComboBox->setView(new QListView());

    ui->copyButton->setLayoutDirection(Qt::RightToLeft);
    setCssProperty(ui->copyButton, "btn-secundary-copy");

    setCssProperty(ui->nameLabel, "text-subtitle", false);
    setCssProperty(ui->urlLabel, "text-subtitle", false);
    setCssProperty(ui->categoryLabel, "text-subtitle", false);
    setCssProperty(ui->subcategoryLabel, "text-subtitle", false);
    setCssProperty(ui->ipfsLabel, "text-subtitle", false);

    initCssEditLine(ui->nameLineEdit, true);
    initCssEditLine(ui->urlLineEdit, true);
    initCssEditLine(ui->categoryLineEdit, true);
    initCssEditLine(ui->subcategoryLineEdit, true);
    initCssEditLine(ui->ipfsLineEdit, true);

    ui->amountLineEdit->setPlaceholderText("Enter Indivisible Amount");
    ui->nameLineEdit->setPlaceholderText("Enter name (example TOKEN/rTOKEN)");
    ui->urlLineEdit->setPlaceholderText("Enter website URL (optional)");
    ui->categoryLineEdit->setPlaceholderText("Enter category (optional)");
    ui->subcategoryLineEdit->setPlaceholderText("Enter subcategory (optional)");
    ui->ipfsLineEdit->setPlaceholderText("Enter ipfs content hash (optional)");

    // connect actions
    connect(ui->sendFromComboBox, SIGNAL(activated(int)), this, SLOT(sendFromComboBoxChanged(int)));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clearButtonClicked()));
    connect(ui->sendButton, SIGNAL(clicked()), this, SLOT(sendButtonClicked()));
    connect(ui->copyButton, SIGNAL(clicked()), this, SLOT(onCopyClicked()));

    connect(ui->divisibleCheckBox, SIGNAL(clicked(bool)), this, SLOT(ChangeDivisibleState(bool)));
}

void CreateMPDialog::ChangeDivisibleState(bool divisible)
{
    if (divisible) { ui->amountLineEdit->setPlaceholderText("Enter Divisible Amount"); } else { ui->amountLineEdit->setPlaceholderText("Enter Indivisible Amount"); }
}

CreateMPDialog::~CreateMPDialog()
{
    delete ui;
}

void CreateMPDialog::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model != NULL) {
        connect(model, SIGNAL(refreshTokenBalance()), this, SLOT(balancesUpdated()));
        connect(model, SIGNAL(reinitTokenState()), this, SLOT(balancesUpdated()));
    }
}

void CreateMPDialog::setWalletModel(WalletModel *model)
{
    // use wallet model to get visibility into RPD balance changes for fees
    this->walletModel = model;
    if (model != NULL) {
       connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(updateFrom()));
    }

    balancesUpdated();
}

void CreateMPDialog::clearFields()
{
    ui->amountLineEdit->setText("");
    ui->nameLineEdit->setText("");
    ui->urlLineEdit->setText("");
    ui->categoryLineEdit->setText("");
    ui->subcategoryLineEdit->setText("");
    ui->ipfsLineEdit->setText("");
}

void CreateMPDialog::updateFrom()
{
    // check if this from address has sufficient fees for a send, if not light up warning label
    std::string currentSetFromAddress = ui->sendFromComboBox->currentText().toStdString();
    size_t spacer = currentSetFromAddress.find(" ");
    if (spacer!=std::string::npos) {
        currentSetFromAddress = currentSetFromAddress.substr(0,spacer);
        ui->sendFromComboBox->setEditable(true);

        QLineEdit *comboDisplay = ui->sendFromComboBox->lineEdit();
        initCssEditLine(comboDisplay, true);
        comboDisplay->setText(QString::fromStdString(currentSetFromAddress));
        comboDisplay->setReadOnly(true);

        comboDisplay->setStyleSheet("QLineEdit {color:white;background-color:transparent;padding:40px 0px;border: none;}");
    }

    if (currentSetFromAddress.empty()) {
        ui->balanceLabel->setText(QString::fromStdString("Address Balance: N/A"));
        ui->feeWarningLabel->setVisible(false);
    } else {
        ui->sendFromComboBox->currentText().toStdString();

        // update the balance for the selected address
        std::map<CTxDestination, CAmount> balances = pwalletMain->GetAddressBalances();
        ui->balanceLabel->setText(QString::fromStdString("Address Balance: " + FormatMoney(balances[DecodeDestination(currentSetFromAddress)]) + " RPD"));

        // warning label will be lit if insufficient fees for simple send (16 byte payload)
        if (CheckFee(currentSetFromAddress, 16)) {
            ui->feeWarningLabel->setVisible(false);
        } else {
            ui->feeWarningLabel->setText("WARNING: This address doesn't have enough RPD to pay network fees.");
            ui->feeWarningLabel->setVisible(true);
        }
    }
}

void CreateMPDialog::updateProperty()
{
    ui->sendFromComboBox->clear();

    std::map<CTxDestination, CAmount> balances = pwalletMain->GetAddressBalances();
    for (std::set<CTxDestination> grouping : pwalletMain->GetAddressGroupings()) {
        for (CTxDestination address : grouping) {
            if (balances[address] == 0)
                continue;

            std::string addressText = EncodeDestination(address) + " (" + FormatMoney(balances[address]) + " RPD)";
            ui->sendFromComboBox->addItem(QString::fromStdString(addressText));
        }
    }
}

void CreateMPDialog::sendMPTransaction()
{
    bool divisible = ui->divisibleCheckBox->isChecked();

    // obtain the selected sender address
    std::string strFromAddress = ui->sendFromComboBox->currentText().toStdString();

    // push recipient address into a CTxDestination type and check validity
    CTxDestination fromAddress;
    if (false == strFromAddress.empty()) { fromAddress = DecodeDestination(strFromAddress); }
    if (!IsValidDestination(fromAddress)) {
        CriticalDialog("Unable to issue token",
        "The sender address selected is not valid.\n\nPlease double-check the transction details thoroughly before retrying to issue token.");
        return;
    }

    // warn if we have to truncate the amount due to a decimal amount for an indivisible property, but allow send to continue
    std::string strAmount = ui->amountLineEdit->text().toStdString();
    if (!divisible) {
        size_t pos = strAmount.find(".");
        if (pos != std::string::npos) {
            std::string tmpStrAmount = strAmount.substr(0,pos);
            std::string strMsgText = "The amount entered contains a decimal however the property being sent is indivisible.\n\nThe amount entered will be truncated as follows:\n";
            strMsgText += "Original amount entered: " + strAmount + "\nAmount that will be sent: " + tmpStrAmount + "\n\n";
            strMsgText += "Do you still wish to proceed with the transaction?";
            QString msgText = QString::fromStdString(strMsgText);

            QMessageBox sendDialog;
            sendDialog.setIcon(QMessageBox::Information);
            sendDialog.setText(msgText);
            sendDialog.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
            sendDialog.setDefaultButton(QMessageBox::Yes);
            sendDialog.setButtonText(QMessageBox::Yes, "Confirm token issuance");

            if (sendDialog.exec() == QMessageBox::No) {
                CriticalDialog("Token issuance cancelled",
                "The token issuance transaction has been cancelled.\n\nPlease double-check the transction details thoroughly before retrying to issue token.");
                return;
            }

            strAmount = tmpStrAmount;
            ui->amountLineEdit->setText(QString::fromStdString(strAmount));
        }
    }

    // use strToInt64 function to get the amount, using divisibility of the property
    int64_t issueAmount = StrToInt64(strAmount, divisible);
    if (0 >= issueAmount) {
        CriticalDialog("Unable to issue token",
        "The amount entered is not valid.\n\nPlease double-check the transction details thoroughly before retrying to issue token.");
        return;
    }

    std::string name = ui->nameLineEdit->text().toStdString();
    if (!IsTokenNameValid(name) && !IsUsernameValid(name)) {
        CriticalDialog("Unable to issue token",
        "The name entered is not valid.\n\nPlease double-check the transction details thoroughly before retrying to issue token.");
        return;
    }

    if (pDbSpInfo->findSPByName(name) > 0) {
        CriticalDialog("Unable to issue token",
        "Token with this name already exists.\n\nPlease double-check the transction details thoroughly before retrying to issue token.");
        return;
    }

    std::string url = ui->urlLineEdit->text().toStdString();
    std::string category = ui->categoryLineEdit->text().toStdString();
    std::string subcategory = ui->subcategoryLineEdit->text().toStdString();

    std::string ipfs = ui->ipfsLineEdit->text().toStdString();
    if (!IsTokenIPFSValid(ipfs)) {
        CriticalDialog("Unable to issue token",
        "The IPFS hash entered is not valid.\n\nPlease double-check the transction details thoroughly before retrying to issue token.");
        return;
    }

    // check if wallet is still syncing, as this will currently cause a lockup if we try to send - compare our chain to peers to see if we're up to date
    // Bitcoin Core devs have removed GetNumBlocksOfPeers, switching to a time based best guess scenario
    uint32_t intBlockDate = GetLatestBlockTime();  // uint32, not using time_t for portability
    QDateTime currentDate = QDateTime::currentDateTime();
    int secs = QDateTime::fromTime_t(intBlockDate).secsTo(currentDate);
    if(secs > 90 * 60) {
        CriticalDialog("Unable to issue token",
        "The client is still synchronizing.  Sending transactions can currently be performed only when the client has completed synchronizing." );
        return;
    }

    // validation checks all look ok, let's throw up a confirmation dialog
    std::string strMsgText = "You are about to issue following token, please check the details thoroughly:\n\n";

    strMsgText += "From: " + EncodeDestination(fromAddress);
    strMsgText += "\nName: " + name;

    if (url != "")
        strMsgText += "\nURL: " + url;

    if (category != "")
        strMsgText += "\nCategory: " + category;

    if (subcategory != "")
        strMsgText += "\nSubategory: " + subcategory;

    if (ipfs != "")
        strMsgText += "\nIPFS: " + ipfs;

    strMsgText += "\nAmount that will be issued: ";

    if (divisible) { strMsgText += FormatDivisibleMP(issueAmount); } else { strMsgText += FormatIndivisibleMP(issueAmount); }

    strMsgText += " " + name;

    strMsgText += "\n\nAre you sure you wish to issue this token?";
    QString msgText = QString::fromStdString(strMsgText);

    QMessageBox sendDialog;
    sendDialog.setIcon(QMessageBox::Information);
    sendDialog.setText(msgText);
    sendDialog.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
    sendDialog.setDefaultButton(QMessageBox::Yes);
    sendDialog.setButtonText( QMessageBox::Yes, "Confirm token issuance");

    if (sendDialog.exec() == QMessageBox::No) {
        CriticalDialog("Token issuance cancelled", "Token issuance transaction has been cancelled.\n\nPlease double-check the transction details thoroughly before retrying to issue token.");
        return;
    }

    // unlock the wallet
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if(!ctx.isValid()) {
        CriticalDialog("Token issuance failed",
        "Token issuance transaction has been cancelled.\n\nThe wallet unlock process must be completed to send a transaction.");
        return; // unlock wallet was cancelled/failed
    }

    // create a payload for the transaction
    int type = divisible ? 2 : 1;

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(1, type, 0, category, subcategory, name, url, ipfs, issueAmount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(EncodeDestination(fromAddress), "", "", 0, payload, txid, rawHex, autoCommit, true);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        CriticalDialog("Token issuance failed",
        "Token issuance failed.\n\nThe error code was: " + QString::number(result).toStdString() + "\nThe error message was:\n" + error_str(result));
        return;
    } else {
        if (!autoCommit) {
            PopulateSimpleDialog(rawHex, "Raw Hex (auto commit is disabled)", "Raw transaction hex");
        } else {
            // ToDo: Fix this ?
            // PendingAdd(txid, EncodeDestination(fromAddress), TOKEN_TYPE_CREATE_PROPERTY_FIXED, 0, issueAmount);
            PopulateTXSentDialog(txid.GetHex());
        }
    }

    clearFields();
}

void CreateMPDialog::sendFromComboBoxChanged(int idx)
{
    updateFrom();
}

void CreateMPDialog::clearButtonClicked()
{
    clearFields();
}

void CreateMPDialog::sendButtonClicked()
{
    sendMPTransaction();
}

void CreateMPDialog::onCopyClicked()
{
    GUIUtil::setClipboard(ui->sendFromComboBox->currentText());
    inform(tr("Address copied"));
}

void CreateMPDialog::inform(const QString& text)
{
    if (!snackBar) snackBar = new SnackBar(nullptr, this);
    snackBar->setText(text);
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

void CreateMPDialog::balancesUpdated()
{
    updateProperty();
    updateFrom();
}
