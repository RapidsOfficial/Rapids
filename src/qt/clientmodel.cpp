// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "clientmodel.h"

#include "bantablemodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "peertablemodel.h"

#include "chainparams.h"
#include "checkpoints.h"
#include "clientversion.h"
#include "main.h"
#include "interfaces/handler.h"
#include "masternodeman.h"
#include "net.h"
#include "netbase.h"
#include "guiinterface.h"
#include "util.h"

#include <stdint.h>

#include <QDateTime>
#include <QDebug>
#include <QTimer>

static const int64_t nClientStartupTime = GetTime();
// Last tip update notification
static int64_t nLastBlockTipUpdateNotification = 0;

ClientModel::ClientModel(OptionsModel* optionsModel, QObject* parent) : QObject(parent),
                                                                        optionsModel(optionsModel),
                                                                        peerTableModel(0),
                                                                        banTableModel(0),
                                                                        cacheTip(nullptr),
                                                                        cachedMasternodeCountString(""),
                                                                        cachedReindexing(0), cachedImporting(0),
                                                                        numBlocksAtStartup(-1), pollTimer(0),
                                                                        lockedTokenStateChanged(false),
                                                                        lockedTokenBalanceChanged(false)
{
    peerTableModel = new PeerTableModel(this);
    banTableModel = new BanTableModel(this);
    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, &ClientModel::updateTimer);
    pollTimer->start(MODEL_UPDATE_DELAY);

    pollMnTimer = new QTimer(this);
    connect(pollMnTimer, &QTimer::timeout, this, &ClientModel::updateMnTimer);

    subscribeToCoreSignals();
}

ClientModel::~ClientModel()
{
    unsubscribeFromCoreSignals();
}

int ClientModel::getNumConnections(unsigned int flags) const
{
    CConnman::NumConnections connections = CConnman::CONNECTIONS_NONE;

    if(flags == CONNECTIONS_IN)
        connections = CConnman::CONNECTIONS_IN;
    else if (flags == CONNECTIONS_OUT)
        connections = CConnman::CONNECTIONS_OUT;
    else if (flags == CONNECTIONS_ALL)
        connections = CConnman::CONNECTIONS_ALL;

    if (g_connman)
         return g_connman->GetNodeCount(connections);
    return 0;
}

QString ClientModel::getMasternodeCountString() const
{
    int ipv4 = 0, ipv6 = 0, onion = 0;
    mnodeman.CountNetworks(ActiveProtocol(), ipv4, ipv6, onion);
    int nUnknown = mnodeman.size() - ipv4 - ipv6 - onion;
    if(nUnknown < 0) nUnknown = 0;
    return tr("Total: %1 (IPv4: %2 / IPv6: %3 / Tor: %4 / Unknown: %5)").arg(QString::number((int)mnodeman.size())).arg(QString::number((int)ipv4)).arg(QString::number((int)ipv6)).arg(QString::number((int)onion)).arg(QString::number((int)nUnknown));
}

int ClientModel::getNumBlocks()
{
    if (!cacheTip) {
        cacheTip = WITH_LOCK(cs_main, return chainActive.Tip(););
    }

    return cacheTip ? cacheTip->nHeight : 0;
}

int ClientModel::getNumBlocksAtStartup()
{
    if (numBlocksAtStartup == -1) numBlocksAtStartup = getNumBlocks();
    return numBlocksAtStartup;
}

quint64 ClientModel::getTotalBytesRecv() const
{
    if(!g_connman)
        return 0;
    return g_connman->GetTotalBytesRecv();
}

quint64 ClientModel::getTotalBytesSent() const
{
    if(!g_connman)
        return 0;
    return g_connman->GetTotalBytesSent();
}

QDateTime ClientModel::getLastBlockDate() const
{
    const int nTime = (cacheTip == nullptr ? Params().GenesisBlock().GetBlockTime() : cacheTip->GetBlockTime());
    return QDateTime::fromTime_t(nTime);
}

QString ClientModel::getLastBlockHash() const
{
    const uint256& nHash = (cacheTip == nullptr ? Params().GenesisBlock().GetHash() : cacheTip->GetBlockHash());
    return QString::fromStdString(nHash.GetHex());
}

double ClientModel::getVerificationProgress() const
{
    return Checkpoints::GuessVerificationProgress(cacheTip);
}

void ClientModel::updateTimer()
{
    // Get required lock upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    Q_EMIT bytesChanged(getTotalBytesRecv(), getTotalBytesSent());
}

