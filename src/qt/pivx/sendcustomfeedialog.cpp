// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/sendcustomfeedialog.h"
#include "qt/pivx/forms/ui_sendcustomfeedialog.h"
#include "qt/pivx/qtutils.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include <QListView>
#include <QComboBox>

SendCustomFeeDialog::SendCustomFeeDialog(PIVXGUI *parent) :
    QDialog(parent),
    ui(new Ui::SendCustomFeeDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());
    setCssProperty(ui->frame, "container-dialog");

    // Text
    ui->labelTitle->setText(tr("Customize Fee"));
    ui->labelMessage->setText(tr("Customize the transaction fee, depending on the fee value your transaction might be included faster in the blockchain."));
    setCssProperty(ui->labelTitle, "text-title-dialog");
    setCssProperty(ui->labelMessage, "text-main-grey");

    // Recommended
    setCssProperty(ui->labelFee, "text-main-grey-big");
    setCssProperty(ui->comboBoxRecommended, "btn-combo-dialog");
    ui->comboBoxRecommended->setView(new QListView());
    ui->comboBoxRecommended->addItem(tr("Normal"), 5);
    ui->comboBoxRecommended->addItem(tr("Slow"), 20);
    ui->comboBoxRecommended->addItem(tr("Fast"), 1);

    // Custom
    setCssProperty(ui->labelCustomFee, "label-subtitle-dialog");
    ui->lineEditCustomFee->setPlaceholderText("0.000001");
    initCssEditLine(ui->lineEditCustomFee, true);
    GUIUtil::setupAmountWidget(ui->lineEditCustomFee, this);

    // Buttons
    setCssProperty(ui->btnEsc, "ic-close");
    setCssProperty(ui->btnCancel, "btn-dialog-cancel");
    ui->btnSave->setText(tr("SAVE"));
    setCssBtnPrimary(ui->btnSave);

    connect(ui->btnEsc, &QPushButton::clicked, this, &SendCustomFeeDialog::close);
    connect(ui->btnCancel, &QPushButton::clicked, this, &SendCustomFeeDialog::close);
    connect(ui->btnSave, &QPushButton::clicked, this, &SendCustomFeeDialog::accept);
    connect(ui->checkBoxCustom, &QCheckBox::clicked, this, &SendCustomFeeDialog::onCustomChecked);
    connect(ui->checkBoxRecommended, &QCheckBox::clicked, this, &SendCustomFeeDialog::onRecommendedChecked);
    connect(ui->comboBoxRecommended, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
        this, &SendCustomFeeDialog::updateFee);
    if (parent)
        connect(parent, &PIVXGUI::themeChanged, this, &SendCustomFeeDialog::onChangeTheme);
    ui->checkBoxRecommended->setChecked(true);
}

void SendCustomFeeDialog::setWalletModel(WalletModel* walletModel){
    this->walletModel = walletModel;
}

void SendCustomFeeDialog::showEvent(QShowEvent *event){
    updateFee();
    if (walletModel && walletModel->hasWalletCustomFee()) {
        ui->checkBoxCustom->setChecked(true);
        onCustomChecked();
    }
}

void SendCustomFeeDialog::onCustomChecked(){
    bool isChecked = ui->checkBoxCustom->checkState() == Qt::Checked;
    ui->lineEditCustomFee->setEnabled(isChecked);
    ui->comboBoxRecommended->setEnabled(!isChecked);
    ui->checkBoxRecommended->setChecked(!isChecked);

    if(walletModel && ui->lineEditCustomFee->text().isEmpty()) {
        CAmount nFee;
        walletModel->getWalletCustomFee(nFee);
        ui->lineEditCustomFee->setText(BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), nFee));
    }
}

void SendCustomFeeDialog::onRecommendedChecked(){
    bool isChecked = ui->checkBoxRecommended->checkState() == Qt::Checked;
    ui->lineEditCustomFee->setEnabled(!isChecked);
    ui->comboBoxRecommended->setEnabled(isChecked);
    ui->checkBoxCustom->setChecked(!isChecked);
}

// Fast = 1.
// Medium = 5
// Slow = 20
void SendCustomFeeDialog::updateFee(){
    if (!walletModel || !walletModel->getOptionsModel()) return;

    QVariant num = ui->comboBoxRecommended->currentData();
    bool res = false;
    int nBlocksToConfirm = num.toInt(&res);
    if (res) {
        feeRate = mempool.estimateFee(nBlocksToConfirm);
        if (feeRate <= CWallet::minTxFee) feeRate = CWallet::minTxFee;    // not enough data => minfee
        ui->labelFee->setText(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                           feeRate.GetFeePerK()) + "/kB");
    }
}

void SendCustomFeeDialog::accept()
{
    // Persist custom fee in the wallet
    if (walletModel)
        walletModel->setWalletCustomFee(ui->checkBoxCustom->checkState() == Qt::Checked, getFeeRate().GetFeePerK());
    QDialog::accept();
}

void SendCustomFeeDialog::clear()
{
    onRecommendedChecked();
    updateFee();
}

CFeeRate SendCustomFeeDialog::getFeeRate(){
    return ui->checkBoxRecommended->isChecked() ?
           feeRate : CFeeRate(GUIUtil::parseValue(ui->lineEditCustomFee->text(), walletModel->getOptionsModel()->getDisplayUnit()));
}

bool SendCustomFeeDialog::isCustomFeeChecked()
{
    return ui->checkBoxCustom->checkState() == Qt::Checked;
}

void SendCustomFeeDialog::onChangeTheme(bool isLightTheme, QString& theme){
    this->setStyleSheet(theme);
    updateStyle(this);
}

SendCustomFeeDialog::~SendCustomFeeDialog(){
    delete ui;
}
