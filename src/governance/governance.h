// Copyright (c) 2022 Rapids Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ROZA_VALIDATORS_H
#define ROZA_VALIDATORS_H

#include <script/script.h>
#include <chainparams.h>
#include <dbwrapper.h>
#include <chain.h>

#define GOVERNANCE_MARKER 71
#define GOVERNANCE_ACTION 65
#define GOVERNANCE_COST 67
#define GOVERNANCE_FEE 102

#define GOVERNANCE_COST_FIXED 1
#define GOVERNANCE_COST_MANAGED 2
#define GOVERNANCE_COST_VARIABLE 3

class CGovernance : CDBWrapper 
{
public:
    CGovernance(size_t nCacheSize, bool fMemory, bool fWipe);
    bool Init(bool fWipe, const CChainParams& chainparams);

    bool UpdateCost(CAmount cost, int type, int height);
    bool RevertUpdateCost(int type, int height);
    CAmount GetCost(int type);

    bool UpdateFeeScript(CScript script, int height);
    bool RevertUpdateFeeScript(int height);
    CScript GetFeeScript();

    using CDBWrapper::Sync;
  
};

#endif /* RAPIDS_GOVERNANCE_H */
