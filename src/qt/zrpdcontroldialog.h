// Copyright (c) 2017-2020 The PIVX developers
// Copyright (c) 2018-2020 The Rapids developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZRPDCONTROLDIALOG_H
#define ZRPDCONTROLDIALOG_H

#include <QDialog>
#include <QTreeWidgetItem>
#include "zrpd/zerocoin.h"

class CZerocoinMint;
class WalletModel;

namespace Ui {
class ZRpdControlDialog;
}

class CZRpdControlWidgetItem : public QTreeWidgetItem
{
public:
    explicit CZRpdControlWidgetItem(QTreeWidget *parent, int type = Type) : QTreeWidgetItem(parent, type) {}
    explicit CZRpdControlWidgetItem(int type = Type) : QTreeWidgetItem(type) {}
    explicit CZRpdControlWidgetItem(QTreeWidgetItem *parent, int type = Type) : QTreeWidgetItem(parent, type) {}

    bool operator<(const QTreeWidgetItem &other) const;
};

class ZRpdControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ZRpdControlDialog(QWidget *parent);
    ~ZRpdControlDialog();

    void setModel(WalletModel* model);

    static std::set<std::string> setSelectedMints;
    static std::set<CMintMeta> setMints;
    static std::vector<CMintMeta> GetSelectedMints();

private:
    Ui::ZRpdControlDialog *ui;
    WalletModel* model;

    void updateList();
    void updateLabels();

    enum {
        COLUMN_CHECKBOX,
        COLUMN_DENOMINATION,
        COLUMN_PUBCOIN,
        COLUMN_VERSION,
        COLUMN_CONFIRMATIONS,
        COLUMN_ISSPENDABLE
    };
    friend class CZRpdControlWidgetItem;

private Q_SLOTS:
    void updateSelection(QTreeWidgetItem* item, int column);
    void ButtonAllClicked();
};

#endif // ZRPDCONTROLDIALOG_H
