// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAINPARAMS_H
#define BITCOIN_CHAINPARAMS_H

#include "chainparamsbase.h"
#include "checkpoints.h"
#include "consensus/params.h"
#include "primitives/block.h"
#include "protocol.h"
#include "uint256.h"

#include "libzerocoin/Params.h"
#include <vector>

typedef unsigned char MessageStartChars[MESSAGE_START_SIZE];

struct CDNSSeedData {
    std::string name, host;
    CDNSSeedData(const std::string& strName, const std::string& strHost) : name(strName), host(strHost) {}
};

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * PIVX system. There are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. It has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class CChainParams
{
public:
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,
        SECRET_KEY,     // BIP16
        EXT_PUBLIC_KEY, // BIP32
        EXT_SECRET_KEY, // BIP32
        EXT_COIN_TYPE,  // BIP44
        STAKING_ADDRESS,

        MAX_BASE58_TYPES
    };

    const Consensus::Params& GetConsensus() const { return consensus; }
    const MessageStartChars& MessageStart() const { return pchMessageStart; }
    int GetDefaultPort() const { return nDefaultPort; }

    const CBlock& GenesisBlock() const { return genesis; }
    const std::vector<unsigned char>& AlertKey() const { return vAlertPubKey; }
    /** Used to check majorities for block version upgrade */
    int EnforceBlockUpgradeMajority() const { return nEnforceBlockUpgradeMajority; }
    int RejectBlockOutdatedMajority() const { return nRejectBlockOutdatedMajority; }
    int ToCheckBlockUpgradeMajority() const { return nToCheckBlockUpgradeMajority; }
    int MaxReorganizationDepth() const { return nMaxReorganizationDepth; }

    /** Used if GenerateBitcoins is called with a negative number of threads */
    int DefaultMinerThreads() const { return nMinerThreads; }
    /** Make miner wait to have peers to avoid wasting work */
    bool MiningRequiresPeers() const { return fMiningRequiresPeers; }
    /** Headers first syncing is disabled */
    bool HeadersFirstSyncingActive() const { return fHeadersFirstSyncingActive; };
    /** Default value for -checkmempool and -checkblockindex argument */
    bool DefaultConsistencyChecks() const { return fDefaultConsistencyChecks; }
    /** Skip proof-of-work check: allow mining of any difficulty block */
    bool SkipProofOfWorkCheck() const { return fSkipProofOfWorkCheck; }
    /** Make standard checks */
    bool RequireStandard() const { return fRequireStandard; }
    /** returns the coinstake maturity (min depth required) **/
    bool HasStakeMinAgeOrDepth(const int contextHeight, const uint32_t contextTime, const int utxoFromBlockHeight, const uint32_t utxoFromBlockTime) const;

    /** The masternode count that we will allow the see-saw reward payments to be off by */
    int MasternodeCountDrift() const { return nMasternodeCountDrift; }
    /** In the future use NetworkIDString() for RPC fields */
    bool TestnetToBeDeprecatedFieldRPC() const { return fTestnetToBeDeprecatedFieldRPC; }
    /** Return the BIP70 network string (main, test or regtest) */
    std::string NetworkIDString() const { return strNetworkID; }
    const std::vector<CDNSSeedData>& DNSSeeds() const { return vSeeds; }
    const std::vector<unsigned char>& Base58Prefix(Base58Type type) const { return base58Prefixes[type]; }
    const std::vector<CAddress>& FixedSeeds() const { return vFixedSeeds; }
    virtual const Checkpoints::CCheckpointData& Checkpoints() const = 0;
    int PoolMaxTransactions() const { return nPoolMaxTransactions; }
    /** Return the number of blocks in a budget cycle */
    int GetBudgetCycleBlocks() const { return nBudgetCycleBlocks; }
    int64_t GetProposalEstablishmentTime() const { return nProposalEstablishmentTime; }

    CAmount GetMinColdStakingAmount() const { return nMinColdStakingAmount; }

    /** Spork key and Masternode Handling **/
    std::string SporkPubKey() const { return strSporkPubKey; }
    std::string SporkPubKeyOld() const { return strSporkPubKeyOld; }
    int64_t NewSporkStart() const { return nEnforceNewSporkKey; }
    int64_t RejectOldSporkKey() const { return nRejectOldSporkKey; }
    std::string ObfuscationPoolDummyAddress() const { return strObfuscationPoolDummyAddress; }
    int64_t StartMasternodePayments() const { return nStartMasternodePayments; }
    int64_t Budget_Fee_Confirmations() const { return nBudget_Fee_Confirmations; }

    CBaseChainParams::Network NetworkID() const { return networkID; }
    bool IsRegTestNet() const { return NetworkID() == CBaseChainParams::REGTEST; }

    /** Zerocoin **/
    std::string Zerocoin_Modulus() const { return zerocoinModulus; }
    libzerocoin::ZerocoinParams* Zerocoin_Params(bool useModulusV1) const;
    int Zerocoin_MaxSpendsPerTransaction() const { return nMaxZerocoinSpendsPerTransaction; }
    int Zerocoin_MaxPublicSpendsPerTransaction() const { return nMaxZerocoinPublicSpendsPerTransaction; }
    CAmount Zerocoin_MintFee() const { return nMinZerocoinMintFee; }
    int Zerocoin_MintRequiredConfirmations() const { return nMintRequiredConfirmations; }
    int Zerocoin_RequiredAccumulation() const { return nRequiredAccumulation; }
    int Zerocoin_DefaultSpendSecurity() const { return nDefaultSecurityLevel; }
    int Zerocoin_HeaderVersion() const { return nZerocoinHeaderVersion; }
    int Zerocoin_RequiredStakeDepth() const { return nZerocoinRequiredStakeDepth; }

    /** Height or Time Based Activations **/
    int ModifierUpgradeBlock() const { return nModifierUpdateBlock; }
    int PivxBadBlockTime() const { return nPivxBadBlockTime; }
    int PivxBadBlocknBits() const { return nPivxBadBlocknBits; }
    int Zerocoin_StartHeight() const { return nZerocoinStartHeight; }
    int Zerocoin_Block_EnforceSerialRange() const { return nBlockEnforceSerialRange; }
    int Zerocoin_Block_RecalculateAccumulators() const { return nBlockRecalculateAccumulators; }
    int Zerocoin_Block_FirstFraudulent() const { return nBlockFirstFraudulent; }
    int Zerocoin_Block_LastGoodCheckpoint() const { return nBlockLastGoodCheckpoint; }
    int Zerocoin_StartTime() const { return nZerocoinStartTime; }
    int Block_Enforce_Invalid() const { return nBlockEnforceInvalidUTXO; }
    int Zerocoin_Block_V2_Start() const { return nBlockZerocoinV2; }
    bool IsStakeModifierV2(const int nHeight) const { return nHeight >= nBlockStakeModifierlV2; }
    int NewSigsActive(const int nHeight) const { return nHeight >= nBlockEnforceNewMessageSignatures; }
    int Block_V7_StartHeight() const { return nBlockV7StartHeight; }

    // fake serial attack
    int Zerocoin_Block_EndFakeSerial() const { return nFakeSerialBlockheightEnd; }
    CAmount GetSupplyBeforeFakeSerial() const { return nSupplyBeforeFakeSerial; }

    int Zerocoin_Block_Double_Accumulated() const { return nBlockDoubleAccumulated; }
    CAmount InvalidAmountFiltered() const { return nInvalidAmountFiltered; };

    int Zerocoin_Block_Public_Spend_Enabled() const { return nPublicZCSpends; }
    int Zerocoin_Block_Last_Checkpoint() const { return nBlockLastAccumulatorCheckpoint; }

