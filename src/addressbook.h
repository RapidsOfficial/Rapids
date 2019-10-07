//
// Created by furszy on 2019-10-07.
//

#ifndef FURSZY_PIVX_ADDRESSBOOK_H
#define FURSZY_PIVX_ADDRESSBOOK_H

#include <map>
#include <string>

/** Address book data */
class CAddressBookData
{
public:

    class AddressBookPurpose {
    public:
        inline static const std::string UNKNOWN = "unknown";
        inline static const std::string RECEIVE = "receive";
        inline static const std::string SEND = "send";
        inline static const std::string DELEGABLE = "delegable";
        inline static const std::string DELEGATOR = "delegator";
    };

    std::string name;
    std::string purpose;

    CAddressBookData()
    {
        purpose = AddressBookPurpose::UNKNOWN;
    }

    typedef std::map<std::string, std::string> StringMap;
    StringMap destdata;
};

#endif //FURSZY_PIVX_ADDRESSBOOK_H
