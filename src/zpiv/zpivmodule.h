// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
#ifndef PIVX_ZPIVMODULE_H
#define PIVX_ZPIVMODULE_H

#include "libzerocoin/bignum.h"
#include "libzerocoin/Denominations.h"
#include "libzerocoin/CoinSpend.h"
#include "libzerocoin/Coin.h"
#include "libzerocoin/SpendType.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "main.h"
#include "serialize.h"
#include "uint256.h"
#include <streams.h>
#include <utilstrencodings.h>
#include "zpiv/zerocoin.h"

class PublicCoinSpend : public libzerocoin::CoinSpend{
public:

    PublicCoinSpend(){};

    PublicCoinSpend(CBigNum serial, CBigNum randomness, CPubKey pubkey, std::vector<unsigned char> vchSig){
        this->coinSerialNumber = serial;
        this->randomness = randomness;
        this->vchSig = vchSig;
        this->pubkey = pubkey;
    };

    template <typename Stream>
    PublicCoinSpend(Stream& strm){
        strm >> *this;
    }

    uint8_t getVersion() const { return libzerocoin::PrivateCoin::PUBKEY_VERSION; }

    bool HasValidSerial(libzerocoin::ZerocoinParams* params) const override;
    bool HasValidSignature() const override ;
    const uint256 signatureHash() const override { return hashTxOut; }
    libzerocoin::SpendType getSpendType() const { return libzerocoin::SpendType::SPEND; }

    CBigNum randomness;
    // prev out values
    uint256 txHash = 0;
    // hash of the outputs of the txes that spend this coins
    uint256 hashTxOut = 0;
    unsigned int outputIndex = -1;
    libzerocoin::PublicCoin *pubCoin = nullptr;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(coinSerialNumber);
        READWRITE(randomness);
        READWRITE(pubkey);
        READWRITE(vchSig);
    }
};


class ZPIVModule {

public:
    ZPIVModule(){}

    bool createInput(CTxIn &in, CZerocoinMint mint, uint256 hashTxOut);
    PublicCoinSpend parseCoinSpend(const CTxIn &in, const CTransaction& tx);
    bool validateInput(const CTxIn &in, const CTxOut &prevOut, const CTransaction& tx, PublicCoinSpend& ret);
};


#endif //PIVX_ZPIVMODULE_H