void ClientModel::updateMnTimer()
{
    // Get required lock upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if (!lockMain)
        return;
    QString newMasternodeCountString = getMasternodeCountString();

    if (cachedMasternodeCountString != newMasternodeCountString) {
        cachedMasternodeCountString = newMasternodeCountString;

        Q_EMIT strMasternodesChanged(cachedMasternodeCountString);
    }
}

void ClientModel::startMasternodesTimer()
{
    if (!pollMnTimer->isActive()) {
        // no need to update as frequent as data for balances/txes/blocks
        pollMnTimer->start(MODEL_UPDATE_DELAY * 40);
    }
}

void ClientModel::stopMasternodesTimer()
{
    if (pollMnTimer->isActive()) {
        pollMnTimer->stop();
    }
}

void ClientModel::updateNumConnections(int numConnections)
{
    Q_EMIT numConnectionsChanged(numConnections);
}

void ClientModel::invalidateTokenState()
{
    Q_EMIT reinitTokenState();
}

void ClientModel::updateTokenState()
{
    lockedTokenStateChanged = false;
    Q_EMIT refreshTokenState();
}

bool ClientModel::tryLockTokenStateChanged()
{
    // Try to avoid Token queuing too many messages for the UI
    if (lockedTokenStateChanged) {
        return false;
    }

    lockedTokenStateChanged = true;
    return true;
}

void ClientModel::updateTokenBalance()
{
    lockedTokenBalanceChanged = false;
    Q_EMIT refreshTokenBalance();
}

bool ClientModel::tryLockTokenBalanceChanged()
{
    // Try to avoid Token queuing too many messages for the UI
    if (lockedTokenBalanceChanged) {
        return false;
    }

    lockedTokenBalanceChanged = true;
    return true;
}

void ClientModel::updateTokenPending(bool pending)
{
    Q_EMIT refreshTokenPending(pending);
}

void ClientModel::updateAlert()
{
    Q_EMIT alertsChanged(getStatusBarWarnings());
}

bool ClientModel::inInitialBlockDownload() const
{
    return cachedInitialSync;
}

enum BlockSource ClientModel::getBlockSource() const
{
    if (fReindex)
        return BLOCK_SOURCE_REINDEX;
    else if (fImporting)
        return BLOCK_SOURCE_DISK;
    else if (getNumConnections() > 0)
        return BLOCK_SOURCE_NETWORK;

    return BLOCK_SOURCE_NONE;
}

QString ClientModel::getStatusBarWarnings() const
{
    return QString::fromStdString(GetWarnings("statusbar"));
}

OptionsModel* ClientModel::getOptionsModel()
{
    return optionsModel;
}

PeerTableModel* ClientModel::getPeerTableModel()
{
    return peerTableModel;
}

BanTableModel *ClientModel::getBanTableModel()
{
    return banTableModel;
}

QString ClientModel::formatFullVersion() const
{
    return QString::fromStdString(FormatFullVersion());
}

QString ClientModel::formatBuildDate() const
{
    return QString::fromStdString(CLIENT_DATE);
}

bool ClientModel::isReleaseVersion() const
{
    return CLIENT_VERSION_IS_RELEASE;
}

QString ClientModel::clientName() const
{
    return QString::fromStdString(CLIENT_NAME);
}

QString ClientModel::formatClientStartupTime() const
{
    return QDateTime::fromTime_t(nClientStartupTime).toString();
}

QString ClientModel::dataDir() const
{
    return GUIUtil::boostPathToQString(GetDataDir());
}

void ClientModel::updateBanlist()
{
    banTableModel->refresh();
}

static void BlockTipChanged(ClientModel *clientmodel, bool initialSync, const CBlockIndex *pIndex)
{
    // lock free async UI updates in case we have a new block tip
    // during initial sync, only update the UI if the last update
    // was > 1000ms (MODEL_UPDATE_DELAY) ago
    int64_t now = 0;
    if (initialSync)
        now = GetTimeMillis();

    // if we are in-sync, update the UI regardless of last update time
    if (!initialSync || now - nLastBlockTipUpdateNotification > MODEL_UPDATE_DELAY) {
        //pass a async signal to the UI thread
        clientmodel->setCacheTip(pIndex);
        clientmodel->setCacheImporting(fImporting);
        clientmodel->setCacheReindexing(fReindex);
        clientmodel->setCacheInitialSync(initialSync);
        Q_EMIT clientmodel->numBlocksChanged(pIndex->nHeight);
        nLastBlockTipUpdateNotification = now;
    }
}

// Handlers for core signals
static void TokenStateInvalidated(ClientModel *clientmodel)
{
    // This will be triggered if a reorg invalidates the state
    QMetaObject::invokeMethod(clientmodel, "invalidateTokenState", Qt::QueuedConnection);
}

