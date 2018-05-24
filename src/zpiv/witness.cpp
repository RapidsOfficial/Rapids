#include <chainparams.h>
#include "witness.h"

void CoinWitnessData::SetNull()
{
    coin = nullptr;
    pAccumulator = nullptr;
    pWitness = nullptr;
    nMintsAdded = 0;
    nHeightMintAdded = 0;
    nHeightCheckpoint = 0;
    nHeightAccStart = 0;
    nHeightAccEnd = 0;
}

CoinWitnessData::CoinWitnessData()
{
    SetNull();
}

CoinWitnessData::CoinWitnessData(CZerocoinMint& mint)
{
    denom = mint.GetDenomination();
    isV1 = libzerocoin::ExtractVersionFromSerial(mint.GetSerialNumber()) < libzerocoin::PrivateCoin::PUBKEY_VERSION;
    libzerocoin::ZerocoinParams* paramsCoin = Params().Zerocoin_Params(isV1);
    coin = std::unique_ptr<libzerocoin::PublicCoin>(new libzerocoin::PublicCoin(paramsCoin, mint.GetValue(), denom));
    libzerocoin::Accumulator accumulator1(Params().Zerocoin_Params(false), denom);
    pWitness = std::unique_ptr<libzerocoin::AccumulatorWitness>(new libzerocoin::AccumulatorWitness(Params().Zerocoin_Params(false), accumulator1, *coin));
}

void CoinWitnessData::SetHeightMintAdded(int nHeight)
{
    nHeightMintAdded = nHeight;
    nHeightCheckpoint = nHeight + (10 - (nHeight % 10));
    nHeightAccStart = nHeight - (nHeight % 10);
}