protected:
    CChainParams() {}

    Consensus::Params consensus;
    MessageStartChars pchMessageStart;
    //! Raw pub key bytes for the broadcast alert signing key.
    std::vector<unsigned char> vAlertPubKey;
    int nDefaultPort;
    int nMaxReorganizationDepth;
    int nEnforceBlockUpgradeMajority;
    int nRejectBlockOutdatedMajority;
    int nToCheckBlockUpgradeMajority;
    int64_t nPivxBadBlockTime;
    unsigned int nPivxBadBlocknBits;
    int nMasternodeCountDrift;
    int nMaturity;

    int nModifierUpdateBlock;
    int nMinerThreads;
    std::vector<CDNSSeedData> vSeeds;
    std::vector<unsigned char> base58Prefixes[MAX_BASE58_TYPES];
    CBaseChainParams::Network networkID;
    std::string strNetworkID;
    CBlock genesis;
    std::vector<CAddress> vFixedSeeds;
    bool fMiningRequiresPeers;
    bool fDefaultConsistencyChecks;
    bool fRequireStandard;
    bool fSkipProofOfWorkCheck;
    bool fTestnetToBeDeprecatedFieldRPC;
    bool fHeadersFirstSyncingActive;
    int nPoolMaxTransactions;
    int nBudgetCycleBlocks;
    std::string strSporkPubKey;
    std::string strSporkPubKeyOld;
    int64_t nEnforceNewSporkKey;
    int64_t nRejectOldSporkKey;
    std::string strObfuscationPoolDummyAddress;
    int64_t nStartMasternodePayments;
    std::string zerocoinModulus;
    int nMaxZerocoinSpendsPerTransaction;
    int nMaxZerocoinPublicSpendsPerTransaction;
    CAmount nMinZerocoinMintFee;
    CAmount nInvalidAmountFiltered;
    int nMintRequiredConfirmations;
    int nRequiredAccumulation;
    int nDefaultSecurityLevel;
    int nZerocoinHeaderVersion;
    int64_t nBudget_Fee_Confirmations;
    int nZerocoinStartHeight;
    int nZerocoinStartTime;
    int nZerocoinRequiredStakeDepth;
    int64_t nProposalEstablishmentTime;

    int nBlockEnforceSerialRange;
    int nBlockRecalculateAccumulators;
    int nBlockFirstFraudulent;
    int nBlockLastGoodCheckpoint;
    int nBlockEnforceInvalidUTXO;
    int nBlockZerocoinV2;
    int nBlockDoubleAccumulated;
    int nPublicZCSpends;
    int nBlockStakeModifierlV2;
    int nBlockEnforceNewMessageSignatures;
    int nBlockV7StartHeight;
    int nBlockLastAccumulatorCheckpoint;

    CAmount nMinColdStakingAmount;

    // fake serial attack
    int nFakeSerialBlockheightEnd = 0;
    CAmount nSupplyBeforeFakeSerial = 0;
};

/**
 * Return the currently selected parameters. This won't change after app startup
 * outside of the unit tests.
 */
const CChainParams& Params();

/** Return parameters for the given network. */
CChainParams& Params(CBaseChainParams::Network network);

/** Sets the params returned by Params() to those for the given network. */
void SelectParams(CBaseChainParams::Network network);

/**
 * Looks for -regtest or -testnet and then calls SelectParams as appropriate.
 * Returns false if an invalid combination is given.
 */
bool SelectParamsFromCommandLine();

#endif // BITCOIN_CHAINPARAMS_H
