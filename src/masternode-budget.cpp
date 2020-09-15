// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "main.h"

#include "addrman.h"
#include "chainparams.h"
#include "fs.h"
#include "masternode-budget.h"
#include "masternode-sync.h"
#include "masternode.h"
#include "masternodeman.h"
#include "netmessagemaker.h"
#include "util.h"


CBudgetManager budget;
RecursiveMutex cs_budget;

std::map<uint256, int64_t> askedForSourceProposalOrBudget;
std::vector<CBudgetProposalBroadcast> vecImmatureBudgetProposals;
std::vector<CFinalizedBudgetBroadcast> vecImmatureFinalizedBudgets;

int nSubmittedFinalBudget;

bool IsBudgetCollateralValid(const uint256& nTxCollateralHash, const uint256& nExpectedHash, std::string& strError, int64_t& nTime, int& nConf, bool fBudgetFinalization)
{
    CTransaction txCollateral;
    uint256 nBlockHash;
    if (!GetTransaction(nTxCollateralHash, txCollateral, nBlockHash, true)) {
        strError = strprintf("Can't find collateral tx %s", txCollateral.ToString());
        LogPrint(BCLog::MNBUDGET,"%s: %s\n", __func__, strError);
        return false;
    }

    if (txCollateral.vout.size() < 1) return false;
    if (txCollateral.nLockTime != 0) return false;

    CScript findScript;
    findScript << OP_RETURN << ToByteVector(nExpectedHash);

    bool foundOpReturn = false;
    for (const CTxOut &o : txCollateral.vout) {
        if (!o.scriptPubKey.IsNormalPaymentScript() && !o.scriptPubKey.IsUnspendable()) {
            strError = strprintf("Invalid Script %s", txCollateral.ToString());
            LogPrint(BCLog::MNBUDGET,"%s: %s\n", __func__, strError);
            return false;
        }
        if (fBudgetFinalization) {
            // Collateral for budget finalization
            // Note: there are still old valid budgets out there, but the check for the new 5 PIV finalization collateral
            //       will also cover the old 50 PIV finalization collateral.
            LogPrint(BCLog::MNBUDGET, "Final Budget: o.scriptPubKey(%s) == findScript(%s) ?\n", HexStr(o.scriptPubKey), HexStr(findScript));
            if (o.scriptPubKey == findScript) {
                LogPrint(BCLog::MNBUDGET, "Final Budget: o.nValue(%ld) >= BUDGET_FEE_TX(%ld) ?\n", o.nValue, BUDGET_FEE_TX);
                if(o.nValue >= BUDGET_FEE_TX) {
                    foundOpReturn = true;
                }
            }
        }
        else {
            // Collateral for normal budget proposal
            LogPrint(BCLog::MNBUDGET, "Normal Budget: o.scriptPubKey(%s) == findScript(%s) ?\n", HexStr(o.scriptPubKey), HexStr(findScript));
            if (o.scriptPubKey == findScript) {
                LogPrint(BCLog::MNBUDGET, "Normal Budget: o.nValue(%ld) >= PROPOSAL_FEE_TX(%ld) ?\n", o.nValue, PROPOSAL_FEE_TX);
                if(o.nValue >= PROPOSAL_FEE_TX) {
                    foundOpReturn = true;
                }
            }
        }
    }
    if (!foundOpReturn) {
        strError = strprintf("Couldn't find opReturn %s in %s", nExpectedHash.ToString(), txCollateral.ToString());
        LogPrint(BCLog::MNBUDGET,"%s: %s\n", __func__, strError);
        return false;
    }

    // RETRIEVE CONFIRMATIONS AND NTIME
    /*
        - nTime starts as zero and is passed-by-reference out of this function and stored in the external proposal
        - nTime is never validated via the hashing mechanism and comes from a full-validated source (the blockchain)
    */

    int conf = GetIXConfirmations(nTxCollateralHash);
    if (!nBlockHash.IsNull()) {
        BlockMap::iterator mi = mapBlockIndex.find(nBlockHash);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex)) {
                conf += chainActive.Height() - pindex->nHeight + 1;
                nTime = pindex->nTime;
            }
        }
    }

    nConf = conf;

    //if we're syncing we won't have swiftTX information, so accept 1 confirmation
    const int nRequiredConfs = Params().GetConsensus().nBudgetFeeConfirmations;
    if (conf >= nRequiredConfs) {
        return true;
    } else {
        strError = strprintf("Collateral requires at least %d confirmations - %d confirmations", nRequiredConfs, conf);
        LogPrint(BCLog::MNBUDGET,"%s: %s\n", __func__, strError);
        return false;
    }
}

void CBudgetManager::CheckOrphanVotes()
{
    LOCK(cs);


    std::string strError = "";
    auto it1 = mapOrphanMasternodeBudgetVotes.begin();
    while (it1 != mapOrphanMasternodeBudgetVotes.end()) {
        if (budget.UpdateProposal(((*it1).second), NULL, strError)) {
            LogPrint(BCLog::MNBUDGET,"%s: Proposal/Budget is known, activating and removing orphan vote\n", __func__);
            mapOrphanMasternodeBudgetVotes.erase(it1);
        }
        ++it1;
    }
    auto it2 = mapOrphanFinalizedBudgetVotes.begin();
    while (it2 != mapOrphanFinalizedBudgetVotes.end()) {
        if (budget.UpdateFinalizedBudget(((*it2).second), NULL, strError)) {
            LogPrint(BCLog::MNBUDGET,"%s: Proposal/Budget is known, activating and removing orphan vote\n", __func__);
            mapOrphanFinalizedBudgetVotes.erase(it2);
        }
        ++it2;
    }
    LogPrint(BCLog::MNBUDGET,"%s: Done\n", __func__);
}

void CBudgetManager::SubmitFinalBudget()
{
    static int nSubmittedHeight = 0; // height at which final budget was submitted last time
    int nCurrentHeight = GetBestHeight();

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int nBlockStart = nCurrentHeight - nCurrentHeight % nBlocksPerCycle + nBlocksPerCycle;
    if (nSubmittedHeight >= nBlockStart){
        LogPrint(BCLog::MNBUDGET,"%s: nSubmittedHeight(=%ld) < nBlockStart(=%ld) condition not fulfilled.\n",
                __func__, nSubmittedHeight, nBlockStart);
        return;
    }

     // Submit final budget during the last 2 days (2880 blocks) before payment for Mainnet, about 9 minutes (9 blocks) for Testnet
    int finalizationWindow = ((nBlocksPerCycle / 30) * 2);

    if (Params().NetworkID() == CBaseChainParams::TESTNET) {
        // NOTE: 9 blocks for testnet is way to short to have any masternode submit an automatic vote on the finalized(!) budget,
        //       because those votes are only submitted/relayed once every 56 blocks in CFinalizedBudget::AutoCheck()

        finalizationWindow = 64; // 56 + 4 finalization confirmations + 4 minutes buffer for propagation
    }

    int nFinalizationStart = nBlockStart - finalizationWindow;

    int nOffsetToStart = nFinalizationStart - nCurrentHeight;

    if (nBlockStart - nCurrentHeight > finalizationWindow) {
        LogPrint(BCLog::MNBUDGET,"%s: Too early for finalization. Current block is %ld, next Superblock is %ld.\n", __func__, nCurrentHeight, nBlockStart);
        LogPrint(BCLog::MNBUDGET,"%s: First possible block for finalization: %ld. Last possible block for finalization: %ld. "
                "You have to wait for %ld block(s) until Budget finalization will be possible\n", __func__, nFinalizationStart, nBlockStart, nOffsetToStart);
        return;
    }

    std::vector<CBudgetProposal*> vBudgetProposals = budget.GetBudget();
    std::string strBudgetName = "main";
    std::vector<CTxBudgetPayment> vecTxBudgetPayments;

    for (auto & vBudgetProposal : vBudgetProposals) {
        CTxBudgetPayment txBudgetPayment;
        txBudgetPayment.nProposalHash = vBudgetProposal->GetHash();
        txBudgetPayment.payee = vBudgetProposal->GetPayee();
        txBudgetPayment.nAmount = vBudgetProposal->GetAllotted();
        vecTxBudgetPayments.push_back(txBudgetPayment);
    }

    if (vecTxBudgetPayments.size() < 1) {
        LogPrint(BCLog::MNBUDGET,"%s: Found No Proposals For Period\n", __func__);
        return;
    }

    CFinalizedBudgetBroadcast tempBudget(strBudgetName, nBlockStart, vecTxBudgetPayments, UINT256_ZERO);
    const uint256& budgetHash = tempBudget.GetHash();
    if (HaveSeenFinalizedBudget(budgetHash)) {
        LogPrint(BCLog::MNBUDGET,"%s: Budget already exists - %s\n", __func__, budgetHash.ToString());
        nSubmittedHeight = nCurrentHeight;
        return; //already exists
    }

    //create fee tx
    CTransaction tx;
    uint256 txidCollateral;

    if (!mapCollateralTxids.count(budgetHash)) {
        CWalletTx wtx;
        // Get our change address
        CReserveKey keyChange(pwalletMain);
        if (!pwalletMain->CreateBudgetFeeTX(wtx, budgetHash, keyChange, true)) {
            LogPrint(BCLog::MNBUDGET,"%s: Can't make collateral transaction\n", __func__);
            return;
        }

        // Send the tx to the network. Do NOT use SwiftTx, locking might need too much time to propagate, especially for testnet
        const CWallet::CommitResult& res = pwalletMain->CommitTransaction(wtx, keyChange, g_connman.get(), "NO-ix");
        if (res.status != CWallet::CommitStatus::OK)
            return;
        tx = (CTransaction)wtx;
        txidCollateral = tx.GetHash();
        mapCollateralTxids.emplace(budgetHash, txidCollateral);
    } else {
        txidCollateral = mapCollateralTxids[budgetHash];
    }

    //create the proposal incase we're the first to make it
    CFinalizedBudgetBroadcast finalizedBudgetBroadcast(strBudgetName, nBlockStart, vecTxBudgetPayments, txidCollateral);

    // check
    int nConf = 0;
    int64_t nTime = 0;
    std::string strError = "";
    if (!IsBudgetCollateralValid(txidCollateral, finalizedBudgetBroadcast.GetHash(), strError, nTime, nConf, true)) {
        LogPrint(BCLog::MNBUDGET,"%s: Invalid Collateral for finalized budget - %s \n", __func__, strError);
        return;
    }

    if (!finalizedBudgetBroadcast.UpdateValid(nCurrentHeight)) {
        LogPrint(BCLog::MNBUDGET,"%s: Invalid finalized budget - %s \n", __func__, finalizedBudgetBroadcast.IsInvalidReason());
        return;
    }

    LOCK(cs);
    AddSeenFinalizedBudget(finalizedBudgetBroadcast);
    finalizedBudgetBroadcast.Relay();
    budget.AddFinalizedBudget(finalizedBudgetBroadcast);
    nSubmittedHeight = nCurrentHeight;
    LogPrint(BCLog::MNBUDGET,"%s: Done! %s\n", __func__, finalizedBudgetBroadcast.GetHash().ToString());
}

//
// CBudgetDB
//

CBudgetDB::CBudgetDB()
{
    pathDB = GetDataDir() / "budget.dat";
    strMagicMessage = "MasternodeBudget";
}

