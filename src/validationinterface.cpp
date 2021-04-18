// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validationinterface.h"

#include <boost/signals2/signal.hpp>

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
// XX42 g_signals.m_internals->EraseTransaction.connect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->UpdatedBlockTip.connect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->SyncTransaction.connect(boost::bind(&CValidationInterface::SyncTransaction, pwalletIn, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    g_signals.m_internals->NotifyTransactionLock.connect(boost::bind(&CValidationInterface::NotifyTransactionLock, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->UpdatedTransaction.connect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->SetBestChain.connect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->Broadcast.connect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->BlockChecked.connect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, boost::placeholders::_1, boost::placeholders::_2));
// XX42    g_signals.m_internals->ScriptForMining.connect(boost::bind(&CValidationInterface::GetScriptForMining, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->BlockFound.connect(boost::bind(&CValidationInterface::ResetRequestCount, pwalletIn, boost::placeholders::_1));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.m_internals->BlockFound.disconnect(boost::bind(&CValidationInterface::ResetRequestCount, pwalletIn, boost::placeholders::_1));
// XX42    g_signals.m_internals->ScriptForMining.disconnect(boost::bind(&CValidationInterface::GetScriptForMining, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->BlockChecked.disconnect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, boost::placeholders::_1, boost::placeholders::_2));
    g_signals.m_internals->Broadcast.disconnect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->SetBestChain.disconnect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->UpdatedTransaction.disconnect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->NotifyTransactionLock.disconnect(boost::bind(&CValidationInterface::NotifyTransactionLock, pwalletIn, boost::placeholders::_1));
    g_signals.m_internals->SyncTransaction.disconnect(boost::bind(&CValidationInterface::SyncTransaction, pwalletIn, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    g_signals.m_internals->UpdatedBlockTip.disconnect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, boost::placeholders::_1));
// XX42    g_signals.m_internals->EraseTransaction.disconnect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, boost::placeholders::_1));
}

void UnregisterAllValidationInterfaces() {
    g_signals.m_internals->BlockFound.disconnect_all_slots();
// XX42    g_signals.m_internals->ScriptForMining.disconnect_all_slots();
    g_signals.m_internals->BlockChecked.disconnect_all_slots();
    g_signals.m_internals->Broadcast.disconnect_all_slots();
    g_signals.m_internals->SetBestChain.disconnect_all_slots();
    g_signals.m_internals->UpdatedTransaction.disconnect_all_slots();
    g_signals.m_internals->NotifyTransactionLock.disconnect_all_slots();
    g_signals.m_internals->SyncTransaction.disconnect_all_slots();
    g_signals.m_internals->UpdatedBlockTip.disconnect_all_slots();
// XX42    g_signals.m_internals->EraseTransaction.disconnect_all_slots();
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
