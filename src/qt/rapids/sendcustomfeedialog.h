// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2018-2020 The Rapids developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SENDCUSTOMFEEDIALOG_H
#define SENDCUSTOMFEEDIALOG_H

#include <QDialog>
#include "amount.h"

class RapidsGUI;
class WalletModel;

namespace Ui {
class SendCustomFeeDialog;
}

class SendCustomFeeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendCustomFeeDialog(RapidsGUI *parent = nullptr);
    ~SendCustomFeeDialog();

    void setWalletModel(WalletModel* model);
    void showEvent(QShowEvent *event) override;
    CFeeRate getFeeRate();
    bool isCustomFeeChecked();
    void clear();

public Q_SLOTS:
    void onRecommendedChecked();
    void onCustomChecked();
    void updateFee();
    void onChangeTheme(bool isLightTheme, QString& theme);

protected Q_SLOTS:
    void accept() override;

private:
    Ui::SendCustomFeeDialog *ui;
    WalletModel* walletModel = nullptr;
    CFeeRate feeRate;
};

#endif // SENDCUSTOMFEEDIALOG_H
