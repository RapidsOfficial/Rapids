// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2013-2014 The NovaCoin Developers
// Copyright (c) 2014-2018 The BlackCoin Developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>

#include "db.h"
#include "kernel.h"
#include "script/interpreter.h"
#include "util.h"
#include "stakeinput.h"
#include "utilmoneystr.h"
#include "zpivchain.h"
#include "zpiv/zpos.h"

/**
 * CStakeKernel Constructor
 *
 * @param[in]   pindexPrev      index of the parent of the kernel block
 * @param[in]   stakeInput      input for the coinstake of the kernel block
 * @param[in]   nBits           target difficulty bits of the kernel block
 * @param[in]   nTimeTx         time of the kernel block
 */
CStakeKernel::CStakeKernel(const CBlockIndex* const pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int nTimeTx):
    stakeUniqueness(stakeInput->GetUniqueness()),
    nTime(nTimeTx),
    nBits(nBits),
    stakeValue(stakeInput->GetValue())
{
    if (!Params().GetConsensus().IsStakeModifierV2(pindexPrev->nHeight + 1)) { // Modifier v1
        uint64_t nStakeModifier = 0;
        GetOldStakeModifier(stakeInput, nStakeModifier);
        stakeModifier << nStakeModifier;
    } else { // Modifier v2
        stakeModifier << pindexPrev->GetStakeModifierV2();
    }
    CBlockIndex* pindexFrom = stakeInput->GetIndexFrom();
    nTimeBlockFrom = pindexFrom->nTime;
}

// Return stake kernel hash
uint256 CStakeKernel::GetHash() const
{
    CDataStream ss(stakeModifier);
    ss << nTimeBlockFrom << stakeUniqueness << nTime;
    return Hash(ss.begin(), ss.end());
}

// Check that the kernel hash meets the target required
bool CStakeKernel::CheckKernelHash(bool fSkipLog) const
{
    // Get weighted target
    uint256 bnTarget;
    bnTarget.SetCompact(nBits);
    bnTarget *= (uint256(stakeValue) / 100);

    // Check PoS kernel hash
    const uint256& hashProofOfStake = GetHash();
    const bool res = hashProofOfStake < bnTarget;

    if (!fSkipLog || res) {
        LogPrint("staking", "%s : Proof Of Stake:"
                            "\nssUniqueID=%s"
                            "\nnTimeTx=%d"
                            "\nhashProofOfStake=%s"
                            "\nnBits=%d"
                            "\nweight=%d"
                            "\nbnTarget=%s (res: %d)\n\n",
            __func__, HexStr(stakeUniqueness), nTime, hashProofOfStake.GetHex(),
            nBits, stakeValue, bnTarget.GetHex(), res);
    }
    return res;
}


/*
 * PoS Validation
 */

// helper function for CheckProofOfStake and GetStakeKernelHash
bool LoadStakeInput(const CBlock& block, const CBlockIndex* pindexPrev, std::unique_ptr<CStakeInput>& stake)
{
    // If previous index is not provided, look for it in the blockmap
    if (!pindexPrev) {
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi != mapBlockIndex.end() && (*mi).second) pindexPrev = (*mi).second;
        else return error("%s : couldn't find previous block", __func__);
    } else {
        // check that is the actual parent block
        if (block.hashPrevBlock != pindexPrev->GetBlockHash())
            return error("%s : previous block mismatch");
    }

    // Check that this is a PoS block
    if (!block.IsProofOfStake())
        return error("called on non PoS block");

    // Construct the stakeinput object
    const CTxIn& txin = block.vtx[1].vin[0];
    stake = txin.IsZerocoinSpend() ?
            std::unique_ptr<CStakeInput>(new CLegacyZPivStake()) :
            std::unique_ptr<CStakeInput>(new CPivStake());

    return stake->InitFromTxIn(txin);
}

/*
 * Stake                Check if stakeInput can stake a block on top of pindexPrev
 *
 * @param[in]   pindexPrev      index of the parent block of the block being staked
 * @param[in]   stakeInput      input for the coinstake
 * @param[in]   nBits           target difficulty bits
 * @param[in]   nTimeTx         new blocktime
 * @return      bool            true if stake kernel hash meets target protocol
 */
