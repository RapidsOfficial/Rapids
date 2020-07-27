// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2013-2014 The NovaCoin Developers
// Copyright (c) 2014-2018 The BlackCoin Developers
// Copyright (c) 2015-2018-2020 The Rapids developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "kernel.h"

#include "db.h"
#include "legacy/stakemodifier.h"
#include "script/interpreter.h"
#include "util.h"
#include "stakeinput.h"
#include "utilmoneystr.h"

#include <boost/assign/list_of.hpp>

/**
* CStakeKernel Constructor
*
* @param[in]   pindexPrev      index of the parent of the kernel block
* @param[in]   stakeInput      input for the coinstake of the kernel block
* @param[in]   nBits           target difficulty bits of the kernel block
* @param[in]   nTimeTx         time of the kernel block
*/
CStakeKernel::CStakeKernel(const CBlockIndex* const pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int nTimeTx) :
	stakeUniqueness(stakeInput->GetUniqueness()),
	nTime(nTimeTx),
	nBits(nBits),
	stakeValue(stakeInput->GetValue())
{
	// Set kernel stake modifier
	if (pindexPrev->nHeight + 1 < Params().GetConsensus().height_start_StakeModifierV2) {
		uint64_t nStakeModifier = 0;
		if (!GetOldStakeModifier(stakeInput, nStakeModifier))
			LogPrintf("%s : ERROR: Failed to get kernel stake modifier\n", __func__);
		// Modifier v1
		stakeModifier << nStakeModifier;
	}
	else {
		// Modifier v2
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
		LogPrint(BCLog::STAKING, "%s : Proof Of Stake:"
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
	}
	else {
		// check that is the actual parent block
		if (block.hashPrevBlock != pindexPrev->GetBlockHash())
			return error("%s : previous block mismatch");
	}

	// Check that this is a PoS block
	if (!block.IsProofOfStake())
		return error("called on non PoS block");

	// Construct the stakeinput object
	const CTxIn& txin = block.vtx[1].vin[0];
	stake = std::unique_ptr<CStakeInput>(new CRpdStake());

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
	//const bool fRegTest = Params().IsRegTestNet();
	//nTimeTx = (fRegTest ? GetAdjustedTime() : GetCurrentTimeSlot());
	//if (nTimeTx <= pindexPrev->nTime && !fRegTest) return false;

	const Consensus::Params& consensus = Params().GetConsensus();
	const bool fTimeV2 = consensus.IsTimeProtocolV2(nHeightTx);
	if (fTimeV2) {
		const bool fRegTest = Params().IsRegTestNet();
		nTimeTx = (fRegTest ? GetAdjustedTime() : GetCurrentTimeSlot());
		// double check that we are not on the same slot as prev block
		if (nTimeTx <= pindexPrev->nTime && !fRegTest) return false;

		// Verify Proof Of Stake
		CStakeKernel stakeKernel(pindexPrev, stakeInput, nBits, nTimeTx);
		return stakeKernel.CheckKernelHash(true);
		// check stake kernel
		//return CheckStakeKernelHash(pindexPrev, nBits, stakeInput, nTimeTx, hashProofOfStake);
	}

	// Time protocol V1: iterate the hashing (can be removed after hard-fork)
	return StakeV1(pindexPrev, stakeInput, nBits, nTimeTx);
}

bool StakeV1(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int64_t& nTimeTx)
{
	//bool fSuccess = false;
	////nTimeTx = pindexPrev->nTime;
	//unsigned int nTryTime = nTimeTx;
	//// iterate from nTimeTx up to nTimeTx + nHashDrift
	//// but not after the max allowed future blocktime drift (3 minutes for PoS)
	//const unsigned int maxTime = GetAdjustedTime() + 45;

	//while (nTryTime < maxTime) {
	//	//new block came in, move on
	//	if (chainActive.Height() != pindexPrev->nHeight) break;

	//	++nTryTime;
	//	// if stake hash does not meet the target then continue to next iteration
	//	CStakeKernel stakeKernel(pindexPrev, stakeInput, nBits, nTryTime);
	//	if (!stakeKernel.CheckKernelHash(true))
	//		continue;

	//	// if we made it this far, then we have successfully found a valid kernel hash
	//	fSuccess = true;
	//	nTimeTx = nTryTime;
	//	break;
	//}

	//mapHashedBlocks.clear();
	//mapHashedBlocks[pindexPrev->nHeight] = GetTime(); //store a time stamp of when we last hashed on this block

	//return fSuccess;
	
	bool fSuccess = false;
	unsigned int nTryTime = 0;
	unsigned int i;
	int nHeightStart = chainActive.Height();
	for (i = 0; i < (45); i++) //iterate the hashing
	{
		if (chainActive.Height() != nHeightStart) break;

		nTryTime = nTimeTx + 45 - i;
		CStakeKernel stakeKernel(pindexPrev, stakeInput, nBits, nTryTime);
		if (!stakeKernel.CheckKernelHash(true))
			continue;
		fSuccess = true; // if we make it this far then we have successfully created a stake hash
		nTimeTx = nTryTime;
		break;
	}
	mapHashedBlocks.clear();
	mapHashedBlocks[pindexPrev->nHeight] = GetTime(); //store a time stamp of when we last hashed on this block

	return fSuccess;

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
	const int nHeight = pindexPrev->nHeight + 1;
	// Initialize stake input
	std::unique_ptr<CStakeInput> stakeInput;
	if (!LoadStakeInput(block, pindexPrev, stakeInput)) {
		strError = "stake input initialization failed";
		return false;
	}

	// Stake input contextual checks
	if (!stakeInput->ContextCheck(nHeight, block.nTime)) {
		strError = "stake input failing contextual checks";
		return false;
	}

	// Verify Proof Of Stake
	CStakeKernel stakeKernel(pindexPrev, stakeInput.get(), block.nBits, block.nTime);
	if (!stakeKernel.CheckKernelHash()) {
		strError = "kernel hash check fails";
		return false;
	}

	// Verify tx input signature
	CTxOut stakePrevout;
	if (!stakeInput->GetTxOutFrom(stakePrevout)) {
		strError = "unable to get stake prevout for coinstake";
		return false;
	}
	const CTransaction& tx = block.vtx[1];
	const CTxIn& txin = tx.vin[0];
	ScriptError serror;
	if (!VerifyScript(txin.scriptSig, stakePrevout.scriptPubKey, STANDARD_SCRIPT_VERIFY_FLAGS,
		TransactionSignatureChecker(&tx, 0), &serror)) {
		strError = strprintf("signature fails: %s", serror ? ScriptErrorString(serror) : "");
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