bool CBudgetDB::Write(const CBudgetManager& objToSave)
{
    LOCK(objToSave.cs);

    int64_t nStart = GetTimeMillis();

    // serialize, checksum data up to that point, then append checksum
    CDataStream ssObj(SER_DISK, CLIENT_VERSION);
    ssObj << strMagicMessage;                   // masternode cache file specific magic message
    ssObj << FLATDATA(Params().MessageStart()); // network specific magic number
    ssObj << objToSave;
    uint256 hash = Hash(ssObj.begin(), ssObj.end());
    ssObj << hash;

    // open output file, and associate with CAutoFile
    FILE* file = fsbridge::fopen(pathDB, "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathDB.string());

    // Write and commit header, data
    try {
        fileout << ssObj;
    } catch (const std::exception& e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
    fileout.fclose();

    LogPrint(BCLog::MNBUDGET,"Written info to budget.dat  %dms\n", GetTimeMillis() - nStart);

    return true;
}

CBudgetDB::ReadResult CBudgetDB::Read(CBudgetManager& objToLoad, bool fDryRun)
{
    LOCK(objToLoad.cs);

    int64_t nStart = GetTimeMillis();
    // open input file, and associate with CAutoFile
    FILE* file = fsbridge::fopen(pathDB, "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        error("%s : Failed to open file %s", __func__, pathDB.string());
        return FileError;
    }

    // use file size to size memory buffer
    int fileSize = fs::file_size(pathDB);
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if (dataSize < 0)
        dataSize = 0;
    std::vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char*)&vchData[0], dataSize);
        filein >> hashIn;
    } catch (const std::exception& e) {
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return HashReadError;
    }
    filein.fclose();

    CDataStream ssObj(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssObj.begin(), ssObj.end());
    if (hashIn != hashTmp) {
        error("%s : Checksum mismatch, data corrupted", __func__);
        return IncorrectHash;
    }


    unsigned char pchMsgTmp[4];
    std::string strMagicMessageTmp;
    try {
        // de-serialize file header (masternode cache file specific magic message) and ..
        ssObj >> strMagicMessageTmp;

        // ... verify the message matches predefined one
        if (strMagicMessage != strMagicMessageTmp) {
            error("%s : Invalid masternode cache magic message", __func__);
            return IncorrectMagicMessage;
        }


        // de-serialize file header (network specific magic number) and ..
        ssObj >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp))) {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }

        // de-serialize data into CBudgetManager object
        ssObj >> objToLoad;
    } catch (const std::exception& e) {
        objToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrint(BCLog::MNBUDGET,"Loaded info from budget.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrint(BCLog::MNBUDGET,"  %s\n", objToLoad.ToString());
    if (!fDryRun) {
        LogPrint(BCLog::MNBUDGET,"Budget manager - cleaning....\n");
        objToLoad.CheckAndRemove();
        LogPrint(BCLog::MNBUDGET,"Budget manager - result:\n");
        LogPrint(BCLog::MNBUDGET,"  %s\n", objToLoad.ToString());
    }

    return Ok;
}

void DumpBudgets()
{
    int64_t nStart = GetTimeMillis();

    CBudgetDB budgetdb;
    CBudgetManager tempBudget;

    LogPrint(BCLog::MNBUDGET,"Verifying budget.dat format...\n");
    CBudgetDB::ReadResult readResult = budgetdb.Read(tempBudget, true);
    // there was an error and it was not an error on file opening => do not proceed
    if (readResult == CBudgetDB::FileError)
        LogPrint(BCLog::MNBUDGET,"Missing budgets file - budget.dat, will try to recreate\n");
    else if (readResult != CBudgetDB::Ok) {
        LogPrint(BCLog::MNBUDGET,"Error reading budget.dat: ");
        if (readResult == CBudgetDB::IncorrectFormat)
            LogPrint(BCLog::MNBUDGET,"magic is ok but data has invalid format, will try to recreate\n");
        else {
            LogPrint(BCLog::MNBUDGET,"file format is unknown or invalid, please fix it manually\n");
            return;
        }
    }
    LogPrint(BCLog::MNBUDGET,"Writting info to budget.dat...\n");
    budgetdb.Write(budget);

    LogPrint(BCLog::MNBUDGET,"Budget dump finished  %dms\n", GetTimeMillis() - nStart);
}

bool CBudgetManager::AddFinalizedBudget(CFinalizedBudget& finalizedBudget)
{
    if (!finalizedBudget.UpdateValid(GetBestHeight())) {
        LogPrint(BCLog::MNBUDGET,"%s: invalid finalized budget - %s\n", __func__, finalizedBudget.IsInvalidReason());
        return false;
    }

    if (mapFinalizedBudgets.count(finalizedBudget.GetHash())) {
        return false;
    }

    mapFinalizedBudgets.insert(std::make_pair(finalizedBudget.GetHash(), finalizedBudget));
    return true;
}

bool CBudgetManager::AddProposal(CBudgetProposal& budgetProposal)
{
    LOCK(cs);
    if (!budgetProposal.UpdateValid(GetBestHeight())) {
        LogPrint(BCLog::MNBUDGET,"%s: invalid budget proposal - %s\n", __func__, budgetProposal.IsInvalidReason());
        return false;
    }

    if (mapProposals.count(budgetProposal.GetHash())) {
        return false;
    }

    mapProposals.insert(std::make_pair(budgetProposal.GetHash(), budgetProposal));
    LogPrint(BCLog::MNBUDGET,"%s: proposal %s added\n", __func__, budgetProposal.GetName());
    return true;
}

void CBudgetManager::CheckAndRemove()
{
    int nCurrentHeight = GetBestHeight();
    std::map<uint256, CFinalizedBudget> tmpMapFinalizedBudgets;
    std::map<uint256, CBudgetProposal> tmpMapProposals;

    LogPrint(BCLog::MNBUDGET, "%s: mapFinalizedBudgets cleanup - size before: %d\n", __func__, mapFinalizedBudgets.size());
    for (auto& it: mapFinalizedBudgets) {
        CFinalizedBudget* pfinalizedBudget = &(it.second);
        if (!pfinalizedBudget->UpdateValid(nCurrentHeight)) {
            LogPrint(BCLog::MNBUDGET,"%s: Invalid finalized budget: %s\n", __func__, pfinalizedBudget->IsInvalidReason());
        } else {
            LogPrint(BCLog::MNBUDGET,"%s: Found valid finalized budget: %s %s\n", __func__,
                      pfinalizedBudget->GetName(), pfinalizedBudget->GetFeeTXHash().ToString());
            pfinalizedBudget->CheckAndVote();
            tmpMapFinalizedBudgets.insert(std::make_pair(pfinalizedBudget->GetHash(), *pfinalizedBudget));
        }
    }

    LogPrint(BCLog::MNBUDGET, "%s: mapProposals cleanup - size before: %d\n", __func__, mapProposals.size());
    for (auto& it: mapProposals) {
        CBudgetProposal* pbudgetProposal = &(it.second);
        if (!pbudgetProposal->UpdateValid(nCurrentHeight)) {
            LogPrint(BCLog::MNBUDGET,"%s: Invalid budget proposal - %s\n", __func__, pbudgetProposal->IsInvalidReason());
        } else {
             LogPrint(BCLog::MNBUDGET,"%s: Found valid budget proposal: %s %s\n", __func__,
                      pbudgetProposal->GetName(), pbudgetProposal->GetFeeTXHash().ToString());
             tmpMapProposals.insert(std::make_pair(pbudgetProposal->GetHash(), *pbudgetProposal));
        }
    }

    // Remove invalid entries by overwriting complete map
    mapFinalizedBudgets.swap(tmpMapFinalizedBudgets);
    mapProposals.swap(tmpMapProposals);

    LogPrint(BCLog::MNBUDGET, "%s: mapFinalizedBudgets cleanup - size after: %d\n", __func__, mapFinalizedBudgets.size());
    LogPrint(BCLog::MNBUDGET, "%s: mapProposals cleanup - size after: %d\n", __func__, mapProposals.size());
    LogPrint(BCLog::MNBUDGET,"%s: PASSED\n", __func__);

}

void CBudgetManager::FillBlockPayee(CMutableTransaction& txNew, bool fProofOfStake)
{
    LOCK(cs);

    int chainHeight = GetBestHeight();
    if (chainHeight <= 0) return;

    int nHighestCount = 0;
    CScript payee;
    CAmount nAmount = 0;

    // ------- Grab The Highest Count

    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);
        if (pfinalizedBudget->GetVoteCount() > nHighestCount &&
            chainHeight + 1 >= pfinalizedBudget->GetBlockStart() &&
            chainHeight + 1 <= pfinalizedBudget->GetBlockEnd() &&
            pfinalizedBudget->GetPayeeAndAmount(chainHeight + 1, payee, nAmount)) {
            nHighestCount = pfinalizedBudget->GetVoteCount();
        }

        ++it;
    }

    CAmount blockValue = GetBlockValue(chainHeight);

    if (fProofOfStake) {
        if (nHighestCount > 0) {
            unsigned int i = txNew.vout.size();
            txNew.vout.resize(i + 1);
            txNew.vout[i].scriptPubKey = payee;
            txNew.vout[i].nValue = nAmount;

            CTxDestination address1;
            ExtractDestination(payee, address1);
            LogPrint(BCLog::MNBUDGET,"%s: Budget payment to %s for %lld, nHighestCount = %d\n", __func__, EncodeDestination(address1), nAmount, nHighestCount);
        }
        else {
            LogPrint(BCLog::MNBUDGET,"%s: No Budget payment, nHighestCount = %d\n", __func__, nHighestCount);
        }
    } else {
        //miners get the full amount on these blocks
        txNew.vout[0].nValue = blockValue;

        if (nHighestCount > 0) {
            txNew.vout.resize(2);

            //these are super blocks, so their value can be much larger than normal
            txNew.vout[1].scriptPubKey = payee;
            txNew.vout[1].nValue = nAmount;

            CTxDestination address1;
            ExtractDestination(payee, address1);
            LogPrint(BCLog::MNBUDGET,"%s: Budget payment to %s for %lld\n", __func__, EncodeDestination(address1), nAmount);
        }
    }
}

CFinalizedBudget* CBudgetManager::FindFinalizedBudget(const uint256& nHash)
{
    if (mapFinalizedBudgets.count(nHash))
        return &mapFinalizedBudgets[nHash];

    return NULL;
}

const CBudgetProposal* CBudgetManager::FindProposalByName(const std::string& strProposalName) const
{
    int64_t nYesCountMax = std::numeric_limits<int64_t>::min();
    const CBudgetProposal* pbudgetProposal = nullptr;

    for (const auto& it: mapProposals) {
        const CBudgetProposal& proposal = it.second;
        int64_t nYesCount = proposal.GetYeas() - proposal.GetNays();
        if (proposal.GetName() == strProposalName && nYesCount > nYesCountMax) {
            pbudgetProposal = &proposal;
            nYesCountMax = nYesCount;
        }
    }

    return pbudgetProposal;
}

CBudgetProposal* CBudgetManager::FindProposal(const uint256& nHash)
{
    LOCK(cs);

    if (mapProposals.count(nHash))
        return &mapProposals[nHash];

    return NULL;
}

