/**
 * @file       Accumulator.h
 *
 * @brief      Accumulator class for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017-2020 The PIVX developers

#ifndef ACCUMULATOR_H_
#define ACCUMULATOR_H_

#include "Coin.h"

namespace libzerocoin {
/**
 * \brief Implementation of the RSA-based accumulator.
 **/

class Accumulator {
public:
    template<typename Stream>
    Accumulator(const AccumulatorAndProofParams* p, Stream& strm):
        params(p)
    {
        strm >> *this;
    }

    template<typename Stream>
    Accumulator(const ZerocoinParams* p, Stream& strm)
    {
        strm >> *this;
        this->params = &(p->accumulatorParams);
    }

    Accumulator(const AccumulatorAndProofParams* p, const CoinDenomination d);
    Accumulator(const ZerocoinParams* p, const CoinDenomination d, CBigNum bnValue = 0);
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(value);
        READWRITE(denomination);
    }

    void accumulate(const PublicCoin &coin);
    void increment(const CBigNum& bnValue);
    CoinDenomination getDenomination() const;
    const CBigNum& getValue() const;
    void setValue(CBigNum bnValue);
    void setInitialValue();
    Accumulator& operator +=(const PublicCoin& c);
    bool operator==(const Accumulator rhs) const;

private:
    const AccumulatorAndProofParams* params;
    CBigNum value;
    CoinDenomination denomination;
};

} /* namespace libzerocoin */
#endif /* ACCUMULATOR_H_ */
