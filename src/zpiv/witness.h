#ifndef PIVX_WITNESS_H
#define PIVX_WITNESS_H


#include <libzerocoin/Accumulator.h>
#include <libzerocoin/Coin.h>
#include "zerocoin.h"

class CoinWitnessData
{
public:
    std::unique_ptr<libzerocoin::PublicCoin> coin;
    std::unique_ptr<libzerocoin::Accumulator> pAccumulator;
    std::unique_ptr<libzerocoin::AccumulatorWitness> pWitness;
    libzerocoin::CoinDenomination denom;
    int nHeightCheckpoint;
    int nHeightMintAdded;
    int nHeightAccStart;
    int nHeightAccEnd;
    int nMintsAdded;
    uint256 txid;
    bool isV1;

    CoinWitnessData();
    CoinWitnessData(CZerocoinMint& mint);
    void SetHeightMintAdded(int nHeight);
    void SetNull();
};

#endif //PIVX_WITNESS_H
