// Copyright (c) 2020 The PIVX developers
// Copyright (c) 2018-2020 The Rapids developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Rapids_CONSENSUS_ZEROCOIN_VERIFY_H
#define Rapids_CONSENSUS_ZEROCOIN_VERIFY_H

#include "consensus/consensus.h"
#include "main.h"
#include "script/interpreter.h"
#include "zrpdchain.h"

/** Context-independent validity checks */
bool CheckZerocoinSpend(const CTransaction& tx, bool fVerifySignature, CValidationState& state, bool fFakeSerialAttack = false);
// Fake Serial attack Range
bool isBlockBetweenFakeSerialAttackRange(int nHeight);
// Public coin spend
bool CheckPublicCoinSpendEnforced(int blockHeight, bool isPublicSpend);
int CurrentPublicCoinSpendVersion();
bool CheckPublicCoinSpendVersion(int version);
bool ContextualCheckZerocoinSpend(const CTransaction& tx, const libzerocoin::CoinSpend* spend, int nHeight, const uint256& hashBlock);
bool ContextualCheckZerocoinSpendNoSerialCheck(const CTransaction& tx, const libzerocoin::CoinSpend* spend, int nHeight, const uint256& hashBlock);
bool RecalculateRPDSupply(int nHeightStart, bool fSkipZrpd = true);
CAmount GetInvalidUTXOValue();

#endif //Rapids_CONSENSUS_ZEROCOIN_VERIFY_H
