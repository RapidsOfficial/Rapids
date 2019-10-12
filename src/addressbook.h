// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_ADDRESSBOOK_H
#define PIVX_ADDRESSBOOK_H

#include <map>
#include <string>

namespace AddressBook {

    namespace AddressBookPurpose {
        inline static const std::string UNKNOWN = "unknown";
        inline static const std::string RECEIVE = "receive";
        inline static const std::string SEND = "send";
        inline static const std::string DELEGABLE = "delegable";
        inline static const std::string DELEGATOR = "delegator";
    }

/** Address book data */
    class CAddressBookData {
    public:

        std::string name;
        std::string purpose;

        CAddressBookData() {
            purpose = AddressBook::AddressBookPurpose::UNKNOWN;
        }

        typedef std::map<std::string, std::string> StringMap;
        StringMap destdata;
    };

}

#endif //PIVX_ADDRESSBOOK_H
