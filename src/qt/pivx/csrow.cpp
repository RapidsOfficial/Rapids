// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/csrow.h"
#include "qt/pivx/forms/ui_csrow.h"

CSRow::CSRow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CSRow)
{
    ui->setupUi(this);
    ui->labelName->setProperty("cssClass", "text-list-title1");
    ui->labelAddress->setProperty("cssClass", "text-list-body2");
    ui->labelStaking->setProperty("cssClass", "text-list-caption-medium");
    ui->labelAmount->setProperty("cssClass", "text-list-amount-unconfirmed");
}

void CSRow::updateView(QString address, QString label, bool isStaking, QString amount) {
    ui->labelName->setText(label);
    ui->labelAddress->setText(address);
    ui->labelAmount->setText(amount);
    ui->labelStaking->setText((isStaking ? "Staking" : "Not staking"));
    //ui->labelStaking->setProperty("cssClass", (isStaking) ? "text-list-caption-medium-green" : "text-list-caption-medium");
}

void CSRow::updateState(bool isLightTheme, bool isHovered, bool isSelected) {
    ui->lblDivisory->setStyleSheet((isLightTheme) ?  "background-color:#bababa" : "background-color:#40ffffff");
    ui->pushButtonMenu->setVisible(isHovered);
}

void CSRow::enterEvent(QEvent *) {
    ui->pushButtonMenu->setVisible(true);
    update();
}

void CSRow::leaveEvent(QEvent *) {
    ui->pushButtonMenu->setVisible(false);
    update();
}

CSRow::~CSRow(){
    delete ui;
}
