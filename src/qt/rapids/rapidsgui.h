// Copyright (c) 2018-2020 The Rapids developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Rapids_CORE_NEW_GUI_RapidsGUI_H
#define Rapids_CORE_NEW_GUI_RapidsGUI_H

#if defined(HAVE_CONFIG_H)
#include "config/pivx-config.h"
#endif

#include <QMainWindow>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QLabel>

#include "qt/rapids/navmenuwidget.h"
#include "qt/rapids/topbar.h"
#include "qt/rapids/dashboardwidget.h"
#include "qt/rapids/send.h"
#include "qt/rapids/receivewidget.h"
#include "qt/rapids/addresseswidget.h"
#include "qt/rapids/coldstakingwidget.h"
#include "qt/rapids/masternodeswidget.h"
#include "qt/rapids/snackbar.h"
#include "qt/rapids/settings/settingswidget.h"
#include "qt/rapids/settings/settingsfaqwidget.h"
#include "qt/txhistorydialog.h"
#include "qt/sendmpdialog.h"
#include "qt/creatempdialog.h"
#include "qt/balancesdialog.h"
#include "qt/usernamesdialog.h"
#include "qt/metadexdialog.h"
#include "qt/rpcconsole.h"

namespace interfaces {
    class Handler;
}

class ClientModel;
class NetworkStyle;
class Notificator;
class WalletModel;


/**
  Rapids GUI main class. This class represents the main window of the Rapids UI. It communicates with both the client and
  wallet models to give the user an up-to-date view of the current core state.
*/
class RapidsGUI : public QMainWindow
{
    Q_OBJECT

public:
    static const QString DEFAULT_WALLET;

    explicit RapidsGUI(const NetworkStyle* networkStyle, QWidget* parent = 0);
    ~RapidsGUI();

    /** Set the client model.
        The client model represents the part of the core that communicates with the P2P network, and is wallet-agnostic.
    */
    void setClientModel(ClientModel* clientModel);


    void resizeEvent(QResizeEvent *event) override;
    void showHide(bool show);
    int getNavWidth();
Q_SIGNALS:
    void themeChanged(bool isLightTheme, QString& theme);
    void windowResizeEvent(QResizeEvent* event);
public Q_SLOTS:
    void changeTheme(bool isLightTheme);
    void goToDashboard();
    void goToSend();
    void goToReceive();
    void goToAddresses();
    void goToMasterNodes();
    void goToColdStaking();
    void goToSettings();
    void gotoTokensPage();
    void goToSettingsInfo();
    void openNetworkMonitor();

    void connectActions();

    /** Get restart command-line parameters and request restart */
    void handleRestart(QStringList args);

    /** Notify the user of an event from the core network or transaction handling code.
       @param[in] title     the message box / notification title
       @param[in] message   the displayed text
       @param[in] style     modality and style definitions (icon and used buttons - buttons only for message boxes)
                            @see CClientUIInterface::MessageBoxFlags
       @param[in] ret       pointer to a bool that will be modified to whether Ok was clicked (modal only)
    */
    void message(const QString& title, const QString& message, unsigned int style, bool* ret = nullptr);
    void messageInfo(const QString& message);
    bool execDialog(QDialog *dialog, int xDiv = 3, int yDiv = 5);
    /** Open FAQ dialog **/
    void openFAQ(SettingsFaqWidget::Section section = SettingsFaqWidget::Section::INTRO);

    /** Show incoming transaction notification for new transactions. */
    void incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address);
#ifdef ENABLE_WALLET
    /** Set the wallet model.
        The wallet model represents a bitcoin wallet, and offers access to the list of transactions, address book and sending
        functionality.
    */
    bool addWallet(const QString& name, WalletModel* walletModel);
    bool setCurrentWallet(const QString& name);
    void removeAllWallets();
#endif // ENABLE_WALLET

protected:

    void changeEvent(QEvent* e) override;
    void closeEvent(QCloseEvent* event) override;

    /*
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);
    bool eventFilter(QObject* object, QEvent* event);
     */

private:
    // Handlers
    std::unique_ptr<interfaces::Handler> m_handler_message_box;

    bool enableWallet;
    ClientModel* clientModel = nullptr;

    // Actions
    QAction* quitAction = nullptr;
    QAction* toggleHideAction = nullptr;

    // Frame
    NavMenuWidget *navMenu = nullptr;
    TopBar *topBar = nullptr;
    QStackedWidget *stackedContainer = nullptr;

    DashboardWidget *dashboard = nullptr;
    SendWidget *sendWidget = nullptr;
    ReceiveWidget *receiveWidget = nullptr;
    AddressesWidget *addressesWidget = nullptr;
    MasterNodesWidget *masterNodesWidget = nullptr;
    ColdStakingWidget *coldStakingWidget = nullptr;
    SettingsWidget* settingsWidget = nullptr;

    BalancesDialog *balancesPage = nullptr;
    UsernamesDialog *usernamesPage = nullptr;
    TXHistoryDialog *tokensHistory = nullptr;
    CreateMPDialog *createTokenPage = nullptr;
    SendMPDialog *sendTokenPage = nullptr;
    QTabWidget *tabHolder = nullptr;
    // MetaDExDialog *dexPage = nullptr;

    SnackBar *snackBar = nullptr;

    RPCConsole* rpcConsole = nullptr;

    QSystemTrayIcon* trayIcon = nullptr;
    QMenu* trayIconMenu = nullptr;
    Notificator* notificator = nullptr;

    QLabel *op = nullptr;
    bool opEnabled = false;

    /** Create the main UI actions. */
    void createActions(const NetworkStyle* networkStyle);
    /** Create system tray icon and notification */
    void createTrayIcon(const NetworkStyle* networkStyle);
    /** Create system tray menu (or setup the dock menu) */
    void createTrayIconMenu();

    void showTop(QWidget *view);
    bool openStandardDialog(QString title = "", QString body = "", QString okBtn = "OK", QString cancelBtn = "CANCEL");

    /** Connect core signals to GUI client */
    void subscribeToCoreSignals();
    /** Disconnect core signals from GUI client */
    void unsubscribeFromCoreSignals();

public Q_SLOTS:
    /** called by a timer to check if fRequestShutdown has been set **/
    void detectShutdown();

private Q_SLOTS:
    /** Show window if hidden, unminimize when minimized, rise when obscured or show if hidden and fToggleHidden is true */
    void showNormalIfMinimized(bool fToggleHidden = false);

    /** Simply calls showNormalIfMinimized(true) for use in SLOT() macro */
    void toggleHidden();

#ifndef Q_OS_MAC
    /** Handle tray icon clicked */
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
#else
    /** Handle macOS Dock icon clicked */
     void macosDockIconActivated();
#endif

Q_SIGNALS:
    /** Signal raised when a URI was entered or dragged to the GUI */
    void receivedURI(const QString& uri);
    /** Restart handling */
    void requestedRestart(QStringList args);

};


#endif //Rapids_CORE_NEW_GUI_RapidsGUI_H
