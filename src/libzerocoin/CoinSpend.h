/**
 * @file       CoinSpend.h
 *
 * @brief      CoinSpend class for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017-2020 The PIVX developers

#ifndef COINSPEND_H_
#define COINSPEND_H_

#include <streams.h>
#include <utilstrencodings.h>
#include "Accumulator.h"
#include "Coin.h"
#include "Commitment.h"
#include "Params.h"
#include "SpendType.h"

#include "bignum.h"
#include "pubkey.h"
#include "serialize.h"

namespace libzerocoin
{
// Lagacy zPIV - Only for serialization
// Proof that a value inside a commitment C is accumulated in accumulator A
class AccumulatorProofOfKnowledge {
public:
    AccumulatorProofOfKnowledge() {};
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(C_e); READWRITE(C_u); READWRITE(C_r); READWRITE(st_1); READWRITE(st_2); READWRITE(st_3);
        READWRITE(t_1); READWRITE(t_2); READWRITE(t_3); READWRITE(t_4); READWRITE(s_alpha); READWRITE(s_beta);
        READWRITE(s_zeta); READWRITE(s_sigma); READWRITE(s_eta); READWRITE(s_epsilon);
        READWRITE(s_delta); READWRITE(s_xi); READWRITE(s_phi); READWRITE(s_gamma); READWRITE(s_psi);
    }
private:
    CBigNum C_e, C_u, C_r;
    CBigNum st_1, st_2, st_3;
    CBigNum t_1, t_2, t_3, t_4;
    CBigNum s_alpha, s_beta, s_zeta, s_sigma, s_eta, s_epsilon, s_delta;
    CBigNum s_xi, s_phi, s_gamma, s_psi;
};

// Lagacy zPIV - Only for serialization
// Signature of knowledge attesting that the signer knows the values to
// open a commitment to a coin with given serial number
class SerialNumberSignatureOfKnowledge {
public:
    SerialNumberSignatureOfKnowledge(){};
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(s_notprime);
        READWRITE(sprime);
        READWRITE(hash);
    }
private:
    uint256 hash;
    std::vector<CBigNum> s_notprime;
    std::vector<CBigNum> sprime;
};

// Lagacy zPIV - Only for serialization
// Proof that two commitments open to the same value (BROKEN)
class CommitmentProofOfKnowledge {
public:
    CommitmentProofOfKnowledge() {};
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(S1); READWRITE(S2); READWRITE(S3); READWRITE(challenge);
    }
private:
    CBigNum S1, S2, S3, challenge;
};


// Lagacy zPIV - Only for serialization
/** The complete proof needed to spend a zerocoin.
 * Composes together a proof that a coin is accumulated
 * and that it has a given serial number.
 */
class CoinSpend
{
public:

    CoinSpend() {};
    CoinSpend(CDataStream& strm) { strm >> *this; }
    virtual ~CoinSpend(){};

    const CBigNum& getCoinSerialNumber() const { return this->coinSerialNumber; }
    CoinDenomination getDenomination() const { return this->denomination; }
    uint32_t getAccumulatorChecksum() const { return this->accChecksum; }
    uint256 getTxOutHash() const { return ptxHash; }
    CBigNum getAccCommitment() const { return accCommitmentToCoinValue; }
    CBigNum getSerialComm() const { return serialCommitmentToCoinValue; }
    uint8_t getVersion() const { return version; }
    int getCoinVersion() const { return libzerocoin::ExtractVersionFromSerial(coinSerialNumber); }
    CPubKey getPubKey() const { return pubkey; }
    SpendType getSpendType() const { return spendType; }
    std::vector<unsigned char> getSignature() const { return vchSig; }

    static std::vector<unsigned char> ParseSerial(CDataStream& s);

    virtual const uint256 signatureHash() const;
    bool HasValidSerial(ZerocoinParams* params) const;
    bool HasValidSignature() const;
    void setTxOutHash(uint256 txOutHash) { this->ptxHash = txOutHash; };
    void setDenom(libzerocoin::CoinDenomination denom) { this->denomination = denom; }
    void setPubKey(CPubKey pkey, bool fUpdateSerial = false);

    CBigNum CalculateValidSerial(ZerocoinParams* params);
    std::string ToString() const;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(denomination);
        READWRITE(ptxHash);
        READWRITE(accChecksum);
        READWRITE(accCommitmentToCoinValue);
        READWRITE(serialCommitmentToCoinValue);
        READWRITE(coinSerialNumber);
        READWRITE(accumulatorPoK);
        READWRITE(serialNumberSoK);
        READWRITE(commitmentPoK);

        try {
            READWRITE(version);
            READWRITE(pubkey);
            READWRITE(vchSig);
            READWRITE(spendType);
        } catch (...) {
            version = 1;
        }
    }

protected:
    CoinDenomination denomination = ZQ_ERROR;
    CBigNum coinSerialNumber;
    uint8_t version;
    //As of version 2
    CPubKey pubkey;
    std::vector<unsigned char> vchSig;
    SpendType spendType;
    uint256 ptxHash;

private:
    uint32_t accChecksum;
    CBigNum accCommitmentToCoinValue;
    CBigNum serialCommitmentToCoinValue;
    AccumulatorProofOfKnowledge accumulatorPoK;
    SerialNumberSignatureOfKnowledge serialNumberSoK;
    CommitmentProofOfKnowledge commitmentPoK;

};

} /* namespace libzerocoin */
#endif /* COINSPEND_H_ */
