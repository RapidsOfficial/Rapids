// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemasternode.h"

#include "addrman.h"
#include "masternode-sync.h"
#include "masternode.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "messagesigner.h"
#include "netbase.h"
#include "protocol.h"

//
// Bootup the Masternode, look for a 10000 PIVX input and register on the network
//
void CActiveMasternode::ManageStatus()
{
    std::string errorMessage;

    if (!fMasterNode) return;

    LogPrint(BCLog::MASTERNODE, "CActiveMasternode::ManageStatus() - Begin\n");

    //need correct blocks to send ping
    if (!Params().IsRegTestNet() && !masternodeSync.IsBlockchainSynced()) {
        status = ACTIVE_MASTERNODE_SYNC_IN_PROCESS;
        LogPrintf("CActiveMasternode::ManageStatus() - %s\n", GetStatus());
        return;
    }

    if (status == ACTIVE_MASTERNODE_SYNC_IN_PROCESS) status = ACTIVE_MASTERNODE_INITIAL;

    if (status == ACTIVE_MASTERNODE_INITIAL) {
        CMasternode* pmn;
        pmn = mnodeman.Find(pubKeyMasternode);
        if (pmn != NULL) {
            pmn->Check();
            if (pmn->IsEnabled() && pmn->protocolVersion == PROTOCOL_VERSION) EnableHotColdMasterNode(pmn->vin, pmn->addr);
        }
    }

    if (status != ACTIVE_MASTERNODE_STARTED) {
        // Set defaults
        status = ACTIVE_MASTERNODE_NOT_CAPABLE;
        notCapableReason = "";

        if (pwalletMain->IsLocked()) {
            notCapableReason = "Wallet is locked.";
            LogPrintf("CActiveMasternode::ManageStatus() - not capable: %s\n", notCapableReason);
            return;
        }

        if (pwalletMain->GetBalance() == 0) {
            notCapableReason = "Hot node, waiting for remote activation.";
            LogPrintf("CActiveMasternode::ManageStatus() - not capable: %s\n", notCapableReason);
            return;
        }

        if (strMasterNodeAddr.empty()) {
            if (!GetLocal(service)) {
                notCapableReason = "Can't detect external address. Please use the masternodeaddr configuration option.";
                LogPrintf("CActiveMasternode::ManageStatus() - not capable: %s\n", notCapableReason);
                return;
            }
        } else {
            int nPort;
            std::string strHost;
            SplitHostPort(strMasterNodeAddr, nPort, strHost);
            service = LookupNumeric(strHost.c_str(), nPort);
        }

        // The service needs the correct default port to work properly
        if (!CMasternodeBroadcast::CheckDefaultPort(service, errorMessage, "CActiveMasternode::ManageStatus()"))
            return;

        LogPrintf("CActiveMasternode::ManageStatus() - Checking inbound connection to '%s'\n", service.ToString());

        CAddress addr(service, NODE_NETWORK);
        if (!OpenNetworkConnection(addr, true, nullptr)) {
            notCapableReason = "Could not connect to " + service.ToString();
            LogPrintf("CActiveMasternode::ManageStatus() - not capable: %s\n", notCapableReason);
            return;
        }
    }

    //send to all peers
    if (!SendMasternodePing(errorMessage)) {
        LogPrintf("CActiveMasternode::ManageStatus() - Error on Ping: %s\n", errorMessage);
    }
}

std::string CActiveMasternode::GetStatus()
{
    switch (status) {
    case ACTIVE_MASTERNODE_INITIAL:
        return "Node just started, not yet activated";
    case ACTIVE_MASTERNODE_SYNC_IN_PROCESS:
        return "Sync in progress. Must wait until sync is complete to start Masternode";
    case ACTIVE_MASTERNODE_INPUT_TOO_NEW:
        return strprintf("Masternode input must have at least %d confirmations", MASTERNODE_MIN_CONFIRMATIONS);
    case ACTIVE_MASTERNODE_NOT_CAPABLE:
        return "Not capable masternode: " + notCapableReason;
    case ACTIVE_MASTERNODE_STARTED:
        return "Masternode successfully started";
    default:
        return "unknown";
    }
}

bool CActiveMasternode::SendMasternodePing(std::string& errorMessage)
{
    if (status != ACTIVE_MASTERNODE_STARTED) {
        errorMessage = "Masternode is not in a running status";
        return false;
    }

    CPubKey pubKeyMasternode;
    CKey keyMasternode;

    if (!CMessageSigner::GetKeysFromSecret(strMasterNodePrivKey, keyMasternode, pubKeyMasternode)) {
        errorMessage = "Error upon calling GetKeysFromSecret.\n";
        return false;
    }

    LogPrintf("CActiveMasternode::SendMasternodePing() - Relay Masternode Ping vin = %s\n", vin.ToString());

    CMasternodePing mnp(vin);
    if (!mnp.Sign(keyMasternode, pubKeyMasternode)) {
        errorMessage = "Couldn't sign Masternode Ping";
        return false;
    }

    // Update lastPing for our masternode in Masternode list
    CMasternode* pmn = mnodeman.Find(vin);
    if (pmn != NULL) {
        if (pmn->IsPingedWithin(MASTERNODE_PING_SECONDS, mnp.sigTime)) {
            errorMessage = "Too early to send Masternode Ping";
            return false;
        }

        pmn->lastPing = mnp;
        mnodeman.mapSeenMasternodePing.insert(std::make_pair(mnp.GetHash(), mnp));

        //mnodeman.mapSeenMasternodeBroadcast.lastPing is probably outdated, so we'll update it
        CMasternodeBroadcast mnb(*pmn);
        uint256 hash = mnb.GetHash();
        if (mnodeman.mapSeenMasternodeBroadcast.count(hash)) mnodeman.mapSeenMasternodeBroadcast[hash].lastPing = mnp;

        mnp.Relay();
        return true;

    } else {
        // Seems like we are trying to send a ping while the Masternode is not registered in the network
        errorMessage = "Masternode List doesn't include our Masternode, shutting down Masternode pinging service! " + vin.ToString();
        status = ACTIVE_MASTERNODE_NOT_CAPABLE;
        notCapableReason = errorMessage;
        return false;
    }
}

