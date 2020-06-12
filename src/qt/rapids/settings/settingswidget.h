// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2018-2020 The Rapids developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QWidget>
#include "qt/rapids/pwidget.h"
#include "qt/rapids/settings/settingsbackupwallet.h"
#include "qt/rapids/settings/settingsexportcsv.h"
#include "qt/rapids/settings/settingsbittoolwidget.h"
#include "qt/rapids/settings/settingssignmessagewidgets.h"
#include "qt/rapids/settings/settingswalletrepairwidget.h"
#include "qt/rapids/settings/settingswalletoptionswidget.h"
#include "qt/rapids/settings/settingsmainoptionswidget.h"
#include "qt/rapids/settings/settingsdisplayoptionswidget.h"
#include "qt/rapids/settings/settingsmultisendwidget.h"
#include "qt/rapids/settings/settingsinformationwidget.h"
#include "qt/rapids/settings/settingsconsolewidget.h"

class RapidsGUI;

QT_BEGIN_NAMESPACE
class QDataWidgetMapper;
QT_END_NAMESPACE

namespace Ui {
class SettingsWidget;
}

class SettingsWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsWidget(RapidsGUI* parent);
    ~SettingsWidget();

    void loadClientModel() override;
    void loadWalletModel() override;
    void setMapper();
    void showDebugConsole();

Q_SIGNALS:
    /** Get restart command-line parameters and handle restart */
    void handleRestart(QStringList args);

private Q_SLOTS:
    // File
    void onFileClicked();
    void onBackupWalletClicked();
    void onSignMessageClicked();

    // Wallet Configuration
    void onConfigurationClicked();
    void onBipToolClicked();
    void onMultisendClicked();
    void onExportCSVClicked();

    // Options
    void onOptionsClicked();
    void onMainOptionsClicked();
    void onWalletOptionsClicked();
    void onDisplayOptionsClicked();

    void onDiscardChanges();

    // Tools
    void onToolsClicked();
    void onInformationClicked();
    void onDebugConsoleClicked();
    void onWalletRepairClicked();

    // Help
    void onHelpClicked();
    void onAboutClicked();

    void onResetAction();
    void onSaveOptionsClicked();

private:
    Ui::SettingsWidget *ui;
    int navAreaBaseHeight{0};

    SettingsBackupWallet *settingsBackupWallet;
    SettingsExportCSV *settingsExportCsvWidget;
    SettingsBitToolWidget *settingsBitToolWidget;
    SettingsSignMessageWidgets *settingsSingMessageWidgets;
    SettingsWalletRepairWidget *settingsWalletRepairWidget;
    SettingsWalletOptionsWidget *settingsWalletOptionsWidget;
    SettingsMainOptionsWidget *settingsMainOptionsWidget;
    SettingsDisplayOptionsWidget *settingsDisplayOptionsWidget;
    SettingsMultisendWidget *settingsMultisendWidget;
    SettingsInformationWidget *settingsInformationWidget;
    SettingsConsoleWidget *settingsConsoleWidget;

    QDataWidgetMapper* mapper;

    QList<QPushButton*> options;
    // Map of: menu button -> sub menu items
    QMap <QPushButton*, QWidget*> menus;

    void selectOption(QPushButton* option);
    bool openStandardDialog(const QString& title = "", const QString& body = "", const QString& okBtn = "OK", const QString& cancelBtn = "");
    void selectMenu(QPushButton* btn);
};

#endif // SETTINGSWIDGET_H