bool Stake(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int64_t& nTimeTx)
{
    // Double check stake input contextual checks
    const int nHeightTx = pindexPrev->nHeight + 1;
    if (!stakeInput || !stakeInput->ContextCheck(nHeightTx, nTimeTx)) return false;

    // Get the new time slot (and verify it's not the same as previous block)
    const bool fRegTest = Params().IsRegTestNet();
    nTimeTx = (fRegTest ? GetAdjustedTime() : GetCurrentTimeSlot());
    if (nTimeTx <= pindexPrev->nTime && !fRegTest) return false;

    // Verify Proof Of Stake
    CStakeKernel stakeKernel(pindexPrev, stakeInput, nBits, nTimeTx);
    return stakeKernel.CheckKernelHash(true);
}


/*
 * CheckProofOfStake    Check if block has valid proof of stake
 *
 * @param[in]   block           block being verified
 * @param[out]  strError        string error (if any, else empty)
 * @param[in]   pindexPrev      index of the parent block
 *                              (if nullptr, it will be searched in mapBlockIndex)
 * @return      bool            true if the block has a valid proof of stake
 */
bool CheckProofOfStake(const CBlock& block, std::string& strError, const CBlockIndex* pindexPrev)
{
    // Initialize stake input
    std::unique_ptr<CStakeInput> stakeInput;
    if (!LoadStakeInput(block, pindexPrev, stakeInput)) {
        strError = "stake input initialization failed";
        return false;
    }

    // Stake input contextual checks
    if (!stakeInput->ContextCheck(pindexPrev->nHeight + 1, block.nTime)) {
        strError = "stake input failing contextual checks";
        return false;
    }

    // Verify signature
    if (!stakeInput->IsZPIV()) {
        CTransaction txPrev;
        if (!stakeInput->GetTxFrom(txPrev)) {
            strError = "unable to get txPrev for coinstake";
            return false;
        }
        const CTransaction& tx = block.vtx[1];
        const CTxIn& txin = tx.vin[0];
        ScriptError serror;
        if (!VerifyScript(txin.scriptSig, txPrev.vout[txin.prevout.n].scriptPubKey, STANDARD_SCRIPT_VERIFY_FLAGS,
                 TransactionSignatureChecker(&tx, 0), &serror)) {
            strError = strprintf("signature fails: %s", serror ? ScriptErrorString(serror) : "");
            return false;
        }
    }

    // Verify Proof Of Stake
    CStakeKernel stakeKernel(pindexPrev, stakeInput.get(), block.nBits, block.nTime);
    if (!stakeKernel.CheckKernelHash()) {
        strError = "kernel hash check fails";
        return false;
    }

    // All good
    return true;
}


/*
 * GetStakeKernelHash   Return stake kernel of a block
 *
 * @param[out]  hashRet         hash of the kernel (set by this function)
 * @param[in]   block           block with the kernel to return
 * @param[in]   pindexPrev      index of the parent block
 *                              (if nullptr, it will be searched in mapBlockIndex)
 * @return      bool            false if kernel cannot be initialized, true otherwise
 */
bool GetStakeKernelHash(uint256& hashRet, const CBlock& block, const CBlockIndex* pindexPrev)
{
    // Initialize stake input
    std::unique_ptr<CStakeInput> stakeInput;
    if (!LoadStakeInput(block, pindexPrev, stakeInput))
        return error("%s : stake input initialization failed", __func__);

    CStakeKernel stakeKernel(pindexPrev, stakeInput.get(), block.nBits, block.nTime);
    hashRet = stakeKernel.GetHash();
    return true;
}



/*
 * OLD MODIFIER
 */
static const unsigned int MODIFIER_INTERVAL = 60;
static const int MODIFIER_INTERVAL_RATIO = 3;
static const int64_t OLD_MODIFIER_INTERVAL = 2087;


// Get selection interval section (in seconds)
static int64_t GetStakeModifierSelectionIntervalSection(int nSection)
{
    assert(nSection >= 0 && nSection < 64);
    int64_t a = MODIFIER_INTERVAL  * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1)));
    return a;
}

