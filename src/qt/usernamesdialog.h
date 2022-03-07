// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef USERNAMESDIALOG_H
#define USERNAMESDIALOG_H

#include "guiutil.h"

#include <QDialog>

class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QMenu;
class QPoint;
class QResizeEvent;
class QString;
class QWidget;
QT_END_NAMESPACE

namespace Ui {
    class usernamesDialog;
}

class UsernamesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UsernamesDialog(QWidget *parent = 0);
    ~UsernamesDialog();

    void setClientModel(ClientModel *model);
    void setWalletModel(WalletModel *model);
    void AddRow(const std::string& label, const std::string& address);
    void PopulateUsernames();

private:
    Ui::usernamesDialog *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    QMenu *contextMenu;
    QMenu *contextMenuSummary;

    GUIUtil::TableViewLastColumnResizingFixer *borrowedColumnResizingFixer;
    virtual void resizeEvent(QResizeEvent *event);

public Q_SLOTS:
    void balancesUpdated();
    void reinitToken();

private Q_SLOTS:
    void contextualMenu(const QPoint &point);
    void balancesCopyCol0();
    void balancesCopyCol1();

Q_SIGNALS:
    /**  Fired when a message should be reported to the user */
    void message(const QString &title, const QString &message, unsigned int style);
};

#endif // USERNAMESDIALOG_H
