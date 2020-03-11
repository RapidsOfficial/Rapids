// Copyright (c) 2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
#include "wallet/hdchain.h"

#include "base58.h"
#include "chainparams.h"
#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

bool CHDChain::SetNull()
{
    nVersion = CURRENT_VERSION;
    seed_id = CKeyID();
    nExternalChainCounter = 0;
    nInternalChainCounter = 0;
    nStakingChainCounter = 0;
    return IsNull();
}

bool CHDChain::IsNull() const
{
    return seed_id.IsNull();
}

bool CHDChain::SetSeed( const CKeyID& seedId)
{
    // Cannot be initialized twice
    if (!IsNull())
        return false;

    seed_id = seedId;
    return true;
}