// select a block from the candidate blocks in vSortedByTimestamp, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionIntervalStop.
static bool SelectBlockFromCandidates(
    std::vector<std::pair<int64_t, uint256> >& vSortedByTimestamp,
    std::map<uint256, const CBlockIndex*>& mapSelectedBlocks,
    int64_t nSelectionIntervalStop,
    uint64_t nStakeModifierPrev,
    const CBlockIndex** pindexSelected)
{
    bool fModifierV2 = false;
    bool fFirstRun = true;
    bool fSelected = false;
    uint256 hashBest;
    *pindexSelected = (const CBlockIndex*)0;
    for (const PAIRTYPE(int64_t, uint256) & item : vSortedByTimestamp) {
        if (!mapBlockIndex.count(item.second))
            return error("%s : failed to find block index for candidate block %s", __func__, item.second.ToString().c_str());

        const CBlockIndex* pindex = mapBlockIndex[item.second];
        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;

        //if the lowest block height (vSortedByTimestamp[0]) is >= switch height, use new modifier calc
        if (fFirstRun){
            fModifierV2 = pindex->nHeight >= Params().GetConsensus().height_start_StakeModifierNewSelection;
            fFirstRun = false;
        }

        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
            continue;

        // compute the selection hash by hashing an input that is unique to that block
        uint256 hashProof;
        if(fModifierV2)
            hashProof = pindex->GetBlockHash();
        else
            hashProof = pindex->IsProofOfStake() ? UINT256_ZERO : pindex->GetBlockHash();

        CDataStream ss(SER_GETHASH, 0);
        ss << hashProof << nStakeModifierPrev;
        uint256 hashSelection = Hash(ss.begin(), ss.end());

        // the selection hash is divided by 2**32 so that proof-of-stake block
        // is always favored over proof-of-work block. this is to preserve
        // the energy efficiency property
        if (pindex->IsProofOfStake())
            hashSelection >>= 32;

        if (fSelected && hashSelection < hashBest) {
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*)pindex;
        } else if (!fSelected) {
            fSelected = true;
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*)pindex;
        }
    }
    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("%s : selection hash=%s\n", __func__, hashBest.ToString().c_str());
    return fSelected;
}

bool GetOldStakeModifier(CStakeInput* stake, uint64_t& nStakeModifier)
{
    if(Params().IsRegTestNet()) {
        nStakeModifier = 0;
        return true;
    }
    CBlockIndex* pindexFrom = stake->GetIndexFrom();
    if (!pindexFrom) return error("%s : failed to get index from", __func__);
    if (stake->IsZPIV()) {
        int64_t nTimeBlockFrom = pindexFrom->GetBlockTime();
        const int nHeightStop = std::min(chainActive.Height(), Params().GetConsensus().height_last_ZC_AccumCheckpoint-1);
        while (pindexFrom && pindexFrom->nHeight + 1 <= nHeightStop) {
            if (pindexFrom->GetBlockTime() - nTimeBlockFrom > 60 * 60) {
                nStakeModifier = pindexFrom->nAccumulatorCheckpoint.GetCheapHash();
                return true;
            }
            pindexFrom = chainActive.Next(pindexFrom);
        }
        return false;

    } else if (!GetOldModifier(pindexFrom->GetBlockHash(), nStakeModifier))
        return error("%s : failed to get kernel stake modifier", __func__);

    return true;
}

