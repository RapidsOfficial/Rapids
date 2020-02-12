// Copyright (c) 2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
#ifndef PIVX_HDCHAIN_H
#define PIVX_HDCHAIN_H

#include "key.h"

/* Simple HD chain data model */
class CHDChain
{
private:

    int nVersion;
    CKeyID seed_id;

public:
    static const int CURRENT_VERSION = 1;
    // Single account counters, todo: move this to the account
    uint32_t nExternalChainCounter{0};
    uint32_t nInternalChainCounter{0};

    CHDChain() { SetNull(); }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(this->nVersion);
        READWRITE(seed_id);
        // Single account counters, todo: move this to an account class
        READWRITE(nExternalChainCounter);
        READWRITE(nInternalChainCounter);
    }

    bool SetNull();
    bool IsNull() const;

    bool SetSeed(const CKeyID& seedId);
    CKeyID GetID() const { return seed_id; }
};

#endif // PIVX_HDCHAIN_H