bool CBudgetManager::IsBudgetPaymentBlock(int nBlockHeight)
{
    int nHighestCount = -1;
    int nFivePercent = mnodeman.CountEnabled(ActiveProtocol()) / 20;

    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);
        if (pfinalizedBudget->GetVoteCount() > nHighestCount &&
            nBlockHeight >= pfinalizedBudget->GetBlockStart() &&
            nBlockHeight <= pfinalizedBudget->GetBlockEnd()) {
            nHighestCount = pfinalizedBudget->GetVoteCount();
        }

        ++it;
    }

    LogPrint(BCLog::MNBUDGET,"%s: nHighestCount: %lli, 5%% of Masternodes: %lli. Number of finalized budgets: %lli\n",
            __func__, nHighestCount, nFivePercent, mapFinalizedBudgets.size());

    // If budget doesn't have 5% of the network votes, then we should pay a masternode instead
    if (nHighestCount > nFivePercent) return true;

    return false;
}

TrxValidationStatus CBudgetManager::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    LOCK(cs);

    TrxValidationStatus transactionStatus = TrxValidationStatus::InValid;
    int nHighestCount = 0;
    int nFivePercent = mnodeman.CountEnabled(ActiveProtocol()) / 20;
    std::vector<CFinalizedBudget*> ret;

    LogPrint(BCLog::MNBUDGET,"%s: checking %lli finalized budgets\n", __func__, mapFinalizedBudgets.size());

    // ------- Grab The Highest Count

    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);

        if (pfinalizedBudget->GetVoteCount() > nHighestCount &&
            nBlockHeight >= pfinalizedBudget->GetBlockStart() &&
            nBlockHeight <= pfinalizedBudget->GetBlockEnd()) {
            nHighestCount = pfinalizedBudget->GetVoteCount();
        }

        ++it;
    }

    LogPrint(BCLog::MNBUDGET,"%s: nHighestCount: %lli, 5%% of Masternodes: %lli mapFinalizedBudgets.size(): %ld\n",
            __func__, nHighestCount, nFivePercent, mapFinalizedBudgets.size());
    /*
        If budget doesn't have 5% of the network votes, then we should pay a masternode instead
    */
    if (nHighestCount < nFivePercent) return TrxValidationStatus::InValid;

    // check the highest finalized budgets (+/- 10% to assist in consensus)

    std::string strProposals = "";
    int nCountThreshold = nHighestCount - mnodeman.CountEnabled(ActiveProtocol()) / 10;
    bool fThreshold = false;
    it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);
        strProposals = pfinalizedBudget->GetProposals();

        LogPrint(BCLog::MNBUDGET,"%s: checking budget (%s) with blockstart %lli, blockend %lli, nBlockHeight %lli, votes %lli, nCountThreshold %lli\n",
                __func__, strProposals.c_str(), pfinalizedBudget->GetBlockStart(), pfinalizedBudget->GetBlockEnd(),
                nBlockHeight, pfinalizedBudget->GetVoteCount(), nCountThreshold);

        if (pfinalizedBudget->GetVoteCount() > nCountThreshold) {
            fThreshold = true;
            LogPrint(BCLog::MNBUDGET,"%s: GetVoteCount() > nCountThreshold passed\n", __func__);
            if (nBlockHeight >= pfinalizedBudget->GetBlockStart() && nBlockHeight <= pfinalizedBudget->GetBlockEnd()) {
                LogPrint(BCLog::MNBUDGET,"%s: GetBlockStart() passed\n", __func__);
                transactionStatus = pfinalizedBudget->IsTransactionValid(txNew, nBlockHeight);
                if (transactionStatus == TrxValidationStatus::Valid) {
                    LogPrint(BCLog::MNBUDGET,"%s: pfinalizedBudget->IsTransactionValid() passed\n", __func__);
                    return TrxValidationStatus::Valid;
                }
                else {
                    LogPrint(BCLog::MNBUDGET,"%s: pfinalizedBudget->IsTransactionValid() error\n", __func__);
                }
            }
            else {
                LogPrint(BCLog::MNBUDGET,"%s: GetBlockStart() failed, budget is outside current payment cycle and will be ignored.\n", __func__);
            }

        }

        ++it;
    }

    // If not enough masternodes autovoted for any of the finalized budgets pay a masternode instead
    if(!fThreshold) {
        transactionStatus = TrxValidationStatus::VoteThreshold;
    }

    // We looked through all of the known budgets
    return transactionStatus;
}

std::vector<CBudgetProposal*> CBudgetManager::GetAllProposals()
{
    LOCK(cs);

    std::vector<CBudgetProposal*> vBudgetProposalRet;

    for (auto& it: mapProposals) {
        CBudgetProposal* pbudgetProposal = &(it.second);
        pbudgetProposal->CleanAndRemove();
        vBudgetProposalRet.push_back(pbudgetProposal);
    }

    std::sort(vBudgetProposalRet.begin(), vBudgetProposalRet.end(), CBudgetProposal::PtrHigherYes);

    return vBudgetProposalRet;
}

//Need to review this function
std::vector<CBudgetProposal*> CBudgetManager::GetBudget()
{
    LOCK(cs);

    int nHeight = GetBestHeight();
    if (nHeight <= 0)
        return std::vector<CBudgetProposal*>();

    // ------- Sort budgets by net Yes Count
    std::vector<CBudgetProposal*> vBudgetPorposalsSort;
    for (auto& it: mapProposals) {
        it.second.CleanAndRemove();
        vBudgetPorposalsSort.push_back(&it.second);
    }
    std::sort(vBudgetPorposalsSort.begin(), vBudgetPorposalsSort.end(), CBudgetProposal::PtrHigherYes);

    // ------- Grab The Budgets In Order
    std::vector<CBudgetProposal*> vBudgetProposalsRet;
    CAmount nBudgetAllocated = 0;

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int nBlockStart = nHeight - nHeight % nBlocksPerCycle + nBlocksPerCycle;
    int nBlockEnd = nBlockStart + nBlocksPerCycle - 1;
    int mnCount = mnodeman.CountEnabled(ActiveProtocol());
    CAmount nTotalBudget = GetTotalBudget(nBlockStart);

    for (CBudgetProposal* pbudgetProposal: vBudgetPorposalsSort) {
        LogPrint(BCLog::MNBUDGET,"%s: Processing Budget %s\n", __func__, pbudgetProposal->GetName());
        //prop start/end should be inside this period
        if (pbudgetProposal->IsPassing(nBlockStart, nBlockEnd, mnCount)) {
            LogPrint(BCLog::MNBUDGET,"%s:  -   Check 1 passed: valid=%d | %ld <= %ld | %ld >= %ld | Yeas=%d Nays=%d Count=%d | established=%d\n",
                    __func__, pbudgetProposal->IsValid(), pbudgetProposal->GetBlockStart(), nBlockStart, pbudgetProposal->GetBlockEnd(),
                    nBlockEnd, pbudgetProposal->GetYeas(), pbudgetProposal->GetNays(), mnCount / 10, pbudgetProposal->IsEstablished());

            if (pbudgetProposal->GetAmount() + nBudgetAllocated <= nTotalBudget) {
                pbudgetProposal->SetAllotted(pbudgetProposal->GetAmount());
                nBudgetAllocated += pbudgetProposal->GetAmount();
                vBudgetProposalsRet.push_back(pbudgetProposal);
                LogPrint(BCLog::MNBUDGET,"%s:  -     Check 2 passed: Budget added\n", __func__);
            } else {
                pbudgetProposal->SetAllotted(0);
                LogPrint(BCLog::MNBUDGET,"%s:  -     Check 2 failed: no amount allotted\n", __func__);
            }

        } else {
            LogPrint(BCLog::MNBUDGET,"%s:  -   Check 1 failed: valid=%d | %ld <= %ld | %ld >= %ld | Yeas=%d Nays=%d Count=%d | established=%d\n",
                    __func__, pbudgetProposal->IsValid(), pbudgetProposal->GetBlockStart(), nBlockStart, pbudgetProposal->GetBlockEnd(),
                    nBlockEnd, pbudgetProposal->GetYeas(), pbudgetProposal->GetNays(), mnodeman.CountEnabled(ActiveProtocol()) / 10,
                    pbudgetProposal->IsEstablished());
        }

    }

    return vBudgetProposalsRet;
}

std::vector<CFinalizedBudget*> CBudgetManager::GetFinalizedBudgets()
{
    LOCK(cs);

    std::vector<CFinalizedBudget*> vFinalizedBudgetsRet;

    // ------- Grab The Budgets In Order
    for (auto& it: mapFinalizedBudgets) {
        vFinalizedBudgetsRet.push_back(&(it.second));
    }
    std::sort(vFinalizedBudgetsRet.begin(), vFinalizedBudgetsRet.end(), CFinalizedBudget::PtrGreater);

    return vFinalizedBudgetsRet;
}

std::string CBudgetManager::GetRequiredPaymentsString(int nBlockHeight)
{
    LOCK(cs);

    std::string ret = "unknown-budget";

    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);
        if (nBlockHeight >= pfinalizedBudget->GetBlockStart() && nBlockHeight <= pfinalizedBudget->GetBlockEnd()) {
            CTxBudgetPayment payment;
            if (pfinalizedBudget->GetBudgetPaymentByBlock(nBlockHeight, payment)) {
                if (ret == "unknown-budget") {
                    ret = payment.nProposalHash.ToString();
                } else {
                    ret += ",";
                    ret += payment.nProposalHash.ToString();
                }
            } else {
                LogPrint(BCLog::MNBUDGET,"%s:  Couldn't find budget payment for block %d\n", __func__, nBlockHeight);
            }
        }

        ++it;
    }

    return ret;
}

CAmount CBudgetManager::GetTotalBudget(int nHeight)
{
    if (Params().NetworkID() == CBaseChainParams::TESTNET) {
        CAmount nSubsidy = 500 * COIN;
        return ((nSubsidy / 100) * 10) * 146;
    }

    //get block value and calculate from that
    CAmount nSubsidy = 0;
    const Consensus::Params& consensus = Params().GetConsensus();
    const bool isPoSActive = consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_POS);
    if (nHeight >= 151200 && !isPoSActive) {
        nSubsidy = 50 * COIN;
    } else if (isPoSActive && nHeight <= 302399) {
        nSubsidy = 50 * COIN;
    } else if (nHeight <= 345599 && nHeight >= 302400) {
        nSubsidy = 45 * COIN;
    } else if (nHeight <= 388799 && nHeight >= 345600) {
        nSubsidy = 40 * COIN;
    } else if (nHeight <= 431999 && nHeight >= 388800) {
        nSubsidy = 35 * COIN;
    } else if (nHeight <= 475199 && nHeight >= 432000) {
        nSubsidy = 30 * COIN;
    } else if (nHeight <= 518399 && nHeight >= 475200) {
        nSubsidy = 25 * COIN;
    } else if (nHeight <= 561599 && nHeight >= 518400) {
        nSubsidy = 20 * COIN;
    } else if (nHeight <= 604799 && nHeight >= 561600) {
        nSubsidy = 15 * COIN;
    } else if (nHeight <= 647999 && nHeight >= 604800) {
        nSubsidy = 10 * COIN;
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_ZC_V2)) {
        nSubsidy = 10 * COIN;
    } else {
        nSubsidy = 5 * COIN;
    }

    // Amount of blocks in a months period of time (using 1 minutes per) = (60*24*30)
    if (nHeight <= 172800) {
        return 648000 * COIN;
    } else {
        return ((nSubsidy / 100) * 10) * 1440 * 30;
    }
}

void CBudgetManager::AddSeenProposal(const CBudgetProposalBroadcast& prop)
{
    mapSeenMasternodeBudgetProposals.insert(std::make_pair(prop.GetHash(), prop));
}

void CBudgetManager::AddSeenProposalVote(const CBudgetVote& vote)
{
    mapSeenMasternodeBudgetVotes.insert(std::make_pair(vote.GetHash(), vote));
}

