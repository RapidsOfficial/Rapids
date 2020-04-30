// Copyright (c) 2019-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SENDCUSTOMFEEDIALOG_H
#define SENDCUSTOMFEEDIALOG_H

#include <QDialog>
#include "amount.h"
#include "qt/pivx/snackbar.h"

class PIVXGUI;
class WalletModel;

namespace Ui {
class SendCustomFeeDialog;
}

class SendCustomFeeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendCustomFeeDialog(PIVXGUI* parent, WalletModel* model);
    ~SendCustomFeeDialog();

    void showEvent(QShowEvent* event) override;
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
    Ui::SendCustomFeeDialog* ui;
    WalletModel* walletModel = nullptr;
    CFeeRate feeRate;
    SnackBar* snackBar = nullptr;
    void inform(const QString& text);
};

#endif // SENDCUSTOMFEEDIALOG_H
