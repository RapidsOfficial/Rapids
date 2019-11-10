// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "addressbook.h"
#include <string>

namespace AddressBook {

    namespace AddressBookPurpose {
        const std::string UNKNOWN{"unknown"};
        const std::string RECEIVE{"receive"};
        const std::string SEND{"send"};
        const std::string DELEGABLE{"delegable"};
        const std::string DELEGATOR{"delegator"};
        const std::string COLD_STAKING{"coldstaking"};
    }
}