static void TokenStateChanged(ClientModel *clientmodel)
{
    // This will be triggered for each block that contains Token layer transactions
    if (clientmodel->tryLockTokenStateChanged()) {
        QMetaObject::invokeMethod(clientmodel, "updateTokenState", Qt::QueuedConnection);
    }
}

static void TokenBalanceChanged(ClientModel *clientmodel)
{
    // Triggered when a balance for a wallet address changes
    if (clientmodel->tryLockTokenBalanceChanged()) {
        QMetaObject::invokeMethod(clientmodel, "updateTokenBalance", Qt::QueuedConnection);
    }
}

static void TokenPendingChanged(ClientModel *clientmodel, bool pending)
{
    // Triggered when Token pending map adds/removes transactions
    QMetaObject::invokeMethod(clientmodel, "updateTokenPending", Qt::QueuedConnection, Q_ARG(bool, pending));
}

static void ShowProgress(ClientModel* clientmodel, const std::string& title, int nProgress)
{
    // emits signal "showProgress"
    QMetaObject::invokeMethod(clientmodel, "showProgress", Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(title)),
        Q_ARG(int, nProgress));
}

static void NotifyNumConnectionsChanged(ClientModel* clientmodel, int newNumConnections)
{
    // Too noisy: qDebug() << "NotifyNumConnectionsChanged : " + QString::number(newNumConnections);
    QMetaObject::invokeMethod(clientmodel, "updateNumConnections", Qt::QueuedConnection,
        Q_ARG(int, newNumConnections));
}

static void NotifyAlertChanged(ClientModel* clientmodel)
{
    qDebug() << "NotifyAlertChanged";
    QMetaObject::invokeMethod(clientmodel, "updateAlert", Qt::QueuedConnection);
}

static void BannedListChanged(ClientModel *clientmodel)
{
    qDebug() << QString("%1: Requesting update for peer banlist").arg(__func__);
    QMetaObject::invokeMethod(clientmodel, "updateBanlist", Qt::QueuedConnection);
}

void ClientModel::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_show_progress = interfaces::MakeHandler(uiInterface.ShowProgress.connect(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2)));
    m_handler_notify_num_connections_changed = interfaces::MakeHandler(uiInterface.NotifyNumConnectionsChanged.connect(std::bind(NotifyNumConnectionsChanged, this, std::placeholders::_1)));
    m_handler_notify_alert_changed = interfaces::MakeHandler(uiInterface.NotifyAlertChanged.connect(std::bind(NotifyAlertChanged, this)));
    m_handler_banned_list_changed = interfaces::MakeHandler(uiInterface.BannedListChanged.connect(std::bind(BannedListChanged, this)));
    m_handler_notify_block_tip = interfaces::MakeHandler(uiInterface.NotifyBlockTip.connect(std::bind(BlockTipChanged, this, std::placeholders::_1, std::placeholders::_2)));
    
    // Connect Token signals
    m_handler_token_state_changed = interfaces::MakeHandler(uiInterface.TokenStateChanged.connect(std::bind(TokenStateChanged, this)));
    m_handler_token_pending_changed = interfaces::MakeHandler(uiInterface.TokenPendingChanged.connect(std::bind(TokenPendingChanged, this, std::placeholders::_1)));
    m_handler_token_balance_changed = interfaces::MakeHandler(uiInterface.TokenBalanceChanged.connect(std::bind(TokenBalanceChanged, this)));
    m_handler_token_state_invalidated = interfaces::MakeHandler(uiInterface.TokenStateInvalidated.connect(std::bind(TokenStateInvalidated, this)));
}

void ClientModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_show_progress->disconnect();
    m_handler_notify_num_connections_changed->disconnect();
    m_handler_notify_alert_changed->disconnect();
    m_handler_banned_list_changed->disconnect();
    m_handler_notify_block_tip->disconnect();

    // Disconnect Token signals
    m_handler_token_state_changed->disconnect();
    m_handler_token_pending_changed->disconnect();
    m_handler_token_balance_changed->disconnect();
    m_handler_token_state_invalidated->disconnect();
}

bool ClientModel::getTorInfo(std::string& ip_port) const
{
    proxyType onion;
    if (GetProxy((Network) 3, onion) && IsReachable((Network) 3)) {
        {
            LOCK(cs_mapLocalHost);
            for (const std::pair<const CNetAddr, LocalServiceInfo>& item : mapLocalHost) {
                if (item.first.IsTor()) {
                    CService addrOnion(LookupNumeric(item.first.ToString().c_str(), item.second.nPort));
                    ip_port = addrOnion.ToStringIPPort();
                    return true;
                }
            }
        }
    }
    return false;
}
