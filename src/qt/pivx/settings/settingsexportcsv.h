// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SETTINGSEXPORTCSV_H
#define SETTINGSEXPORTCSV_H

#include <QWidget>
#include "qt/pivx/pwidget.h"

namespace Ui {
class SettingsExportCSV;
}

class SettingsExportCSV : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsExportCSV(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsExportCSV();

private Q_SLOTS:
    void selectFileOutput();
    void exportClicked();

private:
    Ui::SettingsExportCSV *ui;
    QString filename;
};

#endif // SETTINGSEXPORTCSV_H
