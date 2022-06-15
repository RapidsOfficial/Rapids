// Copyright (c) 2022 Rapids Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <unordered_map>

#include <governance/governance.h>
#include <utilstrencodings.h>
#include <script/script.h>
#include <chainparams.h>
#include <core_io.h>
#include <amount.h>
#include <base58.h>
#include <chain.h>
#include <util.h>

static const CScript DUMMY_SCRIPT = CScript() << ParseHex("6885777789"); 
static const int DUMMY_TYPE = 0;

static const char DB_FEE_ADDRESS = 'f';
static const char DB_COST = 'c';

static const char DB_GOVERNANCE_INIT  = 'G';

namespace {
    struct CostEntry {
        char key;
        int type;
        int height;

        CostEntry() : key(DB_COST), type(DUMMY_TYPE), height(0) {}
        CostEntry(int type, int height) : key(DB_COST), type(type), height(height) {}

        template<typename Stream>
        void Serialize(Stream &s) const {
            s << key;
            s << type;
            s << height;
        }

        template<typename Stream>
        void Unserialize(Stream& s) {
            s >> key;
            s >> type;
            s >> height;
        }
    };

    struct CostDetails {
        CAmount cost;

        CostDetails() : cost(0) {}
        CostDetails(CAmount cost) : cost(cost) {}

        template<typename Stream>
        void Serialize(Stream &s) const {
            s << cost;
        }

        template<typename Stream>
        void Unserialize(Stream& s) {
            s >> cost;
        }
    };

    struct FeeEntry {
        char key;
        int height;

        FeeEntry() : key(DB_FEE_ADDRESS), height(0) {}
        FeeEntry(int height) : key(DB_FEE_ADDRESS), height(height) {}

        template<typename Stream>
        void Serialize(Stream &s) const {
            s << key;
            s << height;
        }

        template<typename Stream>
        void Unserialize(Stream& s) {
            s >> key;
            s >> height;
        }
    };

    struct FeeDetails {
        CScript script;

        FeeDetails() : script(DUMMY_SCRIPT) {}
        FeeDetails(CScript script) : script(script) {}

        template<typename Stream>
        void Serialize(Stream &s) const {
            s << script;
        }

        template<typename Stream>
        void Unserialize(Stream& s) {
            s >> script;
        }
    };
}

CGovernance::CGovernance(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "governance", nCacheSize, fMemory, fWipe) 
{
}

bool CGovernance::Init(bool fWipe, const CChainParams& chainparams) {
    bool init;

    if (fWipe || Read(DB_GOVERNANCE_INIT, init) == false || init == false) {
        LogPrintf("Governance: Creating new database\n");

        CDBBatch batch;

        // Add dummy entries will be first for searching the database
        batch.Write(CostEntry(), CostDetails());

        // Add initial token issuance cost values
        batch.Write(CostEntry(GOVERNANCE_COST_FIXED, 0), CostDetails(1 * COIN));
        batch.Write(CostEntry(GOVERNANCE_COST_MANAGED, 0), CostDetails(1 * COIN));
        batch.Write(CostEntry(GOVERNANCE_COST_VARIABLE, 0), CostDetails(1 * COIN));

        // Add initial token fee address from chainparams
        CTxDestination destination = DecodeDestination("");
        CScript feeScript = GetScriptForDestination(destination);
        batch.Write(FeeEntry(), FeeDetails(feeScript));

        batch.Write(DB_GOVERNANCE_INIT, true);
        WriteBatch(batch);
    }

    return true;
}

CAmount CGovernance::GetCost(int type) {
    CostDetails details = CostDetails();
    int height = -1;

    std::unique_ptr<CDBIterator> it(NewIterator());
    for (it->Seek(CostEntry()); it->Valid(); it->Next()) {
        CostEntry entry;
        if (it->GetKey(entry) && entry.key == DB_COST) {
            if (entry.type == type && entry.height > height) {
                height = entry.height;
                it->GetValue(details);
            }
        } else {
            break;
        }
    }

    return details.cost;
}

bool CGovernance::UpdateCost(CAmount cost, int type, int height) {
    CostEntry entry(type, height);
    CostDetails details = CostDetails();
    CDBBatch batch;
    std::string type_name;

    if (type == GOVERNANCE_COST_FIXED) {
        type_name = "fixed";
    } else if (type == GOVERNANCE_COST_MANAGED) {
        type_name = "managed";
    } else if (type == GOVERNANCE_COST_VARIABLE) {
        type_name = "variable";
    } else {
        LogPrintf("Governance: Trying to update issuance cost for unknow type\n");
        return false;
    }

    if (!Read(entry, details)) {
        // LogPrintf("Governance: Updating issuance cost for \"%s\" to %s AOK\n", type_name, ValueFromAmountString(cost, 8));
        batch.Write(entry, CostDetails(cost));
    }

    return WriteBatch(batch);
}

bool CGovernance::RevertUpdateCost(int type, int height) {
    CostEntry entry(type, height);
    CostDetails details = CostDetails();
    CDBBatch batch;

    std::string type_name;

    if (type == GOVERNANCE_COST_FIXED) {
        type_name = "fixed";
    } else if (type == GOVERNANCE_COST_MANAGED) {
        type_name = "managed";
    } else if (type == GOVERNANCE_COST_VARIABLE) {
        type_name = "variable";
    }

    if (Read(entry, details)) {
        // LogPrintf("Governance: Revert updating issuance cost for \"%s\" to %s AOK\n", type_name, ValueFromAmountString(details.cost, 8));
        batch.Erase(entry);
    } else {
        // LogPrintf("Governance: Trying to revert unknown issuance cost update, database is corrupted\n");
        return false;
    }

    return WriteBatch(batch);
}

CScript CGovernance::GetFeeScript() {
    FeeDetails details = FeeDetails();
    int height = -1;

    std::unique_ptr<CDBIterator> it(NewIterator());
    for (it->Seek(FeeEntry()); it->Valid(); it->Next()) {
        FeeEntry entry;
        if (it->GetKey(entry) && entry.key == DB_FEE_ADDRESS) {
            if (entry.height > height) {
                height = entry.height;
                it->GetValue(details);
            }
        } else {
            break;
        }
    }

    return details.script;
}

bool CGovernance::UpdateFeeScript(CScript script, int height) {
    FeeEntry entry(height);
    FeeDetails details = FeeDetails();
    CDBBatch batch;

    if (!Read(entry, details)) {
        LogPrintf("Governance: Updating fee script to %s\n", HexStr(script));
        batch.Write(entry, FeeDetails(script));
    }

    return WriteBatch(batch);
}

bool CGovernance::RevertUpdateFeeScript(int height) {
    FeeEntry entry(height);
    FeeDetails details = FeeDetails();
    CDBBatch batch;

    if (Read(entry, details)) {
        LogPrintf("Governance: Revert updating fee script to %s\n", HexStr(details.script));
        batch.Erase(entry);
    } else {
        LogPrintf("Governance: Trying to revert unknown fee script update, database is corrupted\n");
        return false;
    }

    return WriteBatch(batch);
}