void CBudgetManager::AddSeenFinalizedBudget(const CFinalizedBudgetBroadcast& bud)
{
    mapSeenFinalizedBudgets.insert(std::make_pair(bud.GetHash(), bud));
}

void CBudgetManager::AddSeenFinalizedBudgetVote(const CFinalizedBudgetVote& vote)
{
    mapSeenFinalizedBudgetVotes.insert(std::make_pair(vote.GetHash(), vote));
}


CDataStream CBudgetManager::GetProposalVoteSerialized(const uint256& voteHash) const
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(1000);
    ss << mapSeenMasternodeBudgetVotes.at(voteHash);
    return ss;
}

CDataStream CBudgetManager::GetProposalSerialized(const uint256& propHash) const
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(1000);
    ss << mapSeenMasternodeBudgetProposals.at(propHash);
    return ss;
}

CDataStream CBudgetManager::GetFinalizedBudgetVoteSerialized(const uint256& voteHash) const
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(1000);
    ss << mapSeenFinalizedBudgetVotes.at(voteHash);
    return ss;
}

CDataStream CBudgetManager::GetFinalizedBudgetSerialized(const uint256& budgetHash) const
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(1000);
    ss << mapSeenFinalizedBudgets.at(budgetHash);
    return ss;
}

bool CBudgetManager::AddAndRelayProposalVote(const CBudgetVote& vote, std::string& strError)
{
    if (UpdateProposal(vote, nullptr, strError)) {
        AddSeenProposalVote(vote);
        vote.Relay();
        return true;
    }
    return false;
}

void CBudgetManager::NewBlock(int height)
{
    SetBestHeight(height);

    if (masternodeSync.RequestedMasternodeAssets <= MASTERNODE_SYNC_BUDGET) return;

    if (strBudgetMode == "suggest") { //suggest the budget we see
        SubmitFinalBudget();
    }

    int nCurrentHeight = GetBestHeight();
    //this function should be called 1/14 blocks, allowing up to 100 votes per day on all proposals
    if (nCurrentHeight % 14 != 0) return;

    // incremental sync with our peers
    if (masternodeSync.IsSynced()) {
        LogPrint(BCLog::MNBUDGET,"%s:  incremental sync started\n", __func__);
        if (rand() % 1440 == 0) {
            ClearSeen();
            ResetSync();
        }

        CBudgetManager* manager = this;
        g_connman->ForEachNode([manager](CNode* pnode){
            if (pnode->nVersion >= ActiveProtocol())
                manager->Sync(pnode, UINT256_ZERO, true);
        });
        MarkSynced();
    }

    TRY_LOCK(cs, fBudgetNewBlock);
    if (!fBudgetNewBlock) return;
    CheckAndRemove();

    //remove invalid votes once in a while (we have to check the signatures and validity of every vote, somewhat CPU intensive)

    LogPrint(BCLog::MNBUDGET,"%s:  askedForSourceProposalOrBudget cleanup - size: %d\n", __func__, askedForSourceProposalOrBudget.size());
    std::map<uint256, int64_t>::iterator it = askedForSourceProposalOrBudget.begin();
    while (it != askedForSourceProposalOrBudget.end()) {
        if ((*it).second > GetTime() - (60 * 60 * 24)) {
            ++it;
        } else {
            askedForSourceProposalOrBudget.erase(it++);
        }
    }

    LogPrint(BCLog::MNBUDGET,"%s:  mapProposals cleanup - size: %d\n", __func__, mapProposals.size());
    std::map<uint256, CBudgetProposal>::iterator it2 = mapProposals.begin();
    while (it2 != mapProposals.end()) {
        (*it2).second.CleanAndRemove();
        ++it2;
    }

    LogPrint(BCLog::MNBUDGET,"%s:  mapFinalizedBudgets cleanup - size: %d\n", __func__, mapFinalizedBudgets.size());
    std::map<uint256, CFinalizedBudget>::iterator it3 = mapFinalizedBudgets.begin();
    while (it3 != mapFinalizedBudgets.end()) {
        (*it3).second.CleanAndRemove();
        ++it3;
    }

    LogPrint(BCLog::MNBUDGET,"%s:  vecImmatureBudgetProposals cleanup - size: %d\n", __func__, vecImmatureBudgetProposals.size());
    std::vector<CBudgetProposalBroadcast>::iterator it4 = vecImmatureBudgetProposals.begin();
    while (it4 != vecImmatureBudgetProposals.end()) {
        std::string strError = "";
        int nConf = 0;
        const uint256& nHash = it4->GetHash();
        if (!IsBudgetCollateralValid(it4->GetFeeTXHash(), nHash, strError, (*it4).nTime, nConf)) {
            ++it4;
            continue;
        }

        if (!(*it4).UpdateValid(nCurrentHeight)) {
            LogPrint(BCLog::MNBUDGET,"mprop (immature) - invalid budget proposal - %s\n", it4->IsInvalidReason());
            it4 = vecImmatureBudgetProposals.erase(it4);
            continue;
        }

        CBudgetProposal budgetProposal((*it4));
        if (AddProposal(budgetProposal)) {
            (*it4).Relay();
        }

        LogPrint(BCLog::MNBUDGET,"mprop (immature) - new budget - %s\n", nHash.ToString());
        it4 = vecImmatureBudgetProposals.erase(it4);
    }

    LogPrint(BCLog::MNBUDGET,"%s:  vecImmatureFinalizedBudgets cleanup - size: %d\n", __func__, vecImmatureFinalizedBudgets.size());
    std::vector<CFinalizedBudgetBroadcast>::iterator it5 = vecImmatureFinalizedBudgets.begin();
    while (it5 != vecImmatureFinalizedBudgets.end()) {
        std::string strError = "";
        int nConf = 0;
        const uint256& nHash = it5->GetHash();
        if (!IsBudgetCollateralValid(it5->GetFeeTXHash(), nHash, strError, (*it5).nTime, nConf, true)) {
            ++it5;
            continue;
        }

        if (!(*it5).UpdateValid(nCurrentHeight)) {
            LogPrint(BCLog::MNBUDGET,"fbs (immature) - invalid finalized budget - %s\n", it5->IsInvalidReason());
            it5 = vecImmatureFinalizedBudgets.erase(it5);
            continue;
        }

        LogPrint(BCLog::MNBUDGET,"fbs (immature) - new finalized budget - %s\n", nHash.ToString());

        CFinalizedBudget finalizedBudget((*it5));
        if (AddFinalizedBudget(finalizedBudget)) {
            (*it5).Relay();
        }

        it5 = vecImmatureFinalizedBudgets.erase(it5);
    }
    LogPrint(BCLog::MNBUDGET,"%s:  PASSED\n", __func__);
}

void CBudgetManager::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    // lite mode is not supported
    if (fLiteMode) return;
    if (!masternodeSync.IsBlockchainSynced()) return;

    int nCurrentHeight = GetBestHeight();

    LOCK(cs_budget);

    if (strCommand == NetMsgType::BUDGETVOTESYNC) { //Masternode vote sync
        uint256 nProp;
        vRecv >> nProp;

        if (Params().NetworkID() == CBaseChainParams::MAIN) {
            if (nProp.IsNull()) {
                if (pfrom->HasFulfilledRequest("budgetvotesync")) {
                    LogPrint(BCLog::MNBUDGET,"mnvs - peer already asked me for the list\n");
                    LOCK(cs_main);
                    Misbehaving(pfrom->GetId(), 20);
                    return;
                }
                pfrom->FulfilledRequest("budgetvotesync");
            }
        }

        Sync(pfrom, nProp);
        LogPrint(BCLog::MNBUDGET, "mnvs - Sent Masternode votes to peer %i\n", pfrom->GetId());
    }

    if (strCommand == NetMsgType::BUDGETPROPOSAL) { //Masternode Proposal
        CBudgetProposalBroadcast budgetProposalBroadcast;
        vRecv >> budgetProposalBroadcast;

        if (HaveSeenProposal(budgetProposalBroadcast.GetHash())) {
            masternodeSync.AddedBudgetItem(budgetProposalBroadcast.GetHash());
            return;
        }

        std::string strError = "";
        int nConf = 0;
        const uint256& nHash = budgetProposalBroadcast.GetHash();
        const uint256& nFeeTXHash = budgetProposalBroadcast.GetFeeTXHash();
        if (!IsBudgetCollateralValid(nFeeTXHash, nHash, strError, budgetProposalBroadcast.nTime, nConf)) {
            LogPrint(BCLog::MNBUDGET,"Proposal FeeTX is not valid - %s - %s\n", nFeeTXHash.ToString(), strError);
            if (nConf >= 1) vecImmatureBudgetProposals.push_back(budgetProposalBroadcast);
            return;
        }

        AddSeenProposal(budgetProposalBroadcast);

        if (!budgetProposalBroadcast.UpdateValid(nCurrentHeight)) {
            LogPrint(BCLog::MNBUDGET,"mprop - invalid budget proposal - %s\n", budgetProposalBroadcast.IsInvalidReason());
            return;
        }

        CBudgetProposal budgetProposal(budgetProposalBroadcast);
        if (AddProposal(budgetProposal)) {
            budgetProposalBroadcast.Relay();
        }
        masternodeSync.AddedBudgetItem(nHash);

        LogPrint(BCLog::MNBUDGET,"mprop - new budget - %s\n", nHash.ToString());

        //We might have active votes for this proposal that are valid now
        CheckOrphanVotes();
    }

    if (strCommand == NetMsgType::BUDGETVOTE) { // Budget Vote
        CBudgetVote vote;
        vRecv >> vote;
        vote.SetValid(true);

        if (HaveSeenProposalVote(vote.GetHash())) {
            masternodeSync.AddedBudgetItem(vote.GetHash());
            return;
        }

        const CTxIn& voteVin = vote.GetVin();
        CMasternode* pmn = mnodeman.Find(voteVin);
        if (pmn == NULL) {
            LogPrint(BCLog::MNBUDGET,"mvote - unknown masternode - vin: %s\n", voteVin.ToString());
            mnodeman.AskForMN(pfrom, voteVin);
            return;
        }


        AddSeenProposalVote(vote);
        if (!vote.CheckSignature()) {
            if (masternodeSync.IsSynced()) {
                LogPrintf("mvote - signature invalid\n");
                LOCK(cs_main);
                Misbehaving(pfrom->GetId(), 20);
            }
            // it could just be a non-synced masternode
            mnodeman.AskForMN(pfrom, voteVin);
            return;
        }

        std::string strError = "";
        if (UpdateProposal(vote, pfrom, strError)) {
            vote.Relay();
            masternodeSync.AddedBudgetItem(vote.GetHash());
        }

        LogPrint(BCLog::MNBUDGET,"mvote - new budget vote for budget %s - %s\n", vote.GetProposalHash().ToString(),  vote.GetHash().ToString());
    }

    if (strCommand == NetMsgType::FINALBUDGET) { //Finalized Budget Suggestion
        CFinalizedBudgetBroadcast finalizedBudgetBroadcast;
        vRecv >> finalizedBudgetBroadcast;

        if (HaveSeenFinalizedBudget(finalizedBudgetBroadcast.GetHash())) {
            masternodeSync.AddedBudgetItem(finalizedBudgetBroadcast.GetHash());
            return;
        }

        std::string strError = "";
        int nConf = 0;
        const uint256& nHash = finalizedBudgetBroadcast.GetHash();
        const uint256& nFeeTXHash = finalizedBudgetBroadcast.GetFeeTXHash();
        if (!IsBudgetCollateralValid(nFeeTXHash, nHash, strError, finalizedBudgetBroadcast.nTime, nConf, true)) {
            LogPrint(BCLog::MNBUDGET,"fbs - Finalized Budget FeeTX is not valid - %s - %s\n", nFeeTXHash.ToString(), strError);

            if (nConf >= 1) vecImmatureFinalizedBudgets.push_back(finalizedBudgetBroadcast);
            return;
        }

        AddSeenFinalizedBudget(finalizedBudgetBroadcast);

        if (!finalizedBudgetBroadcast.UpdateValid(nCurrentHeight)) {
            LogPrint(BCLog::MNBUDGET,"fbs - invalid finalized budget - %s\n", finalizedBudgetBroadcast.IsInvalidReason());
            return;
        }

        LogPrint(BCLog::MNBUDGET,"fbs - new finalized budget - %s\n", nHash.ToString());

        CFinalizedBudget finalizedBudget(finalizedBudgetBroadcast);
        if (AddFinalizedBudget(finalizedBudget)) {
            finalizedBudgetBroadcast.Relay();
        }
        masternodeSync.AddedBudgetItem(nHash);

        //we might have active votes for this budget that are now valid
        CheckOrphanVotes();
    }

    if (strCommand == NetMsgType::FINALBUDGETVOTE) { //Finalized Budget Vote
        CFinalizedBudgetVote vote;
        vRecv >> vote;
        vote.SetValid(true);

        if (HaveSeenFinalizedBudgetVote(vote.GetHash())) {
            masternodeSync.AddedBudgetItem(vote.GetHash());
            return;
        }

        const CTxIn& voteVin = vote.GetVin();
        CMasternode* pmn = mnodeman.Find(voteVin);
        if (pmn == NULL) {
            LogPrint(BCLog::MNBUDGET, "fbvote - unknown masternode - vin: %s\n", voteVin.prevout.hash.ToString());
            mnodeman.AskForMN(pfrom, voteVin);
            return;
        }

        AddSeenFinalizedBudgetVote(vote);
        if (!vote.CheckSignature()) {
            if (masternodeSync.IsSynced()) {
                LogPrintf("fbvote - signature from masternode %s invalid\n", HexStr(pmn->pubKeyMasternode));
                LOCK(cs_main);
                Misbehaving(pfrom->GetId(), 20);
            }
            // it could just be a non-synced masternode
            mnodeman.AskForMN(pfrom, voteVin);
            return;
        }

        std::string strError = "";
        if (UpdateFinalizedBudget(vote, pfrom, strError)) {
            vote.Relay();
            masternodeSync.AddedBudgetItem(vote.GetHash());

            LogPrint(BCLog::MNBUDGET,"fbvote - new finalized budget vote - %s from masternode %s\n", vote.GetHash().ToString(), HexStr(pmn->pubKeyMasternode));
        } else {
            LogPrint(BCLog::MNBUDGET,"fbvote - rejected finalized budget vote - %s from masternode %s - %s\n", vote.GetHash().ToString(), HexStr(pmn->pubKeyMasternode), strError);
        }
    }
}