bool CActiveMasternode::GetMasterNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey)
{
    return GetMasterNodeVin(vin, pubkey, secretKey, "", "");
}

bool CActiveMasternode::GetMasterNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    // Find possible candidates
    TRY_LOCK(pwalletMain->cs_wallet, fWallet);
    if (!fWallet) return false;

    std::vector<COutput> possibleCoins = SelectCoinsMasternode();
    COutput* selectedOutput;

    // Find the vin
    if (!strTxHash.empty()) {
        // Let's find it
        uint256 txHash(uint256S(strTxHash));
        int outputIndex;
        try {
            outputIndex = std::stoi(strOutputIndex.c_str());
        } catch (const std::exception& e) {
            LogPrintf("%s: %s on strOutputIndex\n", __func__, e.what());
            return false;
        }

        bool found = false;
        for (COutput& out : possibleCoins) {
            if (out.tx->GetHash() == txHash && out.i == outputIndex) {
                selectedOutput = &out;
                found = true;
                break;
            }
        }
        if (!found) {
            LogPrintf("CActiveMasternode::GetMasterNodeVin - Could not locate valid vin\n");
            return false;
        }
    } else {
        // No output specified,  Select the first one
        if (possibleCoins.size() > 0) {
            selectedOutput = &possibleCoins[0];
        } else {
            LogPrintf("CActiveMasternode::GetMasterNodeVin - Could not locate specified vin from possible list\n");
            return false;
        }
    }

    // At this point we have a selected output, retrieve the associated info
    return GetVinFromOutput(*selectedOutput, vin, pubkey, secretKey);
}


// Extract Masternode vin information from output
bool CActiveMasternode::GetVinFromOutput(COutput out, CTxIn& vin, CPubKey& pubkey, CKey& secretKey)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    CScript pubScript;

    vin = CTxIn(out.tx->GetHash(), out.i);
    pubScript = out.tx->vout[out.i].scriptPubKey; // the inputs PubKey

    CTxDestination address1;
    ExtractDestination(pubScript, address1);

    const CKeyID* keyID = boost::get<CKeyID>(&address1);
    if (!keyID) {
        LogPrintf("CActiveMasternode::GetMasterNodeVin - Address does not refer to a key\n");
        return false;
    }

    if (!pwalletMain->GetKey(*keyID, secretKey)) {
        LogPrintf("CActiveMasternode::GetMasterNodeVin - Private key for address is not known\n");
        return false;
    }

    pubkey = secretKey.GetPubKey();
    return true;
}

// get all possible outputs for running Masternode
std::vector<COutput> CActiveMasternode::SelectCoinsMasternode()
{
    std::vector<COutput> vCoins;
    std::vector<COutput> filteredCoins;
    std::vector<COutPoint> confLockedCoins;

    // Temporary unlock MN coins from masternode.conf
    if (GetBoolArg("-mnconflock", true)) {
        uint256 mnTxHash;
        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            mnTxHash.SetHex(mne.getTxHash());

            int nIndex;
            if(!mne.castOutputIndex(nIndex))
                continue;

            COutPoint outpoint = COutPoint(mnTxHash, nIndex);
            confLockedCoins.push_back(outpoint);
            pwalletMain->UnlockCoin(outpoint);
        }
    }

    // Retrieve all possible outputs
    pwalletMain->AvailableCoins(&vCoins);

    // Lock MN coins from masternode.conf back if they where temporary unlocked
    if (!confLockedCoins.empty()) {
        for (COutPoint outpoint : confLockedCoins)
            pwalletMain->LockCoin(outpoint);
    }

    // Filter
    for (const COutput& out : vCoins) {
        if (out.tx->vout[out.i].nValue == 10000 * COIN) { //exactly
            filteredCoins.push_back(out);
        }
    }
    return filteredCoins;
}

// when starting a Masternode, this can enable to run as a hot wallet with no funds
bool CActiveMasternode::EnableHotColdMasterNode(CTxIn& newVin, CService& newService)
{
    if (!fMasterNode) return false;

    status = ACTIVE_MASTERNODE_STARTED;

    //The values below are needed for signing mnping messages going forward
    vin = newVin;
    service = newService;

    LogPrintf("CActiveMasternode::EnableHotColdMasterNode() - Enabled! You may shut down the cold daemon.\n");

    return true;
}
