// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpiv/zpivmodule.h"
#include "zpivchain.h"
#include "chainparams.h"
#include "libzerocoin/Commitment.h"
#include "libzerocoin/Coin.h"
#include "hash.h"
#include "iostream"

bool PublicCoinSpend::HasValidSerial(libzerocoin::ZerocoinParams* params) const {
    return IsValidSerial(params, coinSerialNumber);
}

bool PublicCoinSpend::HasValidSignature() const {
    // Now check that the signature validates with the serial
    try {
        //V2 serial requires that the signature hash be signed by the public key associated with the serial
        uint256 hashedPubkey = Hash(pubkey.begin(), pubkey.end()) >> libzerocoin::PrivateCoin::V2_BITSHIFT;
        if (hashedPubkey != libzerocoin::GetAdjustedSerial(coinSerialNumber).getuint256()) {
            return error("%s: adjusted serial invalid\n", __func__);
        }
    } catch(std::range_error &e) {
        throw libzerocoin::InvalidSerialException("Serial longer than 256 bits");
    }

    if (!pubkey.Verify(hashTxOut, vchSig)){
        std::cout << "pubkey not verified" << std::endl;
        return error("%s: adjusted serial invalid\n", __func__);
    }
    return true;
}

bool ZPIVModule::createInput(CTxIn &in, CZerocoinMint mint, uint256 hashTxOut){
    uint8_t nVersion = mint.GetVersion();
    if (nVersion < libzerocoin::PrivateCoin::PUBKEY_VERSION) {
        // No v1 serials accepted anymore.
        return error("%s: failed to set zPIV privkey mint version=%d\n", __func__, nVersion);
    }
    CKey key;
    if (!mint.GetKeyPair(key))
        return error("%s: failed to set zPIV privkey mint version=%d\n", __func__, nVersion);

    std::vector<unsigned char> vchSig;
    if (!key.Sign(hashTxOut, vchSig))
        throw std::runtime_error("ZPIVModule failed to sign hashTxOut\n");

    CDataStream ser(SER_NETWORK, PROTOCOL_VERSION);
    PublicCoinSpend spend(mint.GetSerialNumber(),  mint.GetRandomness(), key.GetPubKey(), vchSig);
    ser << spend;

    std::vector<unsigned char> data(ser.begin(), ser.end());
    CScript scriptSigIn = CScript() << OP_ZEROCOINPUBLICSPEND << data.size();
    scriptSigIn.insert(scriptSigIn.end(), data.begin(), data.end());
    in = CTxIn(mint.GetTxHash(), mint.GetOutputIndex(), scriptSigIn, mint.GetDenomination());
    return true;
}

PublicCoinSpend ZPIVModule::parseCoinSpend(const CTxIn &in, const CTransaction& tx){
    if (!in.scriptSig.IsZerocoinPublicSpend()) throw runtime_error("parseCoinSpend() :: input is not a public coin spend");
    std::vector<char, zero_after_free_allocator<char> > data;
    data.insert(data.end(), in.scriptSig.begin() + 4, in.scriptSig.end());
    CDataStream serializedCoinSpend(data, SER_NETWORK, PROTOCOL_VERSION);
    PublicCoinSpend spend(serializedCoinSpend);
    spend.outputIndex = in.prevout.n;
    spend.txHash = in.prevout.hash;
    CTransaction txNew;
    txNew.vout = tx.vout;
    spend.hashTxOut = txNew.GetHash();
    return spend;
}

bool ZPIVModule::validateInput(const CTxIn &in, const CTxOut &prevOut, const CTransaction& tx, PublicCoinSpend& ret){
    if (!in.scriptSig.IsZerocoinPublicSpend() || !prevOut.scriptPubKey.IsZerocoinMint())
        return error("%s: not valid argument/s\n", __func__);

    // Now prove that the commitment value opens to the input
    PublicCoinSpend publicSpend = parseCoinSpend(in, tx);
    libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);

    // Check prev out now
    CValidationState state;
    libzerocoin::PublicCoin pubCoin(params);
    if (!TxOutToPublicCoin(prevOut, pubCoin, state))
        return error("%s: cannot get mint from output\n", __func__);
    publicSpend.pubCoin = &pubCoin;

    // Check that it opens to the input values
    libzerocoin::Commitment commitment(
            &params->coinCommitmentGroup, publicSpend.getCoinSerialNumber(), publicSpend.randomness);
    if (commitment.getCommitmentValue() != pubCoin.getValue()){
        return error("%s: commitments values are not equal\n", __func__);
    }
    ret = publicSpend;

    // Now check that the signature validates with the serial
    if (!publicSpend.HasValidSignature()) {
        return error("%s: signature invalid\n", __func__);;
    }

    return true;
}