void CBudgetManager::SetSynced(bool synced)
{
    LOCK(cs);

    for (const auto& it: mapSeenMasternodeBudgetProposals) {
        CBudgetProposal* pbudgetProposal = FindProposal(it.first);
        if (pbudgetProposal && pbudgetProposal->IsValid()) {
            //mark votes
            pbudgetProposal->SetSynced(synced);
        }
    }

    for (const auto& it: mapSeenFinalizedBudgets) {
        CFinalizedBudget* pfinalizedBudget = FindFinalizedBudget(it.first);
        if (pfinalizedBudget && pfinalizedBudget->IsValid()) {
            //mark votes
            pfinalizedBudget->SetSynced(synced);
        }
    }
}

void CBudgetManager::Sync(CNode* pfrom, const uint256& nProp, bool fPartial)
{
    LOCK(cs);

    /*
        Sync with a client on the network

        --

        This code checks each of the hash maps for all known budget proposals and finalized budget proposals, then checks them against the
        budget object to see if they're OK. If all checks pass, we'll send it to the peer.

    */

    CNetMsgMaker msgMaker(pfrom->GetSendVersion());
    int nInvCount = 0;

    for (auto& it: mapSeenMasternodeBudgetProposals) {
        CBudgetProposal* pbudgetProposal = FindProposal(it.first);
        if (pbudgetProposal && pbudgetProposal->IsValid() && (nProp.IsNull() || it.first == nProp)) {
            pfrom->PushInventory(CInv(MSG_BUDGET_PROPOSAL, it.second.GetHash()));
            nInvCount++;
            pbudgetProposal->SyncVotes(pfrom, fPartial, nInvCount);
        }
    }
    g_connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::SYNCSTATUSCOUNT, MASTERNODE_SYNC_BUDGET_PROP, nInvCount));
    LogPrint(BCLog::MNBUDGET, "%s: sent %d items\n", __func__, nInvCount);

    nInvCount = 0;

    for (auto& it: mapSeenFinalizedBudgets) {
        CFinalizedBudget* pfinalizedBudget = FindFinalizedBudget(it.first);
        if (pfinalizedBudget && pfinalizedBudget->IsValid() && (nProp.IsNull() || it.first == nProp)) {
            pfrom->PushInventory(CInv(MSG_BUDGET_FINALIZED, it.second.GetHash()));
            nInvCount++;
            pfinalizedBudget->SyncVotes(pfrom, fPartial, nInvCount);
        }
    }
    g_connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::SYNCSTATUSCOUNT, MASTERNODE_SYNC_BUDGET_FIN, nInvCount));
    LogPrint(BCLog::MNBUDGET, "%s: sent %d items\n", __func__, nInvCount);
}

bool CBudgetManager::UpdateProposal(const CBudgetVote& vote, CNode* pfrom, std::string& strError)
{
    LOCK(cs);

    const uint256& nProposalHash = vote.GetProposalHash();
    if (!mapProposals.count(nProposalHash)) {
        if (pfrom) {
            // only ask for missing items after our syncing process is complete --
            //   otherwise we'll think a full sync succeeded when they return a result
            if (!masternodeSync.IsSynced()) return false;

            LogPrint(BCLog::MNBUDGET,"%s: Unknown proposal %d, asking for source proposal\n", __func__, nProposalHash.ToString());
            mapOrphanMasternodeBudgetVotes[nProposalHash] = vote;

            if (!askedForSourceProposalOrBudget.count(nProposalHash)) {
                g_connman->PushMessage(pfrom, CNetMsgMaker(pfrom->GetSendVersion()).Make(NetMsgType::BUDGETVOTESYNC, nProposalHash));
                askedForSourceProposalOrBudget[nProposalHash] = GetTime();
            }
        }

        strError = "Proposal not found!";
        return false;
    }


    return mapProposals[nProposalHash].AddOrUpdateVote(vote, strError);
}

bool CBudgetManager::UpdateFinalizedBudget(CFinalizedBudgetVote& vote, CNode* pfrom, std::string& strError)
{
    LOCK(cs);

    const uint256& nBudgetHash = vote.GetBudgetHash();
    if (!mapFinalizedBudgets.count(nBudgetHash)) {
        if (pfrom) {
            // only ask for missing items after our syncing process is complete --
            //   otherwise we'll think a full sync succeeded when they return a result
            if (!masternodeSync.IsSynced()) return false;

            LogPrint(BCLog::MNBUDGET,"%s: Unknown Finalized Proposal %s, asking for source budget\n", __func__, nBudgetHash.ToString());
            mapOrphanFinalizedBudgetVotes[nBudgetHash] = vote;

            if (!askedForSourceProposalOrBudget.count(nBudgetHash)) {
                g_connman->PushMessage(pfrom, CNetMsgMaker(pfrom->GetSendVersion()).Make(NetMsgType::BUDGETVOTESYNC, nBudgetHash));
                askedForSourceProposalOrBudget[nBudgetHash] = GetTime();
            }
        }

        strError = "Finalized Budget " + nBudgetHash.ToString() +  " not found!";
        return false;
    }
    LogPrint(BCLog::MNBUDGET,"%s: Finalized Proposal %s added\n", __func__, nBudgetHash.ToString());
    return mapFinalizedBudgets[nBudgetHash].AddOrUpdateVote(vote, strError);
}

CBudgetProposal::CBudgetProposal()
{
    strProposalName = "unknown";
    nBlockStart = 0;
    nBlockEnd = 0;
    nAmount = 0;
    nTime = 0;
    fValid = true;
    strInvalid = "";
}

CBudgetProposal::CBudgetProposal(std::string strProposalNameIn, std::string strURLIn, int nBlockStartIn, int nBlockEndIn, CScript addressIn, CAmount nAmountIn, uint256 nFeeTXHashIn)
{
    strProposalName = strProposalNameIn;
    strURL = strURLIn;
    nBlockStart = nBlockStartIn;
    nBlockEnd = nBlockEndIn;
    address = addressIn;
    nAmount = nAmountIn;
    nFeeTXHash = nFeeTXHashIn;
    fValid = true;
    strInvalid = "";
}

CBudgetProposal::CBudgetProposal(const CBudgetProposal& other)
{
    strProposalName = other.strProposalName;
    strURL = other.strURL;
    nBlockStart = other.nBlockStart;
    nBlockEnd = other.nBlockEnd;
    address = other.address;
    nAmount = other.nAmount;
    nTime = other.nTime;
    nFeeTXHash = other.nFeeTXHash;
    mapVotes = other.mapVotes;
    fValid = true;
    strInvalid = "";
}

void CBudgetProposal::SyncVotes(CNode* pfrom, bool fPartial, int& nInvCount) const
{
    LOCK(cs);
    for (const auto& it: mapVotes) {
        const CBudgetVote& vote = it.second;
        if (vote.IsValid() && (!fPartial || !vote.IsSynced())) {
            pfrom->PushInventory(CInv(MSG_BUDGET_VOTE, vote.GetHash()));
            nInvCount++;
        }
    }
}

bool CBudgetProposal::UpdateValid(int nCurrentHeight, bool fCheckCollateral)
{
    fValid = false;
    if (GetNays() - GetYeas() > mnodeman.CountEnabled(ActiveProtocol()) / 10) {
        strInvalid = "Proposal " + strProposalName + ": Active removal";
        return false;
    }

    if (nBlockStart < 0) {
        strInvalid = "Invalid Proposal";
        return false;
    }

    if (nBlockEnd < nBlockStart) {
        strInvalid = "Proposal " + strProposalName + ": Invalid nBlockEnd (end before start)";
        return false;
    }

    if (nAmount < 10 * COIN) {
        strInvalid = "Proposal " + strProposalName + ": Invalid nAmount";
        return false;
    }

    if (address == CScript()) {
        strInvalid = "Proposal " + strProposalName + ": Invalid Payment Address";
        return false;
    }

    if (fCheckCollateral) {
        int nConf = 0;
        std::string strError;
        if (!IsBudgetCollateralValid(nFeeTXHash, GetHash(), strError, nTime, nConf)) {
            strInvalid = "Proposal " + strProposalName + ": Invalid collateral (" + strError + ")";
            return false;
        }
    }

    /*
        TODO: There might be an issue with multisig in the coinbase on mainnet, we will add support for it in a future release.
    */
    if (address.IsPayToScriptHash()) {
        strInvalid = "Proposal " + strProposalName + ": Multisig is not currently supported.";
        return false;
    }

    //if proposal doesn't gain traction within 2 weeks, remove it
    // nTime not being saved correctly
    // -- TODO: We should keep track of the last time the proposal was valid, if it's invalid for 2 weeks, erase it
    // if(nTime + (60*60*24*2) < GetAdjustedTime()) {
    //     if(GetYeas()-GetNays() < (mnodeman.CountEnabled(ActiveProtocol())/10)) {
    //         strError = "Not enough support";
    //         return false;
    //     }
    // }

    //can only pay out 10% of the possible coins (min value of coins)
    if (nAmount > budget.GetTotalBudget(nBlockStart)) {
        strInvalid = "Proposal " + strProposalName + ": Payment more than max";
        return false;
    }

    // Calculate maximum block this proposal will be valid, which is start of proposal + (number of payments * cycle)
    int nProposalEnd = GetBlockStart() + (Params().GetConsensus().nBudgetCycleBlocks * GetTotalPaymentCount());

    if (nCurrentHeight <= 0) {
        strInvalid = "Proposal " + strProposalName + ": Tip is NULL";
        return true;
    }

    if(nProposalEnd < nCurrentHeight) {
        strInvalid = "Proposal " + strProposalName + ": Invalid nBlockEnd (" + std::to_string(nProposalEnd) + ") < current height (" + std::to_string(nCurrentHeight) + ")";
        return false;
    }

    fValid = true;
    strInvalid.clear();
    return true;
}

