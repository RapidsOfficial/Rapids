// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CREATEMPDIALOG_H
#define CREATEMPDIALOG_H

#include "walletmodel.h"

#include <QDialog>
#include <QString>

#include "qt/rapids/snackbar.h"

#include "platformstyle.h"

class ClientModel;

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace Ui {
    class CreateMPDialog;
}

/** Dialog for sending Master Protocol tokens */
class CreateMPDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreateMPDialog(QWidget *parent = 0);
    ~CreateMPDialog();

    void setClientModel(ClientModel *model);
    void setWalletModel(WalletModel *model);

    void clearFields();
    void sendMPTransaction();
    void updateProperty();
    void updatePropSelector();

public Q_SLOTS:
    void propertyComboBoxChanged(int idx);
    void sendFromComboBoxChanged(int idx);
    void clearButtonClicked();
    void sendButtonClicked();
    void onCopyClicked();
    void balancesUpdated();
    void updateFrom();

private:
    Ui::CreateMPDialog *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    SnackBar *snackBar = nullptr;

    void inform(const QString& text);

Q_SIGNALS:
    // Fired when a message should be reported to the user
    void message(const QString &title, const QString &message, unsigned int style);
};

#endif // CREATEMPDIALOG_H
