// Copyright (c) 2018 The PIVX developers
// Copyright (c) 2018-2020 The Rapids developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Rapids_SPENDTYPE_H
#define Rapids_SPENDTYPE_H

#include <cstdint>

namespace libzerocoin {
    enum SpendType : uint8_t {
        SPEND, // Used for a typical spend transaction, zRPD should be unusable after
        STAKE, // Used for a spend that occurs as a stake
        MN_COLLATERAL, // Used when proving ownership of zRPD that will be used for masternodes (future)
        SIGN_MESSAGE // Used to sign messages that do not belong above (future)
    };
}

#endif //Rapids_SPENDTYPE_H