bool CBudgetProposal::IsEstablished() const
{
    return nTime < GetAdjustedTime() - Params().GetConsensus().nProposalEstablishmentTime;
}

bool CBudgetProposal::IsPassing(int nBlockStartBudget, int nBlockEndBudget, int mnCount) const
{
    if (!fValid)
        return false;

    if (this->nBlockStart > nBlockStartBudget)
        return false;

    if (this->nBlockEnd < nBlockEndBudget)
        return false;

    if (GetYeas() - GetNays() <= mnCount / 10)
        return false;

    if (!IsEstablished())
        return false;

    return true;
}

bool CBudgetProposal::AddOrUpdateVote(const CBudgetVote& vote, std::string& strError)
{
    std::string strAction = "New vote inserted:";
    LOCK(cs);

    const uint256& hash = vote.GetVin().prevout.GetHash();
    const int64_t voteTime = vote.GetTime();

    if (mapVotes.count(hash)) {
        const int64_t& oldTime = mapVotes[hash].GetTime();
        if (oldTime > voteTime) {
            strError = strprintf("new vote older than existing vote - %s\n", vote.GetHash().ToString());
            LogPrint(BCLog::MNBUDGET, "%s: %s\n", __func__, strError);
            return false;
        }
        if (voteTime - oldTime < BUDGET_VOTE_UPDATE_MIN) {
            strError = strprintf("time between votes is too soon - %s - %lli sec < %lli sec\n",
                    vote.GetHash().ToString(), voteTime - oldTime, BUDGET_VOTE_UPDATE_MIN);
            LogPrint(BCLog::MNBUDGET, "%s: %s\n", __func__, strError);
            return false;
        }
        strAction = "Existing vote updated:";
    }

    if (voteTime > GetTime() + (60 * 60)) {
        strError = strprintf("new vote is too far ahead of current time - %s - nTime %lli - Max Time %lli\n", vote.GetHash().ToString(), voteTime, GetTime() + (60 * 60));
        LogPrint(BCLog::MNBUDGET, "%s: %s\n", __func__, strError);
        return false;
    }

    mapVotes[hash] = vote;
    LogPrint(BCLog::MNBUDGET, "%s: %s %s\n", __func__, strAction.c_str(), vote.GetHash().ToString().c_str());

    return true;
}

UniValue CBudgetProposal::GetVotesArray() const
{
    LOCK(cs);
    UniValue ret(UniValue::VARR);
    for (const auto& it: mapVotes) {
        ret.push_back(it.second.ToJSON());
    }
    return ret;
}

void CBudgetProposal::SetSynced(bool synced)
{
    LOCK(cs);
    for (auto& it: mapVotes) {
        CBudgetVote& vote = it.second;
        if (synced) {
            if (vote.IsValid()) vote.SetSynced(true);
        } else {
            vote.SetSynced(false);
        }
    }
}

// If masternode voted for a proposal, but is now invalid -- remove the vote
void CBudgetProposal::CleanAndRemove()
{
    std::map<uint256, CBudgetVote>::iterator it = mapVotes.begin();

    while (it != mapVotes.end()) {
        CMasternode* pmn = mnodeman.Find((*it).second.GetVin());
        (*it).second.SetValid(pmn != nullptr);
        ++it;
    }
}

double CBudgetProposal::GetRatio() const
{
    int yeas = GetYeas();
    int nays = GetNays();

    if (yeas + nays == 0) return 0.0f;

    return ((double)(yeas) / (double)(yeas + nays));
}

int CBudgetProposal::GetVoteCount(CBudgetVote::VoteDirection vd) const
{
    LOCK(cs);
    int ret = 0;
    for (const auto& it : mapVotes) {
        const CBudgetVote& vote = it.second;
        if (vote.GetDirection() == vd && vote.IsValid())
            ret++;
    }
    return ret;
}

int CBudgetProposal::GetBlockStartCycle() const
{
    //end block is half way through the next cycle (so the proposal will be removed much after the payment is sent)
    return GetBlockCycle(nBlockStart);
}

int CBudgetProposal::GetBlockCycle(int nHeight)
{
    return nHeight - nHeight % Params().GetConsensus().nBudgetCycleBlocks;
}

int CBudgetProposal::GetBlockEndCycle() const
{
    // Right now single payment proposals have nBlockEnd have a cycle too early!
    // switch back if it break something else
    // end block is half way through the next cycle (so the proposal will be removed much after the payment is sent)
    // return nBlockEnd - GetBudgetPaymentCycleBlocks() / 2;

    // End block is half way through the next cycle (so the proposal will be removed much after the payment is sent)
    return nBlockEnd;

}

int CBudgetProposal::GetTotalPaymentCount() const
{
    return (GetBlockEndCycle() - GetBlockStartCycle()) / Params().GetConsensus().nBudgetCycleBlocks;
}

int CBudgetProposal::GetRemainingPaymentCount(int nCurrentHeight) const
{
    // If this budget starts in the future, this value will be wrong
    int nPayments = (GetBlockEndCycle() - GetBlockCycle(nCurrentHeight)) / Params().GetConsensus().nBudgetCycleBlocks - 1;
    // Take the lowest value
    return std::min(nPayments, GetTotalPaymentCount());
}

inline bool CBudgetProposal::PtrHigherYes(CBudgetProposal* a, CBudgetProposal* b)
{
    const int netYes_a = a->GetYeas() - a->GetNays();
    const int netYes_b = b->GetYeas() - b->GetNays();

    if (netYes_a == netYes_b) return a->GetFeeTXHash() > b->GetFeeTXHash();

    return netYes_a > netYes_b;
}

CBudgetProposalBroadcast::CBudgetProposalBroadcast(std::string strProposalNameIn, std::string strURLIn, int nPaymentCount, CScript addressIn, CAmount nAmountIn, int nBlockStartIn, uint256 nFeeTXHashIn)
{
    strProposalName = strProposalNameIn;
    strURL = strURLIn;

    nBlockStart = nBlockStartIn;

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int nCycleStart = nBlockStart - nBlockStart % nBlocksPerCycle;

    // Right now single payment proposals have nBlockEnd have a cycle too early!
    // switch back if it break something else
    // calculate the end of the cycle for this vote, add half a cycle (vote will be deleted after that block)
    // nBlockEnd = nCycleStart + GetBudgetPaymentCycleBlocks() * nPaymentCount + GetBudgetPaymentCycleBlocks() / 2;

    // Calculate the end of the cycle for this vote, vote will be deleted after next cycle
    nBlockEnd = nCycleStart + (nBlocksPerCycle + 1)  * nPaymentCount;

    address = addressIn;
    nAmount = nAmountIn;

    nFeeTXHash = nFeeTXHashIn;
}

void CBudgetProposalBroadcast::Relay()
{
    CInv inv(MSG_BUDGET_PROPOSAL, GetHash());
    g_connman->RelayInv(inv);
}

CBudgetVote::CBudgetVote() :
        CSignedMessage(),
        fValid(true),
        fSynced(false),
        nProposalHash(UINT256_ZERO),
        nVote(VOTE_ABSTAIN),
        nTime(0),
        vin()
{ }

CBudgetVote::CBudgetVote(CTxIn vinIn, uint256 nProposalHashIn, VoteDirection nVoteIn) :
        CSignedMessage(),
        fValid(true),
        fSynced(false),
        nProposalHash(nProposalHashIn),
        nVote(nVoteIn),
        vin(vinIn)
{
    nTime = GetAdjustedTime();
}

void CBudgetVote::Relay() const
{
    CInv inv(MSG_BUDGET_VOTE, GetHash());
    g_connman->RelayInv(inv);
}

uint256 CBudgetVote::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << vin;
    ss << nProposalHash;
    ss << (int) nVote;
    ss << nTime;
    return ss.GetHash();
}

std::string CBudgetVote::GetStrMessage() const
{
    return vin.prevout.ToStringShort() + nProposalHash.ToString() +
            std::to_string(nVote) + std::to_string(nTime);
}

UniValue CBudgetVote::ToJSON() const
{
    UniValue bObj(UniValue::VOBJ);
    bObj.push_back(Pair("mnId", vin.prevout.hash.ToString()));
    bObj.push_back(Pair("nHash", vin.prevout.GetHash().ToString()));
    bObj.push_back(Pair("Vote", GetVoteString()));
    bObj.push_back(Pair("nTime", nTime));
    bObj.push_back(Pair("fValid", fValid));
    return bObj;
}

CFinalizedBudget::CFinalizedBudget() :
        fAutoChecked(false),
        fValid(true),
        strInvalid(),
        mapVotes(),
        strBudgetName(""),
        nBlockStart(0),
        vecBudgetPayments(),
        nFeeTXHash(),
        nTime(0)
{ }

CFinalizedBudget::CFinalizedBudget(const CFinalizedBudget& other) :
        fAutoChecked(false),
        fValid(true),
        strInvalid(),
        mapVotes(other.mapVotes),
        strBudgetName(other.strBudgetName),
        nBlockStart(other.nBlockStart),
        vecBudgetPayments(other.vecBudgetPayments),
        nFeeTXHash(other.nFeeTXHash),
        nTime(other.nTime)
{ }

bool CFinalizedBudget::AddOrUpdateVote(const CFinalizedBudgetVote& vote, std::string& strError)
{
    LOCK(cs);

    const uint256& hash = vote.GetVin().prevout.GetHash();
    const int64_t voteTime = vote.GetTime();
    std::string strAction = "New vote inserted:";

    if (mapVotes.count(hash)) {
        const int64_t oldTime = mapVotes[hash].GetTime();
        if (oldTime > voteTime) {
            strError = strprintf("new vote older than existing vote - %s\n", vote.GetHash().ToString());
            LogPrint(BCLog::MNBUDGET, "%s: %s\n", __func__, strError);
            return false;
        }
        if (voteTime - oldTime < BUDGET_VOTE_UPDATE_MIN) {
            strError = strprintf("time between votes is too soon - %s - %lli sec < %lli sec\n",
                    vote.GetHash().ToString(), voteTime - oldTime, BUDGET_VOTE_UPDATE_MIN);
            LogPrint(BCLog::MNBUDGET, "%s: %s\n", __func__, strError);
            return false;
        }
        strAction = "Existing vote updated:";
    }

    if (voteTime > GetTime() + (60 * 60)) {
        strError = strprintf("new vote is too far ahead of current time - %s - nTime %lli - Max Time %lli\n",
                vote.GetHash().ToString(), voteTime, GetTime() + (60 * 60));
        LogPrint(BCLog::MNBUDGET, "%s: %s\n", __func__, strError);
        return false;
    }

    mapVotes[hash] = vote;
    LogPrint(BCLog::MNBUDGET, "%s: %s %s\n", __func__, strAction.c_str(), vote.GetHash().ToString().c_str());
    return true;
}

