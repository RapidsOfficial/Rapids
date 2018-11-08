//
// Copyright (c) 2015-2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <iostream>
#include "genwit.h"
#include "chainparams.h"

CGenWit::CGenWit() : accWitValue(0) {}

CGenWit::CGenWit(const CBloomFilter &filter, int startingHeight, libzerocoin::CoinDenomination den, int requestNum, CBigNum accWitValue)
        : filter(filter), startingHeight(startingHeight), den(den), requestNum(requestNum), accWitValue(accWitValue) {}

bool CGenWit::isValid(int chainActiveHeight) {
    if (den == libzerocoin::CoinDenomination::ZQ_ERROR){
        return false;
    }
    if(!filter.IsWithinSizeConstraints()){
        //TODO: throw exception
        return false;
    }

    if (startingHeight < Params().Zerocoin_Block_V2_Start()){
        return false;
    }

    if (accWitValue == 0){
        std::cout << "invalid accWit" << std::endl;
        return false;
    }

    return (startingHeight < chainActiveHeight - 20);
}

const CBloomFilter &CGenWit::getFilter() const {
    return filter;
}

int CGenWit::getStartingHeight() const {
    return startingHeight;
}

libzerocoin::CoinDenomination CGenWit::getDen() const {
    return den;
}

int CGenWit::getRequestNum() const {
    return requestNum;
}

CNode *CGenWit::getPfrom() const {
    return pfrom;
}

void CGenWit::setPfrom(CNode *pfrom) {
    CGenWit::pfrom = pfrom;
}

const CBigNum &CGenWit::getAccWitValue() const {
    return accWitValue;
}

const std::string CGenWit::toString() const {
    return "From: " + pfrom->addrName + ",\n" +
           "Height: " + std::to_string(startingHeight) + ",\n" +
           "accWit: " + accWitValue.GetHex();
}
