// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/settings/settingsexportcsv.h"
#include "qt/pivx/settings/forms/ui_settingsexportcsv.h"
#include <QFile>
#include "csvmodelwriter.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "qt/pivx/qtutils.h"
#include "guiinterface.h"

SettingsExportCSV::SettingsExportCSV(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsExportCSV)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title
    ui->labelTitle->setText(tr("Export Accounting"));
    ui->labelTitle->setProperty("cssClass", "text-title-screen");

    // Subtitle
    ui->labelSubtitle1->setText(tr("Export all of your transactions in a csv file."));
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    // Location
    ui->labelSubtitleLocation->setText(tr("Where"));
    ui->labelSubtitleLocation->setProperty("cssClass", "text-title");

    ui->pushButtonDocuments->setText(tr("Select folder..."));
    ui->pushButtonDocuments->setProperty("cssClass", "btn-edit-primary-folder");
    setShadow(ui->pushButtonDocuments);

    // Buttons
    ui->pushButtonSave->setText(tr("Export"));
    setCssBtnPrimary(ui->pushButtonSave);

    SortEdit* lineEdit = new SortEdit(ui->comboBoxSort);
    connect(lineEdit, &SortEdit::Mouse_Pressed, [this](){ui->comboBoxSort->showPopup();});
    setSortTx(ui->comboBoxSort, lineEdit);

    SortEdit* lineEditType = new SortEdit(ui->comboBoxSortType);
    connect(lineEditType, &SortEdit::Mouse_Pressed, [this](){ui->comboBoxSortType->showPopup();});
    setSortTxTypeFilter(ui->comboBoxSortType, lineEditType);
    ui->comboBoxSortType->setCurrentIndex(0);

    connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(exportClicked()));
    connect(ui->pushButtonDocuments, SIGNAL(clicked()), this, SLOT(selectFileOutput()));
}

void SettingsExportCSV::selectFileOutput()
{
    QString filenameRet = GUIUtil::getSaveFileName(this,
                                        tr("Export CSV"), QString(),
                                        tr("PIVX_csv_export(*.csv)"), NULL);

    if (!filenameRet.isEmpty()) {
        filename = filenameRet;
        ui->pushButtonDocuments->setText(filename);
    }
}

void SettingsExportCSV::exportClicked()
{
    if (filename.isNull()) {
        inform(tr("Please select a folder to export the csv file first."));
        return;
    }

    CSVModelWriter writer(filename);
    bool fExport = false;

    if (walletModel) {

        if (!txFilter) {
            // Set up transaction list
            txFilter = new TransactionFilterProxy();
            txFilter->setDynamicSortFilter(true);
            txFilter->setSortCaseSensitivity(Qt::CaseInsensitive);
            txFilter->setFilterCaseSensitivity(Qt::CaseInsensitive);
            txFilter->setSortRole(Qt::EditRole);
            txFilter->setSourceModel(walletModel->getTransactionTableModel());
        }

        // First type filter
        txFilter->setTypeFilter(ui->comboBoxSortType->itemData(ui->comboBoxSortType->currentIndex()).toInt());

        // Second tx filter.
        int columnIndex = TransactionTableModel::Date;
        Qt::SortOrder order = Qt::DescendingOrder;
        switch (ui->comboBoxSort->itemData(ui->comboBoxSort->currentIndex()).toInt()) {
            case SortTx::DATE_ASC:{
                columnIndex = TransactionTableModel::Date;
                order = Qt::AscendingOrder;
                break;
            }
            case SortTx::DATE_DESC:{
                columnIndex = TransactionTableModel::Date;
                break;
            }
            case SortTx::AMOUNT_ASC:{
                columnIndex = TransactionTableModel::Amount;
                order = Qt::AscendingOrder;
                break;
            }
            case SortTx::AMOUNT_DESC:{
                columnIndex = TransactionTableModel::Amount;
                break;
            }

        }
        txFilter->sort(columnIndex, order);

        // name, column, role
        writer.setModel(txFilter);
        writer.addColumn(tr("Confirmed"), 0, TransactionTableModel::ConfirmedRole);
        if (walletModel->haveWatchOnly())
            writer.addColumn(tr("Watch-only"), TransactionTableModel::Watchonly);
        writer.addColumn(tr("Date"), 0, TransactionTableModel::DateRole);
        writer.addColumn(tr("Type"), TransactionTableModel::Type, Qt::EditRole);
        writer.addColumn(tr("Label"), 0, TransactionTableModel::LabelRole);
        writer.addColumn(tr("Address"), 0, TransactionTableModel::AddressRole);
        writer.addColumn(BitcoinUnits::getAmountColumnTitle(walletModel->getOptionsModel()->getDisplayUnit()), 0, TransactionTableModel::FormattedAmountRole);
        writer.addColumn(tr("ID"), 0, TransactionTableModel::TxIDRole);

        fExport = writer.write();
    }

    if (fExport) {
        inform(tr("Exporting Successful\nThe transaction history was successfully saved to %1.").arg(filename));
    } else {
        inform(tr("Exporting Failed\nThere was an error trying to save the transaction history to %1.").arg(filename));
    }
}

SettingsExportCSV::~SettingsExportCSV()
{
    delete ui;
}