UniValue CFinalizedBudget::GetVotesObject() const
{
    LOCK(cs);
    UniValue ret(UniValue::VOBJ);
    for (const auto& it: mapVotes) {
        const CFinalizedBudgetVote& vote = it.second;
        ret.push_back(std::make_pair(vote.GetVin().prevout.ToStringShort(), vote.ToJSON()));
    }
    return ret;
}

void CFinalizedBudget::SetSynced(bool synced)
{
    LOCK(cs);
    for (auto& it: mapVotes) {
        CFinalizedBudgetVote& vote = it.second;
        if (synced) {
            if (vote.IsValid()) vote.SetSynced(true);
        } else {
            vote.SetSynced(false);
        }
    }
}

// Sort budget proposals by hash
struct sortProposalsByHash  {
    bool operator()(const CBudgetProposal* left, const CBudgetProposal* right)
    {
        return (left->GetHash() < right->GetHash());
    }
};

// Check finalized budget and vote on it if correct. Masternodes only
void CFinalizedBudget::CheckAndVote()
{
    if (!fMasterNode || fAutoChecked) {
        LogPrint(BCLog::MNBUDGET,"%s: fMasterNode=%d fAutoChecked=%d\n", __func__, fMasterNode, fAutoChecked);
        return;
    }

    if (activeMasternode.vin == nullopt) {
        LogPrint(BCLog::MNBUDGET,"%s: Active Masternode not initialized.\n", __func__);
        return;
    }

    // Do this 1 in 4 blocks -- spread out the voting activity
    // -- this function is only called every fourteenth block, so this is really 1 in 56 blocks
    if (rand() % 4 != 0) {
        LogPrint(BCLog::MNBUDGET,"%s: waiting\n", __func__);
        return;
    }

    fAutoChecked = true; //we only need to check this once

    if (strBudgetMode == "auto") //only vote for exact matches
    {
        LOCK(cs);
        std::vector<CBudgetProposal*> vBudgetProposals = budget.GetBudget();

        // We have to resort the proposals by hash (they are sorted by votes here) and sort the payments
        // by hash (they are not sorted at all) to make the following tests deterministic
        // We're working on copies to avoid any side-effects by the possibly changed sorting order

        // Sort copy of proposals by hash (descending)
        std::vector<CBudgetProposal*> vBudgetProposalsSortedByHash(vBudgetProposals);
        std::sort(vBudgetProposalsSortedByHash.begin(), vBudgetProposalsSortedByHash.end(), CBudgetProposal::PtrGreater);

        // Sort copy payments by hash (descending)
        std::vector<CTxBudgetPayment> vecBudgetPaymentsSortedByHash(vecBudgetPayments);
        std::sort(vecBudgetPaymentsSortedByHash.begin(), vecBudgetPaymentsSortedByHash.end(), std::greater<CTxBudgetPayment>());

        for (unsigned int i = 0; i < vecBudgetPaymentsSortedByHash.size(); i++) {
            LogPrint(BCLog::MNBUDGET,"%s: Budget-Payments - nProp %d %s\n", __func__, i, vecBudgetPaymentsSortedByHash[i].nProposalHash.ToString());
            LogPrint(BCLog::MNBUDGET,"%s: Budget-Payments - Payee %d %s\n", __func__, i, HexStr(vecBudgetPaymentsSortedByHash[i].payee));
            LogPrint(BCLog::MNBUDGET,"%s: Budget-Payments - nAmount %d %lli\n", __func__, i, vecBudgetPaymentsSortedByHash[i].nAmount);
        }

        for (unsigned int i = 0; i < vBudgetProposalsSortedByHash.size(); i++) {
            LogPrint(BCLog::MNBUDGET,"%s: Budget-Proposals - nProp %d %s\n", __func__, i, vBudgetProposalsSortedByHash[i]->GetHash().ToString());
            LogPrint(BCLog::MNBUDGET,"%s: Budget-Proposals - Payee %d %s\n", __func__, i, HexStr(vBudgetProposalsSortedByHash[i]->GetPayee()));
            LogPrint(BCLog::MNBUDGET,"%s: Budget-Proposals - nAmount %d %lli\n", __func__, i, vBudgetProposalsSortedByHash[i]->GetAmount());
        }

        if (vBudgetProposalsSortedByHash.size() == 0) {
            LogPrint(BCLog::MNBUDGET,"%s: No Budget-Proposals found, aborting\n", __func__);
            return;
        }

        if (vBudgetProposalsSortedByHash.size() != vecBudgetPaymentsSortedByHash.size()) {
            LogPrint(BCLog::MNBUDGET,"%s: Budget-Proposal length (%ld) doesn't match Budget-Payment length (%ld).\n", __func__,
                      vBudgetProposalsSortedByHash.size(), vecBudgetPaymentsSortedByHash.size());
            return;
        }

        for (unsigned int i = 0; i < vecBudgetPaymentsSortedByHash.size(); i++) {
            if (i > vBudgetProposalsSortedByHash.size() - 1) {
                LogPrint(BCLog::MNBUDGET,"%s: Proposal size mismatch, i=%d > (vBudgetProposals.size() - 1)=%d\n",
                        __func__, i, vBudgetProposalsSortedByHash.size() - 1);
                return;
            }

            if (vecBudgetPaymentsSortedByHash[i].nProposalHash != vBudgetProposalsSortedByHash[i]->GetHash()) {
                LogPrint(BCLog::MNBUDGET,"%s: item #%d doesn't match %s %s\n", __func__,
                        i, vecBudgetPaymentsSortedByHash[i].nProposalHash.ToString(), vBudgetProposalsSortedByHash[i]->GetHash().ToString());
                return;
            }

            // if(vecBudgetPayments[i].payee != vBudgetProposals[i]->GetPayee()){ -- triggered with false positive
            if (HexStr(vecBudgetPaymentsSortedByHash[i].payee) != HexStr(vBudgetProposalsSortedByHash[i]->GetPayee())) {
                LogPrint(BCLog::MNBUDGET,"%s: item #%d payee doesn't match %s %s\n", __func__,
                        i, HexStr(vecBudgetPaymentsSortedByHash[i].payee), HexStr(vBudgetProposalsSortedByHash[i]->GetPayee()));
                return;
            }

            if (vecBudgetPaymentsSortedByHash[i].nAmount != vBudgetProposalsSortedByHash[i]->GetAmount()) {
                LogPrint(BCLog::MNBUDGET,"%s: item #%d payee doesn't match %lli %lli\n", __func__,
                        i, vecBudgetPaymentsSortedByHash[i].nAmount, vBudgetProposalsSortedByHash[i]->GetAmount());
                return;
            }
        }

        LogPrint(BCLog::MNBUDGET,"%s: Finalized Budget Matches! Submitting Vote.\n", __func__);
        SubmitVote();
    }
}

// Remove votes from masternodes which are not valid/existent anymore
void CFinalizedBudget::CleanAndRemove()
{
    std::map<uint256, CFinalizedBudgetVote>::iterator it = mapVotes.begin();

    while (it != mapVotes.end()) {
        CMasternode* pmn = mnodeman.Find((*it).second.GetVin());
        (*it).second.SetValid(pmn != nullptr);
        ++it;
    }
}

CAmount CFinalizedBudget::GetTotalPayout() const
{
    CAmount ret = 0;

    for (auto & vecBudgetPayment : vecBudgetPayments) {
        ret += vecBudgetPayment.nAmount;
    }

    return ret;
}

std::string CFinalizedBudget::GetProposals()
{
    LOCK(cs);
    std::string ret = "";

    for (CTxBudgetPayment& budgetPayment : vecBudgetPayments) {
        CBudgetProposal* pbudgetProposal = budget.FindProposal(budgetPayment.nProposalHash);

        std::string token = budgetPayment.nProposalHash.ToString();

        if (pbudgetProposal) token = pbudgetProposal->GetName();
        if (ret == "") {
            ret = token;
        } else {
            ret += "," + token;
        }
    }
    return ret;
}

std::string CFinalizedBudget::GetStatus() const
{
    std::string retBadHashes = "";
    std::string retBadPayeeOrAmount = "";

    for (int nBlockHeight = GetBlockStart(); nBlockHeight <= GetBlockEnd(); nBlockHeight++) {
        CTxBudgetPayment budgetPayment;
        if (!GetBudgetPaymentByBlock(nBlockHeight, budgetPayment)) {
            LogPrint(BCLog::MNBUDGET,"%s: Couldn't find budget payment for block %lld\n", __func__, nBlockHeight);
            continue;
        }

        const CBudgetProposal* pbudgetProposal = budget.FindProposal(budgetPayment.nProposalHash);
        if (!pbudgetProposal) {
            if (retBadHashes == "") {
                retBadHashes = "Unknown proposal hash! Check this proposal before voting: " + budgetPayment.nProposalHash.ToString();
            } else {
                retBadHashes += "," + budgetPayment.nProposalHash.ToString();
            }
        } else {
            if (pbudgetProposal->GetPayee() != budgetPayment.payee || pbudgetProposal->GetAmount() != budgetPayment.nAmount) {
                if (retBadPayeeOrAmount == "") {
                    retBadPayeeOrAmount = "Budget payee/nAmount doesn't match our proposal! " + budgetPayment.nProposalHash.ToString();
                } else {
                    retBadPayeeOrAmount += "," + budgetPayment.nProposalHash.ToString();
                }
            }
        }
    }

    if (retBadHashes == "" && retBadPayeeOrAmount == "") return "OK";

    return retBadHashes + retBadPayeeOrAmount;
}

void CFinalizedBudget::SyncVotes(CNode* pfrom, bool fPartial, int& nInvCount) const
{
    LOCK(cs);
    for (const auto& it: mapVotes) {
        const CFinalizedBudgetVote& vote = it.second;
        if (vote.IsValid() && (!fPartial || !vote.IsSynced())) {
            pfrom->PushInventory(CInv(MSG_BUDGET_FINALIZED_VOTE, vote.GetHash()));
            nInvCount++;
        }
    }
}

