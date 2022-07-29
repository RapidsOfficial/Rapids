// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_CLIENTMODEL_H
#define BITCOIN_QT_CLIENTMODEL_H

#include "uint256.h"
#include "chain.h"
#include <QObject>
#include <QDateTime>

#include <memory>

class AddressTableModel;
class BanTableModel;
class OptionsModel;
class PeerTableModel;
class TransactionTableModel;

namespace interfaces {
    class Handler;
}

QT_BEGIN_NAMESPACE
class QDateTime;
class QTimer;
QT_END_NAMESPACE

enum BlockSource {
    BLOCK_SOURCE_NONE,
    BLOCK_SOURCE_REINDEX,
    BLOCK_SOURCE_DISK,
    BLOCK_SOURCE_NETWORK
};

enum NumConnections {
    CONNECTIONS_NONE = 0,
    CONNECTIONS_IN = (1U << 0),
    CONNECTIONS_OUT = (1U << 1),
    CONNECTIONS_ALL = (CONNECTIONS_IN | CONNECTIONS_OUT),
};

/** Model for RPD network client. */
class ClientModel : public QObject
{
    Q_OBJECT

public:
    explicit ClientModel(OptionsModel* optionsModel, QObject* parent = 0);
    ~ClientModel();

    OptionsModel* getOptionsModel();
    PeerTableModel* getPeerTableModel();
    BanTableModel *getBanTableModel();

    //! Return number of connections, default is in- and outbound (total)
    int getNumConnections(unsigned int flags = CONNECTIONS_ALL) const;
    int getNumBlocksAtStartup();

    // from cached block index
    int getNumBlocks();
    QDateTime getLastBlockDate() const;
    QString getLastBlockHash() const;
    double getVerificationProgress() const;

    quint64 getTotalBytesRecv() const;
    quint64 getTotalBytesSent() const;

    //! Return true if core is doing initial block download
    bool inInitialBlockDownload() const;
    //! Return true if core is importing blocks
    enum BlockSource getBlockSource() const;
    //! Return warnings to be displayed in status bar
    QString getStatusBarWarnings() const;

    QString formatFullVersion() const;
    QString formatBuildDate() const;
    bool isReleaseVersion() const;
    QString clientName() const;
    QString formatClientStartupTime() const;
    QString dataDir() const;

    // Try to avoid Token queuing too many messages
    bool tryLockTokenStateChanged();
    bool tryLockTokenBalanceChanged();

    void setCacheTip(const CBlockIndex* const tip) { cacheTip = tip; }
    void setCacheReindexing(bool reindex) { cachedReindexing = reindex; }
    void setCacheImporting(bool import) { cachedImporting = import; }
    void setCacheInitialSync(bool _initialSync) { cachedInitialSync = _initialSync; }

    bool getTorInfo(std::string& ip_port) const;

    // Start/Stop the masternode polling timer
    void startMasternodesTimer();
    void stopMasternodesTimer();

private:
    // Listeners
    std::unique_ptr<interfaces::Handler> m_handler_show_progress;
    std::unique_ptr<interfaces::Handler> m_handler_notify_num_connections_changed;
    std::unique_ptr<interfaces::Handler> m_handler_notify_alert_changed;
    std::unique_ptr<interfaces::Handler> m_handler_banned_list_changed;
    std::unique_ptr<interfaces::Handler> m_handler_notify_block_tip;

    std::unique_ptr<interfaces::Handler> m_handler_token_state_changed;
    std::unique_ptr<interfaces::Handler> m_handler_token_pending_changed;
    std::unique_ptr<interfaces::Handler> m_handler_token_balance_changed;
    std::unique_ptr<interfaces::Handler> m_handler_token_state_invalidated;

    QString getMasternodeCountString() const;
    OptionsModel* optionsModel;
    PeerTableModel* peerTableModel;
    BanTableModel *banTableModel;

    const CBlockIndex* cacheTip{nullptr};
    QString cachedMasternodeCountString;
    bool cachedReindexing;
    bool cachedImporting;
    bool cachedInitialSync;

    int numBlocksAtStartup;

    QTimer* pollTimer;
    QTimer* pollMnTimer;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

    // Locks for Token state changes
    bool lockedTokenStateChanged;
    bool lockedTokenBalanceChanged;

Q_SIGNALS:
    void numConnectionsChanged(int count);
    void numBlocksChanged(int count);
    void strMasternodesChanged(const QString& strMasternodes);
    void alertsChanged(const QString& warnings);
    void bytesChanged(quint64 totalBytesIn, quint64 totalBytesOut);

    // Additional Token signals
    void reinitTokenState();
    void refreshTokenState();
    void refreshTokenBalance();
    void refreshTokenPending(bool pending);

    //! Fired when a message should be reported to the user
    void message(const QString& title, const QString& message, unsigned int style, bool* ret = nullptr);

    // Show progress dialog e.g. for verifychain
    void showProgress(const QString& title, int nProgress);

public Q_SLOTS:
    void updateTimer();
    void updateMnTimer();
    void updateNumConnections(int numConnections);
    void updateAlert();
    void updateBanlist();

    // Additional Token slots
    void invalidateTokenState();
    void updateTokenState();
    void updateTokenBalance();
    void updateTokenPending(bool pending);
};

#endif // BITCOIN_QT_CLIENTMODEL_H
