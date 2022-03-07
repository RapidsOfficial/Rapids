// Copyright (c) 2011-2013 The Bitcoin developers // Distributed under the MIT/X11 software license, see the accompanying // file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "usernamesdialog.h"
#include "ui_usernamesdialog.h"

#include "qt/rapids/qtutils.h"

#include "clientmodel.h"
#include "walletmodel.h"
#include "guiutil.h"

#include "tokencore/tokencore.h"
#include "tokencore/sp.h"
#include "tokencore/tally.h"
#include "tokencore/walletutils.h"
#include "tokencore/tx.h"

#include "amount.h"
#include "sync.h"
// #include "ui_interface.h"
#include "wallet/wallet.h"

#include <stdint.h>
#include <map>
#include <sstream>
#include <string>

#include <QAbstractItemView>
#include <QAction>
#include <QDialog>
#include <QHeaderView>
#include <QMenu>
#include <QModelIndex>
#include <QPoint>
#include <QResizeEvent>
#include <QString>
#include <QTableWidgetItem>
#include <QWidget>

using std::ostringstream;
using std::string;
using namespace mastercore;

UsernamesDialog::UsernamesDialog(QWidget *parent) :
    QDialog(parent), ui(new Ui::usernamesDialog), clientModel(0), walletModel(0)
{
    // setup
    ui->setupUi(this);

    // CSS
    this->setStyleSheet(parent->styleSheet());
    setCssProperty(ui->balancesTable, "token-table");

    ui->balancesTable->setShowGrid(false);
    ui->balancesTable->setStyleSheet("QTableWidget {color:white;background-color:transparent;} QHeaderView::section { font-weight: bold; }");

    // setCssProperty(ui->balancesTable, "token-table");

    ui->balancesTable->setColumnCount(2);
    ui->balancesTable->setHorizontalHeaderItem(0, new QTableWidgetItem("Address"));
    ui->balancesTable->setHorizontalHeaderItem(1, new QTableWidgetItem("Username"));
    borrowedColumnResizingFixer = new GUIUtil::TableViewLastColumnResizingFixer(ui->balancesTable, 100, 100);
    // note neither resizetocontents or stretch allow user to adjust - go interactive then manually set widths
    #if QT_VERSION < 0x050000
       ui->balancesTable->horizontalHeader()->setResizeMode(0, QHeaderView::Interactive);
       ui->balancesTable->horizontalHeader()->setResizeMode(1, QHeaderView::Interactive);
    #else
       ui->balancesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
       ui->balancesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    #endif
    ui->balancesTable->setAlternatingRowColors(true);

    // do an initial population
    PopulateUsernames();

    // initial resizing
    // ui->balancesTable->resizeColumnToContents(0);
    // ui->balancesTable->resizeColumnToContents(1);
    // borrowedColumnResizingFixer->stretchColumnWidth(0);
    // borrowedColumnResizingFixer->stretchColumnWidth(1);
    ui->balancesTable->verticalHeader()->setVisible(false);
    ui->balancesTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->balancesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->balancesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->balancesTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->balancesTable->setTabKeyNavigation(false);
    ui->balancesTable->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->balancesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Actions
    QAction *balancesCopyUsernameAction = new QAction(tr("Copy username"), this);
    QAction *balancesCopyAddressAction = new QAction(tr("Copy address"), this);

    contextMenu = new QMenu();
    contextMenu->addAction(balancesCopyUsernameAction);
    contextMenu->addAction(balancesCopyAddressAction);

    // Connect actions
    connect(ui->balancesTable, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
    connect(balancesCopyAddressAction, SIGNAL(triggered()), this, SLOT(balancesCopyCol0()));
    connect(balancesCopyUsernameAction, SIGNAL(triggered()), this, SLOT(balancesCopyCol1()));
}

UsernamesDialog::~UsernamesDialog()
{
    delete ui;
}

void UsernamesDialog::reinitToken()
{
    ui->balancesTable->setRowCount(0);
    PopulateUsernames(); // 2147483646 = summary (last possible ID for test eco props)
}

void UsernamesDialog::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model != NULL) {
        connect(model, SIGNAL(refreshTokenBalance()), this, SLOT(balancesUpdated()));
        connect(model, SIGNAL(reinitTokenState()), this, SLOT(reinitToken()));
    }
}

void UsernamesDialog::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if (model != NULL) { } // do nothing, signals from walletModel no longer needed
}

void UsernamesDialog::AddRow(const std::string& label, const std::string& address)
{
    int workingRow = ui->balancesTable->rowCount();
    ui->balancesTable->insertRow(workingRow);
    QTableWidgetItem *labelCell = new QTableWidgetItem(QString::fromStdString(label));
    QTableWidgetItem *addressCell = new QTableWidgetItem(QString::fromStdString(address));

    if (workingRow % 2 == 0) {
        labelCell->setData(Qt::BackgroundRole, QColor(27,27,27));
        addressCell->setData(Qt::BackgroundRole, QColor(27,27,27));
    } else {
        labelCell->setData(Qt::BackgroundRole, QColor(40,40,40));
        addressCell->setData(Qt::BackgroundRole, QColor(40,40,40));
    }

    labelCell->setTextAlignment(Qt::AlignLeft + Qt::AlignVCenter);
    addressCell->setTextAlignment(Qt::AlignLeft + Qt::AlignVCenter);

    ui->balancesTable->setItem(workingRow, 0, labelCell);
    ui->balancesTable->setItem(workingRow, 1, addressCell);
}

void UsernamesDialog::PopulateUsernames()
{
    ui->balancesTable->setRowCount(0); // fresh slate (note this will automatically cleanup all existing QWidgetItems in the table)

    LOCK(cs_tally);

    // loop over the wallet property list and add the wallet totals
    for (std::set<uint32_t>::iterator it = global_wallet_property_list.begin() ; it != global_wallet_property_list.end(); ++it) {
        uint32_t propertyId = *it;
        std::string username = getPropertyName(propertyId).c_str();

        if (global_balance_money[propertyId] > 0 || global_balance_reserved[propertyId] > 0) {
            if (IsUsernameValid(username)) {
                std::string address = GetUsernameAddress(username);
                AddRow(address, username);
            }
        }
    }
}

void UsernamesDialog::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->balancesTable->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void UsernamesDialog::balancesCopyCol0()
{
    GUIUtil::setClipboard(ui->balancesTable->item(ui->balancesTable->currentRow(),0)->text());
}

void UsernamesDialog::balancesCopyCol1()
{
    GUIUtil::setClipboard(ui->balancesTable->item(ui->balancesTable->currentRow(),1)->text());
}

void UsernamesDialog::balancesUpdated()
{
    PopulateUsernames();
}

// We override the virtual resizeEvent of the QWidget to adjust tables column
// sizes as the tables width is proportional to the dialogs width.
void UsernamesDialog::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    borrowedColumnResizingFixer->stretchColumnWidth(1);
}