bool CFinalizedBudget::UpdateValid(int nCurrentHeight, bool fCheckCollateral)
{
    fValid = false;
    // All(!) finalized budgets have the name "main", so get some additional information about them
    std::string strProposals = GetProposals();

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    // Must be the correct block for payment to happen (once a month)
    if (nBlockStart % nBlocksPerCycle != 0) {
        strInvalid = "Invalid BlockStart";
        return false;
    }

    // The following 2 checks check the same (basically if vecBudgetPayments.size() > 100)
    if (GetBlockEnd() - nBlockStart > 100) {
        strInvalid = "Invalid BlockEnd";
        return false;
    }
    if ((int)vecBudgetPayments.size() > 100) {
        strInvalid = "Invalid budget payments count (too many)";
        return false;
    }
    if (strBudgetName == "") {
        strInvalid = "Invalid Budget Name";
        return false;
    }
    if (nBlockStart == 0) {
        strInvalid = "Budget " + strBudgetName + " (" + strProposals + ") Invalid BlockStart == 0";
        return false;
    }
    if (nFeeTXHash.IsNull()) {
        strInvalid = "Budget " + strBudgetName  + " (" + strProposals + ") Invalid FeeTx == 0";
        return false;
    }

    // Can only pay out 10% of the possible coins (min value of coins)
    if (GetTotalPayout() > budget.GetTotalBudget(nBlockStart)) {
        strInvalid = "Budget " + strBudgetName + " (" + strProposals + ") Invalid Payout (more than max)";
        return false;
    }

    std::string strError2 = "";
    if (fCheckCollateral) {
        int nConf = 0;
        if (!IsBudgetCollateralValid(nFeeTXHash, GetHash(), strError2, nTime, nConf, true)) {
            {
                strInvalid = "Budget " + strBudgetName + " (" + strProposals + ") Invalid Collateral : " + strError2;
                return false;
            }
        }
    }

    // Remove obsolete finalized budgets after some time
    int nBlockStart = nCurrentHeight - nCurrentHeight % nBlocksPerCycle + nBlocksPerCycle;

    // Remove budgets where the last payment (from max. 100) ends before 2 budget-cycles before the current one
    int nMaxAge = nBlockStart - (2 * nBlocksPerCycle);

    if (GetBlockEnd() < nMaxAge) {
        strInvalid = strprintf("Budget " + strBudgetName + " (" + strProposals + ") (ends at block %ld) too old and obsolete", GetBlockEnd());
        return false;
    }

    fValid = true;
    strInvalid.clear();
    return true;
}

bool CFinalizedBudget::IsPaidAlready(uint256 nProposalHash, int nBlockHeight) const
{
    // Remove budget-payments from former/future payment cycles
    std::map<uint256, int>::iterator it = mapPayment_History.begin();
    int nPaidBlockHeight = 0;
    uint256 nOldProposalHash;

    for(it = mapPayment_History.begin(); it != mapPayment_History.end(); /* No incrementation needed */ ) {
        nPaidBlockHeight = (*it).second;
        if((nPaidBlockHeight < GetBlockStart()) || (nPaidBlockHeight > GetBlockEnd())) {
            nOldProposalHash = (*it).first;
            LogPrint(BCLog::MNBUDGET, "%s: Budget Proposal %s, Block %d from old cycle deleted\n",
                    __func__, nOldProposalHash.ToString().c_str(), nPaidBlockHeight);
            mapPayment_History.erase(it++);
        }
        else {
            ++it;
        }
    }

    // Now that we only have payments from the current payment cycle check if this budget was paid already
    if(mapPayment_History.count(nProposalHash) == 0) {
        // New proposal payment, insert into map for checks with later blocks from this cycle
        mapPayment_History.insert(std::pair<uint256, int>(nProposalHash, nBlockHeight));
        LogPrint(BCLog::MNBUDGET, "%s: Budget Proposal %s, Block %d added to payment history\n",
                __func__, nProposalHash.ToString().c_str(), nBlockHeight);
        return false;
    }
    // This budget was paid already -> reject transaction so it gets paid to a masternode instead
    return true;
}

TrxValidationStatus CFinalizedBudget::IsTransactionValid(const CTransaction& txNew, int nBlockHeight) const
{
    TrxValidationStatus transactionStatus = TrxValidationStatus::InValid;
    int nCurrentBudgetPayment = nBlockHeight - GetBlockStart();
    if (nCurrentBudgetPayment < 0) {
        LogPrint(BCLog::MNBUDGET,"%s: Invalid block - height: %d start: %d\n", __func__, nBlockHeight, GetBlockStart());
        return TrxValidationStatus::InValid;
    }

    if (nCurrentBudgetPayment > (int)vecBudgetPayments.size() - 1) {
        LogPrint(BCLog::MNBUDGET,"%s: Invalid last block - current budget payment: %d of %d\n",
                __func__, nCurrentBudgetPayment + 1, (int)vecBudgetPayments.size());
        return TrxValidationStatus::InValid;
    }

    bool paid = false;

    for (const CTxOut& out : txNew.vout) {
        LogPrint(BCLog::MNBUDGET,"%s: nCurrentBudgetPayment=%d, payee=%s == out.scriptPubKey=%s, amount=%ld == out.nValue=%ld\n",
                __func__, nCurrentBudgetPayment, HexStr(vecBudgetPayments[nCurrentBudgetPayment].payee), HexStr(out.scriptPubKey),
                vecBudgetPayments[nCurrentBudgetPayment].nAmount, out.nValue);

        if (vecBudgetPayments[nCurrentBudgetPayment].payee == out.scriptPubKey && vecBudgetPayments[nCurrentBudgetPayment].nAmount == out.nValue) {
            // Check if this proposal was paid already. If so, pay a masternode instead
            paid = IsPaidAlready(vecBudgetPayments[nCurrentBudgetPayment].nProposalHash, nBlockHeight);
            if(paid) {
                LogPrint(BCLog::MNBUDGET,"%s: Double Budget Payment of %d for proposal %d detected. Paying a masternode instead.\n",
                        __func__, vecBudgetPayments[nCurrentBudgetPayment].nAmount, vecBudgetPayments[nCurrentBudgetPayment].nProposalHash.GetHex());
                // No matter what we've found before, stop all checks here. In future releases there might be more than one budget payment
                // per block, so even if the first one was not paid yet this one disables all budget payments for this block.
                transactionStatus = TrxValidationStatus::DoublePayment;
                break;
            }
            else {
                transactionStatus = TrxValidationStatus::Valid;
                LogPrint(BCLog::MNBUDGET,"%s: Found valid Budget Payment of %d for proposal %d\n", __func__,
                          vecBudgetPayments[nCurrentBudgetPayment].nAmount, vecBudgetPayments[nCurrentBudgetPayment].nProposalHash.GetHex());
            }
        }
    }

    if (transactionStatus == TrxValidationStatus::InValid) {
        CTxDestination address1;
        ExtractDestination(vecBudgetPayments[nCurrentBudgetPayment].payee, address1);

        LogPrint(BCLog::MNBUDGET,"%s: Missing required payment - %s: %d c: %d\n", __func__,
                  EncodeDestination(address1), vecBudgetPayments[nCurrentBudgetPayment].nAmount, nCurrentBudgetPayment);
    }

    return transactionStatus;
}

bool CFinalizedBudget::GetBudgetPaymentByBlock(int64_t nBlockHeight, CTxBudgetPayment& payment) const
{
    LOCK(cs);

    int i = nBlockHeight - GetBlockStart();
    if (i < 0) return false;
    if (i > (int)vecBudgetPayments.size() - 1) return false;
    payment = vecBudgetPayments[i];
    return true;
}

bool CFinalizedBudget::GetPayeeAndAmount(int64_t nBlockHeight, CScript& payee, CAmount& nAmount) const
{
    LOCK(cs);

    int i = nBlockHeight - GetBlockStart();
    if (i < 0) return false;
    if (i > (int)vecBudgetPayments.size() - 1) return false;
    payee = vecBudgetPayments[i].payee;
    nAmount = vecBudgetPayments[i].nAmount;
    return true;
}

void CFinalizedBudget::SubmitVote()
{
    // function called only from initialized masternodes
    assert(fMasterNode && activeMasternode.vin != nullopt);

    std::string strError = "";
    CPubKey pubKeyMasternode;
    CKey keyMasternode;

    if (!CMessageSigner::GetKeysFromSecret(strMasterNodePrivKey, keyMasternode, pubKeyMasternode)) {
        LogPrint(BCLog::MNBUDGET,"%s: Error upon calling GetKeysFromSecret\n", __func__);
        return;
    }

    CFinalizedBudgetVote vote(*(activeMasternode.vin), GetHash());
    if (!vote.Sign(keyMasternode, pubKeyMasternode)) {
        LogPrint(BCLog::MNBUDGET,"%s: Failure to sign.", __func__);
        return;
    }

    if (budget.UpdateFinalizedBudget(vote, NULL, strError)) {
        LogPrint(BCLog::MNBUDGET,"%s: new finalized budget vote - %s\n", __func__, vote.GetHash().ToString());

        budget.AddSeenFinalizedBudgetVote(vote);
        vote.Relay();
    } else {
        LogPrint(BCLog::MNBUDGET,"%s: Error submitting vote - %s\n", __func__, strError);
    }
}

bool CFinalizedBudget::operator>(const CFinalizedBudget& other) const
{
    const int count = GetVoteCount();
    const int otherCount = other.GetVoteCount();

    if (count == otherCount) return GetFeeTXHash() > other.GetFeeTXHash();

    return count > otherCount;
}

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast() :
        CFinalizedBudget()
{ }

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast(const CFinalizedBudget& other) :
        CFinalizedBudget(other)
{ }

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast(std::string strBudgetNameIn,
                                                     int nBlockStartIn,
                                                     const std::vector<CTxBudgetPayment>& vecBudgetPaymentsIn,
                                                     uint256 nFeeTXHashIn)
{
    strBudgetName = strBudgetNameIn;
    nBlockStart = nBlockStartIn;
    for (const CTxBudgetPayment& out : vecBudgetPaymentsIn)
        vecBudgetPayments.push_back(out);
    nFeeTXHash = nFeeTXHashIn;
}

void CFinalizedBudgetBroadcast::Relay()
{
    CInv inv(MSG_BUDGET_FINALIZED, GetHash());
    g_connman->RelayInv(inv);
}

CFinalizedBudgetVote::CFinalizedBudgetVote() :
        CSignedMessage(),
        fValid(true),
        fSynced(false),
        vin(),
        nBudgetHash(),
        nTime(0)
{ }

CFinalizedBudgetVote::CFinalizedBudgetVote(CTxIn vinIn, uint256 nBudgetHashIn) :
        CSignedMessage(),
        fValid(true),
        fSynced(false),
        vin(vinIn),
        nBudgetHash(nBudgetHashIn)
{
    nTime = GetAdjustedTime();
}

void CFinalizedBudgetVote::Relay() const
{
    CInv inv(MSG_BUDGET_FINALIZED_VOTE, GetHash());
    g_connman->RelayInv(inv);
}

uint256 CFinalizedBudgetVote::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << vin;
    ss << nBudgetHash;
    ss << nTime;
    return ss.GetHash();
}

UniValue CFinalizedBudgetVote::ToJSON() const
{
    UniValue bObj(UniValue::VOBJ);
    bObj.push_back(Pair("nHash", vin.prevout.GetHash().ToString()));
    bObj.push_back(Pair("nTime", (int64_t) nTime));
    bObj.push_back(Pair("fValid", fValid));
    return bObj;
}

std::string CFinalizedBudgetVote::GetStrMessage() const
{
    return vin.prevout.ToStringShort() + nBudgetHash.ToString() + std::to_string(nTime);
}

std::string CBudgetManager::ToString() const
{
    std::ostringstream info;

    info << "Proposals: " << (int)mapProposals.size() << ", Budgets: " << (int)mapFinalizedBudgets.size() << ", Seen Budgets: " << (int)mapSeenMasternodeBudgetProposals.size() << ", Seen Budget Votes: " << (int)mapSeenMasternodeBudgetVotes.size() << ", Seen Final Budgets: " << (int)mapSeenFinalizedBudgets.size() << ", Seen Final Budget Votes: " << (int)mapSeenFinalizedBudgetVotes.size();

    return info.str();
}