// The stake modifier used to hash for a stake kernel is chosen as the stake
// modifier about a selection interval later than the coin generating the kernel
bool GetOldModifier(const uint256& hashBlockFrom, uint64_t& nStakeModifier)
{
    if (!mapBlockIndex.count(hashBlockFrom))
        return error("%s : block not indexed", __func__);
    const CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];
    int64_t nStakeModifierTime = pindexFrom->GetBlockTime();
    const CBlockIndex* pindex = pindexFrom;
    CBlockIndex* pindexNext = chainActive[pindex->nHeight + 1];

    // loop to find the stake modifier later by a selection interval
    do {
        if (!pindexNext) {
            // Should never happen
            return error("%s : Null pindexNext, current block %s ", __func__, pindex->phashBlock->GetHex());
        }
        pindex = pindexNext;
        if (pindex->GeneratedStakeModifier()) nStakeModifierTime = pindex->GetBlockTime();
        pindexNext = chainActive[pindex->nHeight + 1];
    } while (nStakeModifierTime < pindexFrom->GetBlockTime() + OLD_MODIFIER_INTERVAL);

    nStakeModifier = pindex->GetStakeModifierV1();
    return true;
}

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
// Stake modifier consists of bits each of which is contributed from a
// selected block of a given block group in the past.
// The selection of a block is based on a hash of the block's proof-hash and
// the previous stake modifier.
// Stake modifier is recomputed at a fixed time interval instead of every
// block. This is to make it difficult for an attacker to gain control of
// additional bits in the stake modifier, even after generating a chain of
// blocks.
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier)
{
    nStakeModifier = 0;
    fGeneratedStakeModifier = false;

    if (!pindexPrev) {
        fGeneratedStakeModifier = true;
        return true; // genesis block's modifier is 0
    }
    if (pindexPrev->nHeight == 0) {
        //Give a stake modifier to the first block
        fGeneratedStakeModifier = true;
        nStakeModifier = uint64_t("stakemodifier");
        return true;
    }

    // First find current stake modifier and its generation block time
    // if it's not old enough, return the same stake modifier
    int64_t nModifierTime = 0;
    const CBlockIndex* p = pindexPrev;
    while (p && p->pprev && !p->GeneratedStakeModifier()) p = p->pprev;
    if (!p->GeneratedStakeModifier()) return error("%s : unable to get last modifier", __func__);
    nStakeModifier = p->GetStakeModifierV1();
    nModifierTime = p->GetBlockTime();

    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("%s : prev modifier= %s time=%s\n", __func__, std::to_string(nStakeModifier).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nModifierTime).c_str());

    if (nModifierTime / MODIFIER_INTERVAL >= pindexPrev->GetBlockTime() / MODIFIER_INTERVAL)
        return true;

    // Sort candidate blocks by timestamp
    std::vector<std::pair<int64_t, uint256> > vSortedByTimestamp;
    vSortedByTimestamp.reserve(64 * MODIFIER_INTERVAL  / Params().GetConsensus().nTargetSpacing);
    int64_t nSelectionIntervalStart = (pindexPrev->GetBlockTime() / MODIFIER_INTERVAL ) * MODIFIER_INTERVAL  - OLD_MODIFIER_INTERVAL;
    const CBlockIndex* pindex = pindexPrev;

    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart) {
        vSortedByTimestamp.push_back(std::make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        pindex = pindex->pprev;
    }

    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
    std::reverse(vSortedByTimestamp.begin(), vSortedByTimestamp.end());
    std::sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end());

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    std::map<uint256, const CBlockIndex*> mapSelectedBlocks;
    for (int nRound = 0; nRound < std::min(64, (int)vSortedByTimestamp.size()); nRound++) {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound);

        // select a block from the candidates of current round
        if (!SelectBlockFromCandidates(vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop, nStakeModifier, &pindex))
            return error("%s : unable to select block at round %d", __func__, nRound);

        // write the entropy bit of the selected block
        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);

        // add the selected block from candidates to selected list
        mapSelectedBlocks.insert(std::make_pair(pindex->GetBlockHash(), pindex));
        if (GetBoolArg("-printstakemodifier", false))
            LogPrintf("%s : selected round %d stop=%s height=%d bit=%d\n", __func__,
                nRound, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nSelectionIntervalStop).c_str(), pindex->nHeight, pindex->GetStakeEntropyBit());
    }

    // Print selection map for visualization of the selected blocks
    if (GetBoolArg("-printstakemodifier", false)) {
        std::string strSelectionMap = "";
        // '-' indicates proof-of-work blocks not selected
        strSelectionMap.insert(0, pindexPrev->nHeight - nHeightFirstCandidate + 1, '-');
        pindex = pindexPrev;
        while (pindex && pindex->nHeight >= nHeightFirstCandidate) {
            // '=' indicates proof-of-stake blocks not selected
            if (pindex->IsProofOfStake())
                strSelectionMap.replace(pindex->nHeight - nHeightFirstCandidate, 1, "=");
            pindex = pindex->pprev;
        }
        for (const std::pair<const uint256, const CBlockIndex*> &item : mapSelectedBlocks) {
            // 'S' indicates selected proof-of-stake blocks
            // 'W' indicates selected proof-of-work blocks
            strSelectionMap.replace(item.second->nHeight - nHeightFirstCandidate, 1, item.second->IsProofOfStake() ? "S" : "W");
        }
        LogPrintf("%s : selection height [%d, %d] map %s\n", __func__, nHeightFirstCandidate, pindexPrev->nHeight, strSelectionMap.c_str());
    }
    if (GetBoolArg("-printstakemodifier", false)) {
        LogPrintf("%s : new modifier=%s time=%s\n", __func__, std::to_string(nStakeModifierNew).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M:%S", pindexPrev->GetBlockTime()).c_str());
    }

    nStakeModifier = nStakeModifierNew;
    fGeneratedStakeModifier = true;
    return true;
}
