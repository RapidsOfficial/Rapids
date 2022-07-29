// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
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

#include <vector>

struct CDNSSeedData {
    std::string name, host;
    bool supportsServiceBitsFiltering;
    CDNSSeedData(const std::string& strName, const std::string& strHost, bool supportsServiceBitsFilteringIn = false) : name(strName), host(strHost), supportsServiceBitsFiltering(supportsServiceBitsFilteringIn) {}
};

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * RPD system. There are three: the main network on which people trade goods
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

    enum Bech32Type {
        SAPLING_PAYMENT_ADDRESS,
        SAPLING_FULL_VIEWING_KEY,
        SAPLING_INCOMING_VIEWING_KEY,
        SAPLING_EXTENDED_SPEND_KEY,

        MAX_BECH32_TYPES
    };

    const Consensus::Params& GetConsensus() const { return consensus; }
    const CMessageHeader::MessageStartChars& MessageStart() const { return pchMessageStart; }
    int GetDefaultPort() const { return nDefaultPort; }

    const CBlock& GenesisBlock() const { return genesis; }

    /** Make miner wait to have peers to avoid wasting work */
    bool MiningRequiresPeers() const { return !IsRegTestNet() && !IsTestNet(); }
    /** Headers first syncing is disabled */
    bool HeadersFirstSyncingActive() const { return false; };
    /** Default value for -checkmempool and -checkblockindex argument */
    bool DefaultConsistencyChecks() const { return IsRegTestNet(); }

    /** Return the BIP70 network string (main, test or regtest) */
    std::string NetworkIDString() const { return strNetworkID; }
    const std::vector<CDNSSeedData>& DNSSeeds() const { return vSeeds; }
    const std::vector<unsigned char>& Base58Prefix(Base58Type type) const { return base58Prefixes[type]; }
    const std::string& Bech32HRP(Bech32Type type) const { return bech32HRPs[type]; }
    const std::vector<SeedSpec6>& FixedSeeds() const { return vFixedSeeds; }
    virtual const Checkpoints::CCheckpointData& Checkpoints() const = 0;

    CBaseChainParams::Network NetworkID() const { return networkID; }
    bool IsTestNet() const { return NetworkID() == CBaseChainParams::TESTNET; }
    bool IsRegTestNet() const { return NetworkID() == CBaseChainParams::REGTEST; }

    const std::string& DevFundAddress() const { return devFundAddress; }

    // Governance
    const std::string& GovernanceMasterAddress() const { return strMasterAddress; }
    const std::string& GovernanceFeeAddress() const { return strFeeAddress; }

    const CAmount GovernanceFixedFee() const { return tokenFixedFee; }
    const CAmount GovernanceManagedFee() const { return tokenManagedFee; }
    const CAmount GovernanceVariableFee() const { return tokenVariableFee; }
    const CAmount GovernanceUsernameFee() const { return tokenUsernameFee; }
    const CAmount GovernanceSubFee() const { return tokenSubFee; }

    /** Get masternode collateral */
    CAmount Collateral(int nHeight) const {
        if (nHeight > consensus.height_supply_reduction)
            return 10000 * COIN;

        return 10000000 * COIN;
    }


protected:
    CChainParams() {}

    CBaseChainParams::Network networkID;
    std::string strNetworkID;
    CBlock genesis;
    Consensus::Params consensus;
    CMessageHeader::MessageStartChars pchMessageStart;
    int nDefaultPort;
    std::vector<CDNSSeedData> vSeeds;
    std::vector<unsigned char> base58Prefixes[MAX_BASE58_TYPES];
    std::string bech32HRPs[MAX_BECH32_TYPES];
    std::vector<SeedSpec6> vFixedSeeds;

    std::string devFundAddress;

    // Governance Master address
    std::string strMasterAddress;
    // Governance Fee address
    std::string strFeeAddress;

    CAmount tokenFixedFee;
    CAmount tokenManagedFee;
    CAmount tokenVariableFee;
    CAmount tokenUsernameFee;
    CAmount tokenSubFee;
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

/**
 * Allows modifying the network upgrade regtest parameters.
 */
void UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex idx, int nActivationHeight);

#endif // BITCOIN_CHAINPARAMS_H
