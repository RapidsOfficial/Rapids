// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validationinterface.h"

#include <unordered_map>
#include <boost/signals2/signal.hpp>

struct ValidationInterfaceConnections {
    boost::signals2::scoped_connection UpdatedBlockTip;
    boost::signals2::scoped_connection SyncTransaction;
    boost::signals2::scoped_connection NotifyTransactionLock;
    boost::signals2::scoped_connection UpdatedTransaction;
    boost::signals2::scoped_connection SetBestChain;
    boost::signals2::scoped_connection Broadcast;
    boost::signals2::scoped_connection BlockChecked;
    boost::signals2::scoped_connection BlockFound;
    boost::signals2::scoped_connection ChainTip;
};

struct MainSignalsInstance {
// XX42    boost::signals2::signal<void(const uint256&)> EraseTransaction;
    /** Notifies listeners of updated block chain tip */
    boost::signals2::signal<void (const CBlockIndex *)> UpdatedBlockTip;
    /** A posInBlock value for SyncTransaction which indicates the transaction was conflicted, disconnected, or not in a block */
    static const int SYNC_TRANSACTION_NOT_IN_BLOCK = -1;
    /** Notifies listeners of updated transaction data (transaction, and optionally the block it is found in. */
    boost::signals2::signal<void (const CTransaction &, const CBlockIndex *pindex, int posInBlock)> SyncTransaction;
    /** Notifies listeners of an updated transaction lock without new data. */
    boost::signals2::signal<void (const CTransaction &)> NotifyTransactionLock;
    /** Notifies listeners of an updated transaction without new data (for now: a coinbase potentially becoming visible). */
    boost::signals2::signal<bool (const uint256 &)> UpdatedTransaction;
    /** Notifies listeners of a new active block chain. */
    boost::signals2::signal<void (const CBlockLocator &)> SetBestChain;
    /** Tells listeners to broadcast their data. */
    boost::signals2::signal<void (CConnman* connman)> Broadcast;
    /** Notifies listeners of a block validation result */
    boost::signals2::signal<void (const CBlock&, const CValidationState&)> BlockChecked;
    /** Notifies listeners that a block has been successfully mined */
    boost::signals2::signal<void (const uint256 &)> BlockFound;

    std::unordered_map<CValidationInterface*, ValidationInterfaceConnections> m_connMainSignals;
};

static CMainSignals g_signals;

CMainSignals::CMainSignals() {
    m_internals.reset(new MainSignalsInstance());
}

CMainSignals& GetMainSignals()
{
    return g_signals;
}

void RegisterValidationInterface(CValidationInterface* pwalletIn) {
    ValidationInterfaceConnections& conns = g_signals.m_internals->m_connMainSignals[pwalletIn];
    conns.UpdatedBlockTip = g_signals.m_internals->UpdatedBlockTip.connect(std::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, std::placeholders::_1));
    conns.SyncTransaction = g_signals.m_internals->SyncTransaction.connect(std::bind(&CValidationInterface::SyncTransaction, pwalletIn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    conns.NotifyTransactionLock = g_signals.m_internals->NotifyTransactionLock.connect(std::bind(&CValidationInterface::NotifyTransactionLock, pwalletIn, std::placeholders::_1));
    conns.UpdatedTransaction = g_signals.m_internals->UpdatedTransaction.connect(std::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, std::placeholders::_1));
    conns.SetBestChain = g_signals.m_internals->SetBestChain.connect(std::bind(&CValidationInterface::SetBestChain, pwalletIn, std::placeholders::_1));
    conns.Broadcast = g_signals.m_internals->Broadcast.connect(std::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, std::placeholders::_1));
    conns.BlockChecked = g_signals.m_internals->BlockChecked.connect(std::bind(&CValidationInterface::BlockChecked, pwalletIn, std::placeholders::_1, std::placeholders::_2));
    conns.BlockFound = g_signals.m_internals->BlockFound.connect(std::bind(&CValidationInterface::ResetRequestCount, pwalletIn, std::placeholders::_1));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn) {
    if (g_signals.m_internals) {
        g_signals.m_internals->m_connMainSignals.erase(pwalletIn);
    }
}

void UnregisterAllValidationInterfaces() {
    if (!g_signals.m_internals) {
        return;
    }
    g_signals.m_internals->m_connMainSignals.clear();
}

void CMainSignals::UpdatedBlockTip(const CBlockIndex* pindex) {
    m_internals->UpdatedBlockTip(pindex);
}

void CMainSignals::SyncTransaction(const CTransaction& tx, const CBlockIndex* pindex, int posInBlock) {
    m_internals->SyncTransaction(tx, pindex, posInBlock);
}

void CMainSignals::NotifyTransactionLock(const CTransaction& tx) {
    m_internals->NotifyTransactionLock(tx);
}

void CMainSignals::UpdatedTransaction(const uint256& hash) {
    m_internals->UpdatedTransaction(hash);
}

void CMainSignals::SetBestChain(const CBlockLocator& locator) {
    m_internals->SetBestChain(locator);
}

void CMainSignals::Broadcast(CConnman* connman) {
    m_internals->Broadcast(connman);
}

void CMainSignals::BlockChecked(const CBlock& block, const CValidationState& state) {
    m_internals->BlockChecked(block, state);
}

void CMainSignals::BlockFound(const uint256& hash) {
    m_internals->BlockFound(hash);
}
