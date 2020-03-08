// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"

#include "init.h"
#include "coincontrol.h"
#include "consensus/zerocoin_verify.h"
#include "denomination_functions.h"
#include "primitives/transaction.h"
#include "script/sign.h"
#include "utilmoneystr.h"
#include "zpivchain.h"
#include "zpiv/deterministicmint.h"


/*
 * Legacy Zerocoin Wallet
 */

bool CWallet::AddDeterministicSeed(const uint256& seed)
{
    CWalletDB db(strWalletFile);
    std::string strErr;
    uint256 hashSeed = Hash(seed.begin(), seed.end());

    if(IsCrypted()) {
        if (!IsLocked()) { //if we have password

            CKeyingMaterial kmSeed(seed.begin(), seed.end());
            std::vector<unsigned char> vchSeedSecret;


            //attempt encrypt
            if (EncryptSecret(vMasterKey, kmSeed, hashSeed, vchSeedSecret)) {
                //write to wallet with hashSeed as unique key
                if (db.WriteZPIVSeed(hashSeed, vchSeedSecret)) {
                    return true;
                }
            }
            strErr = "encrypt seed";
        }
        strErr = "save since wallet is locked";
    } else { //wallet not encrypted
        if (db.WriteZPIVSeed(hashSeed, ToByteVector(seed))) {
            return true;
        }
        strErr = "save zpivseed to wallet";
    }
    //the use case for this is no password set seed, mint dzPIV,

    return error("s%: Failed to %s\n", __func__, strErr);
}

bool CWallet::GetDeterministicSeed(const uint256& hashSeed, uint256& seedOut)
{

    CWalletDB db(strWalletFile);
    std::string strErr;
    if (IsCrypted()) {
        if(!IsLocked()) { //if we have password

            std::vector<unsigned char> vchCryptedSeed;
            //read encrypted seed
            if (db.ReadZPIVSeed(hashSeed, vchCryptedSeed)) {
                uint256 seedRetrieved = uint256S(ReverseEndianString(HexStr(vchCryptedSeed)));
                //this checks if the hash of the seed we just read matches the hash given, meaning it is not encrypted
                //the use case for this is when not crypted, seed is set, then password set, the seed not yet crypted in memory
                if(hashSeed == Hash(seedRetrieved.begin(), seedRetrieved.end())) {
                    seedOut = seedRetrieved;
                    return true;
                }

                CKeyingMaterial kmSeed;
                //attempt decrypt
                if (DecryptSecret(vMasterKey, vchCryptedSeed, hashSeed, kmSeed)) {
                    seedOut = uint256S(ReverseEndianString(HexStr(kmSeed)));
                    return true;
                }
                strErr = "decrypt seed";
            } else { strErr = "read seed from wallet"; }
        } else { strErr = "read seed; wallet is locked"; }
    } else {
        std::vector<unsigned char> vchSeed;
        // wallet not crypted
        if (db.ReadZPIVSeed(hashSeed, vchSeed)) {
            seedOut = uint256S(ReverseEndianString(HexStr(vchSeed)));
            return true;
        }
        strErr = "read seed from wallet";
    }

    return error("%s: Failed to %s\n", __func__, strErr);
}

//- ZC Mints (Only for regtest)

std::string CWallet::MintZerocoin(CAmount nValue, CWalletTx& wtxNew, std::vector<CDeterministicMint>& vDMints, const CCoinControl* coinControl)
{
    if (!Params().IsRegTestNet())
        return _("Zerocoin minting available only on regtest");

    // Check amount
    if (nValue <= 0)
        return _("Invalid amount");

    CAmount nBalance = GetBalance();
    const CAmount& nFee = Params().GetConsensus().ZC_MinMintFee;
    if (nValue + nFee > nBalance) {
        LogPrintf("%s: balance=%s fee=%s nValue=%s\n", __func__, FormatMoney(nBalance), FormatMoney(nFee), FormatMoney(nValue));
        return _("Insufficient funds");
    }

    if (IsLocked()) {
        std::string strError = _("Error: Wallet locked, unable to create transaction!");
        LogPrintf("MintZerocoin() : %s", strError.c_str());
        return strError;
    }

    std::string strError;
    CReserveKey reservekey(this);
    CMutableTransaction txNew;
    if (!CreateZerocoinMintTransaction(nValue, txNew, vDMints, &reservekey, strError, coinControl)) {
        return strError;
    }

    wtxNew = CWalletTx(this, txNew);
    wtxNew.fFromMe = true;
    wtxNew.fTimeReceivedIsTxTime = true;

    // Limit size
    unsigned int nBytes = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);
    if (nBytes >= MAX_ZEROCOIN_TX_SIZE) {
        return _("Error: The transaction is larger than the maximum allowed transaction size!");
    }

    //commit the transaction to the network
    if (!CommitTransaction(wtxNew, reservekey)) {
        return _("Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
    } else {
        //update mints with full transaction hash and then database them
        CWalletDB walletdb(strWalletFile);
        for (CDeterministicMint dMint : vDMints) {
            dMint.SetTxHash(wtxNew.GetHash());
            zpivTracker->Add(dMint, true);
        }
    }

    return "";
}

std::string CWallet::MintZerocoinFromOutPoint(CAmount nValue, CWalletTx& wtxNew, std::vector<CDeterministicMint>& vDMints, const std::vector<COutPoint> vOutpts)
{
    CCoinControl* coinControl = new CCoinControl();
    for (const COutPoint& outpt : vOutpts) {
        coinControl->Select(outpt);
    }
    if (!coinControl->HasSelected()){
        std::string strError = _("Error: No valid utxo!");
        LogPrintf("MintZerocoin() : %s", strError.c_str());
        return strError;
    }
    std::string strError = MintZerocoin(nValue, wtxNew, vDMints, coinControl);
    delete coinControl;
    return strError;
}

bool CWallet::CreateZPIVOutPut(libzerocoin::CoinDenomination denomination, CTxOut& outMint, CDeterministicMint& dMint)
{
    // mint a new coin (create Pedersen Commitment) and extract PublicCoin that is shareable from it
    libzerocoin::PrivateCoin coin(Params().GetConsensus().Zerocoin_Params(false), denomination, false);
    zwalletMain->GenerateDeterministicZPIV(denomination, coin, dMint);

    libzerocoin::PublicCoin pubCoin = coin.getPublicCoin();

    // Validate
    if(!pubCoin.validate())
        return error("%s: newly created pubcoin is not valid", __func__);

    zwalletMain->UpdateCount();

    CScript scriptSerializedCoin = CScript() << OP_ZEROCOINMINT << pubCoin.getValue().getvch().size() << pubCoin.getValue().getvch();
    outMint = CTxOut(libzerocoin::ZerocoinDenominationToAmount(denomination), scriptSerializedCoin);

    return true;
}

// Given a set of inputs, find the public key that contributes the most coins to the input set
CScript GetLargestContributor(std::set<std::pair<const CWalletTx*, unsigned int> >& setCoins)
{
    std::map<CScript, CAmount> mapScriptsOut;
    for (const std::pair<const CWalletTx*, unsigned int>& coin : setCoins) {
        CTxOut out = coin.first->vout[coin.second];
        mapScriptsOut[out.scriptPubKey] += out.nValue;
    }

    CScript scriptLargest;
    CAmount nLargestContributor = 0;
    for (auto it : mapScriptsOut) {
        if (it.second > nLargestContributor) {
            scriptLargest = it.first;
            nLargestContributor = it.second;
        }
    }

    return scriptLargest;
}

bool CWallet::CreateZerocoinMintTransaction(const CAmount nValue,
                                            CMutableTransaction& txNew,
                                            std::vector<CDeterministicMint>& vDMints,
                                            CReserveKey* reservekey,
                                            std::string& strFailReason,
                                            const CCoinControl* coinControl)
{
    if (IsLocked()) {
        strFailReason = _("Error: Wallet locked, unable to create transaction!");
        LogPrintf("SpendZerocoin() : %s", strFailReason.c_str());
        return false;
    }

    //add multiple mints that will fit the amount requested as closely as possible
    CAmount nMintingValue = 0;
    CAmount nValueRemaining = 0;
    while (true) {
        //mint a coin with the closest denomination to what is being requested
        nValueRemaining = nValue - nMintingValue;

        libzerocoin::CoinDenomination denomination = libzerocoin::AmountToClosestDenomination(nValueRemaining, nValueRemaining);
        if (denomination == libzerocoin::ZQ_ERROR)
            break;

        CAmount nValueNewMint = libzerocoin::ZerocoinDenominationToAmount(denomination);
        nMintingValue += nValueNewMint;

        CTxOut outMint;
        CDeterministicMint dMint;
        if (!CreateZPIVOutPut(denomination, outMint, dMint)) {
            strFailReason = strprintf("%s: failed to create new zpiv output", __func__);
            return error(strFailReason.c_str());
        }
        txNew.vout.push_back(outMint);

        //store as CZerocoinMint for later use
        LogPrint(BCLog::LEGACYZC, "%s: new mint %s\n", __func__, dMint.ToString());
        vDMints.emplace_back(dMint);
    }

    // calculate fee
    CAmount nTotalValue = nValue + Params().GetConsensus().ZC_MinMintFee * txNew.vout.size();

    CAmount nValueIn = 0;
    std::set<std::pair<const CWalletTx*, unsigned int> > setCoins;
    // select UTXO's to use
    if (!SelectCoinsToSpend(nTotalValue, setCoins, nValueIn, coinControl)) {
        strFailReason = _("Insufficient or insufficient confirmed funds, you might need to wait a few minutes and try again.");
        return false;
    }

    // Fill vin
    for (const std::pair<const CWalletTx*, unsigned int>& coin : setCoins)
        txNew.vin.push_back(CTxIn(coin.first->GetHash(), coin.second));


    //any change that is less than 0.0100000 will be ignored and given as an extra fee
    //also assume that a zerocoinspend that is minting the change will not have any change that goes to Piv
    CAmount nChange = nValueIn - nTotalValue; // Fee already accounted for in nTotalValue
    if (nChange > 1 * CENT) {
        // Fill a vout to ourself using the largest contributing address
        CScript scriptChange = GetLargestContributor(setCoins);

        //add to the transaction
        CTxOut outChange(nChange, scriptChange);
        txNew.vout.push_back(outChange);
    } else if (reservekey) {
        reservekey->ReturnKey();
    }

    // Sign
    int nIn = 0;
    for (const std::pair<const CWalletTx*, unsigned int>& coin : setCoins) {
        if (!SignSignature(*this, *coin.first, txNew, nIn++)) {
            strFailReason = _("Signing transaction failed");
            return false;
        }
    }

    return true;
}

// - ZC PublicSpends

bool CWallet::SpendZerocoin(CAmount nAmount, CWalletTx& wtxNew, CZerocoinSpendReceipt& receipt, std::vector<CZerocoinMint>& vMintsSelected,
        std::list<std::pair<CBitcoinAddress*,CAmount>> addressesTo, CBitcoinAddress* changeAddress)
{
    // Default: assume something goes wrong. Depending on the problem this gets more specific below
    int nStatus = ZPIV_SPEND_ERROR;

    if (IsLocked()) {
        receipt.SetStatus("Error: Wallet locked, unable to create transaction!", ZPIV_WALLET_LOCKED);
        return false;
    }

    CReserveKey reserveKey(this);
    std::vector<CDeterministicMint> vNewMints;
    if (!CreateZCPublicSpendTransaction(
            nAmount,
            wtxNew,
            reserveKey,
            receipt,
            vMintsSelected,
            vNewMints,
            addressesTo,
            changeAddress
    )) {
        return false;
    }


    CWalletDB walletdb(strWalletFile);
    if (!CommitTransaction(wtxNew, reserveKey)) {
        LogPrintf("%s: failed to commit\n", __func__);
        nStatus = ZPIV_COMMIT_FAILED;

        //reset all mints
        for (CZerocoinMint mint : vMintsSelected) {
            uint256 hashPubcoin = GetPubCoinHash(mint.GetValue());
            zpivTracker->SetPubcoinNotUsed(hashPubcoin);
            NotifyZerocoinChanged(this, mint.GetValue().GetHex(), "New", CT_UPDATED);
        }

        //erase spends
        for (CZerocoinSpend spend : receipt.GetSpends()) {
            if (!walletdb.EraseZerocoinSpendSerialEntry(spend.GetSerial())) {
                receipt.SetStatus("Error: It cannot delete coin serial number in wallet", ZPIV_ERASE_SPENDS_FAILED);
            }

            //Remove from public zerocoinDB
            RemoveSerialFromDB(spend.GetSerial());
        }

        // erase new mints
        for (auto& dMint : vNewMints) {
            if (!walletdb.EraseDeterministicMint(dMint.GetPubcoinHash())) {
                receipt.SetStatus("Error: Unable to cannot delete zerocoin mint in wallet", ZPIV_ERASE_NEW_MINTS_FAILED);
            }
        }

        receipt.SetStatus("Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.", nStatus);
        return false;
    }

    //Set spent mints as used
    uint256 txidSpend = wtxNew.GetHash();
    for (CZerocoinMint mint : vMintsSelected) {
        uint256 hashPubcoin = GetPubCoinHash(mint.GetValue());
        zpivTracker->SetPubcoinUsed(hashPubcoin, txidSpend);

        CMintMeta metaCheck = zpivTracker->GetMetaFromPubcoin(hashPubcoin);
        if (!metaCheck.isUsed) {
            receipt.SetStatus("Error, the mint did not get marked as used", nStatus);
            return false;
        }
    }

    // write new Mints to db
    for (auto& dMint : vNewMints) {
        dMint.SetTxHash(txidSpend);
        zpivTracker->Add(dMint, true);
    }

    receipt.SetStatus("Spend Successful", ZPIV_SPEND_OKAY);  // When we reach this point spending zPIV was successful

    return true;
}

bool CWallet::MintsToInputVectorPublicSpend(std::map<CBigNum, CZerocoinMint>& mapMintsSelected, const uint256& hashTxOut, std::vector<CTxIn>& vin,
                                    CZerocoinSpendReceipt& receipt, libzerocoin::SpendType spendType, CBlockIndex* pindexCheckpoint)
{
    // Default error status if not changed below
    receipt.SetStatus(_("Transaction Mint Started"), ZPIV_TXMINT_GENERAL);

    // Get the chain tip to determine the active public spend version
    int nHeight = 0;
    {
        LOCK(cs_main);
        nHeight = chainActive.Height();
    }
    if (!nHeight)
        return error("%s: Unable to get chain tip height", __func__);

    int spendVersion = CurrentPublicCoinSpendVersion();

    for (auto &it : mapMintsSelected) {
        CZerocoinMint mint = it.second;

        // Create the simple input and the scriptSig -> Serial + Randomness + Private key signature of both.
        // As the mint doesn't have the output index search it..
        CTransaction txMint;
        uint256 hashBlock;
        if (!GetTransaction(mint.GetTxHash(), txMint, hashBlock)) {
            receipt.SetStatus(strprintf(_("Unable to find transaction containing mint %s"), mint.GetTxHash().GetHex()), ZPIV_TXMINT_GENERAL);
            return false;
        } else if (mapBlockIndex.count(hashBlock) < 1) {
            // check that this mint made it into the blockchain
            receipt.SetStatus(_("Mint did not make it into blockchain"), ZPIV_TXMINT_GENERAL);
            return false;
        }

        int outputIndex = -1;
        for (unsigned long i = 0; i < txMint.vout.size(); ++i) {
            CTxOut out = txMint.vout[i];
            if (out.scriptPubKey.IsZerocoinMint()){
                libzerocoin::PublicCoin pubcoin(Params().GetConsensus().Zerocoin_Params(false));
                CValidationState state;
                if (!TxOutToPublicCoin(out, pubcoin, state))
                    return error("%s: extracting pubcoin from txout failed", __func__);

                if (pubcoin.getValue() == mint.GetValue()){
                    outputIndex = i;
                    break;
                }
            }
        }

        if (outputIndex == -1) {
            receipt.SetStatus(_("Pubcoin not found in mint tx"), ZPIV_TXMINT_GENERAL);
            return false;
        }

        mint.SetOutputIndex(outputIndex);
        CTxIn in;
        if(!ZPIVModule::createInput(in, mint, hashTxOut, spendVersion)) {
            receipt.SetStatus(_("Cannot create public spend input"), ZPIV_TXMINT_GENERAL);
            return false;
        }
        vin.emplace_back(in);
        receipt.AddSpend(CZerocoinSpend(mint.GetSerialNumber(), UINT256_ZERO, mint.GetValue(), mint.GetDenomination(), 0));
    }

    receipt.SetStatus(_("Spend Valid"), ZPIV_SPEND_OKAY); // Everything okay

    return true;
}

bool CWallet::CreateZCPublicSpendTransaction(
        CAmount nValue,
        CWalletTx& wtxNew,
        CReserveKey& reserveKey,
        CZerocoinSpendReceipt& receipt,
        std::vector<CZerocoinMint>& vSelectedMints,
        std::vector<CDeterministicMint>& vNewMints,
        std::list<std::pair<CBitcoinAddress*,CAmount>> addressesTo,
        CBitcoinAddress* changeAddress)
{
    // Check available funds
    int nStatus = ZPIV_TRX_FUNDS_PROBLEMS;
    if (nValue > GetZerocoinBalance(true)) {
        receipt.SetStatus(_("You don't have enough Zerocoins in your wallet"), nStatus);
        return false;
    }

    if (nValue < 1) {
        receipt.SetStatus(_("Value is below the smallest available denomination (= 1) of zPIV"), nStatus);
        return false;
    }

    // Create transaction
    nStatus = ZPIV_TRX_CREATE;

    // If not already given pre-selected mints, then select mints from the wallet
    CWalletDB walletdb(strWalletFile);
    std::set<CMintMeta> setMints;
    CAmount nValueSelected = 0;
    int nCoinsReturned = 0; // Number of coins returned in change from function below (for debug)
    int nNeededSpends = 0;  // Number of spends which would be needed if selection failed
    const int nMaxSpends = Params().GetConsensus().ZC_MaxPublicSpendsPerTx; // Maximum possible spends for one zPIV public spend transaction
    std::vector<CMintMeta> vMintsToFetch;
    if (vSelectedMints.empty()) {
        //  All of the zPIV used in the public coin spend are mature by default (everything is public now.. no need to wait for any accumulation)
        setMints = zpivTracker->ListMints(true, false, true, true); // need to find mints to spend
        if(setMints.empty()) {
            receipt.SetStatus(_("Failed to find Zerocoins in wallet.dat"), nStatus);
            return false;
        }

        // If the input value is not an int, then we want the selection algorithm to round up to the next highest int
        double dValue = static_cast<double>(nValue) / static_cast<double>(COIN);
        bool fWholeNumber = floor(dValue) == dValue;
        CAmount nValueToSelect = nValue;
        if(!fWholeNumber)
            nValueToSelect = static_cast<CAmount>(ceil(dValue) * COIN);

        // Select the zPIV mints to use in this spend
        std::map<libzerocoin::CoinDenomination, CAmount> DenomMap = GetMyZerocoinDistribution();
        std::list<CMintMeta> listMints(setMints.begin(), setMints.end());
        vMintsToFetch = SelectMintsFromList(nValueToSelect, nValueSelected, nMaxSpends,
                                             nCoinsReturned, listMints, DenomMap, nNeededSpends);
        for (auto& meta : vMintsToFetch) {
            CZerocoinMint mint;
            if (!GetMint(meta.hashSerial, mint))
                return error("%s: failed to fetch hashSerial %s", __func__, meta.hashSerial.GetHex());
            vSelectedMints.emplace_back(mint);
        }
    } else {
        unsigned int mintsCount = 0;
        for (const CZerocoinMint& mint : vSelectedMints) {
            if (nValueSelected < nValue) {
                nValueSelected += ZerocoinDenominationToAmount(mint.GetDenomination());
                mintsCount ++;
            }
            else
                break;
        }
        if (mintsCount < vSelectedMints.size()) {
            vSelectedMints.resize(mintsCount);
        }
    }

    int nArchived = 0;
    for (CZerocoinMint mint : vSelectedMints) {
        // see if this serial has already been spent
        int nHeightSpend;
        if (IsSerialInBlockchain(mint.GetSerialNumber(), nHeightSpend)) {
            receipt.SetStatus(_("Trying to spend an already spent serial #, try again."), nStatus);
            uint256 hashSerial = GetSerialHash(mint.GetSerialNumber());
            if (!zpivTracker->HasSerialHash(hashSerial))
                return error("%s: tracker does not have serialhash %s", __func__, hashSerial.GetHex());

            CMintMeta meta = zpivTracker->Get(hashSerial);
            meta.isUsed = true;
            zpivTracker->UpdateState(meta);

            return false;
        }

        //check that this mint made it into the blockchain
        CTransaction txMint;
        uint256 hashBlock;
        bool fArchive = false;
        if (!GetTransaction(mint.GetTxHash(), txMint, hashBlock)) {
            receipt.SetStatus(strprintf(_("Unable to find transaction containing mint, txHash: %s"), mint.GetTxHash().GetHex()), nStatus);
            fArchive = true;
        } else if (mapBlockIndex.count(hashBlock) < 1) {
            receipt.SetStatus(_("Mint did not make it into blockchain"), nStatus);
            fArchive = true;
        }

        // archive this mint as an orphan
        if (fArchive) {
            //walletdb.ArchiveMintOrphan(mint);
            //nArchived++;
            //todo
        }
    }
    if (nArchived)
        return false;

    if (vSelectedMints.empty()) {
        if(nNeededSpends > 0){
            // Too much spends needed, so abuse nStatus to report back the number of needed spends
            receipt.SetStatus(_("Too many spends needed"), nStatus, nNeededSpends);
        }
        else {
            receipt.SetStatus(_("Failed to select a zerocoin"), nStatus);
        }
        return false;
    }


    if (static_cast<int>(vSelectedMints.size()) > nMaxSpends) {
        receipt.SetStatus(_("Failed to find coin set amongst held coins with less than maxNumber of Spends"), nStatus);
        return false;
    }


    // Create change if needed
    nStatus = ZPIV_TRX_CHANGE;

    CMutableTransaction txNew;
    wtxNew.BindWallet(this);
    {
        LOCK2(cs_main, cs_wallet);
        {
            txNew.vin.clear();
            txNew.vout.clear();

            CAmount nChange = nValueSelected - nValue;

            if (nChange < 0) {
                receipt.SetStatus(_("Selected coins value is less than payment target"), nStatus);
                return false;
            }

            if (nChange > 0 && !changeAddress && addressesTo.size() == 0) {
                receipt.SetStatus(_("Need destination or change address because change is not exact"), nStatus);
                return false;
            }

            //if there are addresses to send to then use them, if not generate a new address to send to
            CBitcoinAddress destinationAddr;
            if (addressesTo.size() == 0) {
                CPubKey pubkey;
                assert(reserveKey.GetReservedKey(pubkey)); // should never fail
                destinationAddr = CBitcoinAddress(pubkey.GetID());
                addressesTo.push_back(std::make_pair(&destinationAddr, nValue));
            }

            for (std::pair<CBitcoinAddress*,CAmount> pair : addressesTo){
                CScript scriptZerocoinSpend = GetScriptForDestination(pair.first->Get());
                //add output to pivx address to the transaction (the actual primary spend taking place)
                // TODO: check value?
                CTxOut txOutZerocoinSpend(pair.second, scriptZerocoinSpend);
                txNew.vout.push_back(txOutZerocoinSpend);
            }

            //add change output if we are spending too much (only applies to spending multiple at once)
            if (nChange) {
                CScript scriptChange;
                // Change address
                if(changeAddress){
                    scriptChange = GetScriptForDestination(changeAddress->Get());
                } else {
                    // Reserve a new key pair from key pool
                    CPubKey vchPubKey;
                    assert(reserveKey.GetReservedKey(vchPubKey)); // should never fail
                    scriptChange = GetScriptForDestination(vchPubKey.GetID());
                }
                //mint change as zerocoins
                CTxOut txOutChange(nValueSelected - nValue, scriptChange);
                txNew.vout.push_back(txOutChange);
            }

            //hash with only the output info in it to be used in Signature of Knowledge
            // and in CoinRandomness Schnorr Signature
            uint256 hashTxOut = txNew.GetHash();

            CBlockIndex* pindexCheckpoint = nullptr;
            std::map<CBigNum, CZerocoinMint> mapSelectedMints;
            for (const CZerocoinMint& mint : vSelectedMints)
                mapSelectedMints.insert(std::make_pair(mint.GetValue(), mint));

            //add all of the mints to the transaction as inputs
            std::vector<CTxIn> vin;
            if (!MintsToInputVectorPublicSpend(mapSelectedMints, hashTxOut, vin, receipt,
                                               libzerocoin::SpendType::SPEND, pindexCheckpoint))
                return false;
            txNew.vin = vin;

            // Limit size
            unsigned int nBytes = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);
            if (nBytes >= MAX_ZEROCOIN_TX_SIZE) {
                receipt.SetStatus(_("In rare cases, a spend with 7 coins exceeds our maximum allowable transaction size, please retry spend using 6 or less coins"), ZPIV_TX_TOO_LARGE);
                return false;
            }

            //now that all inputs have been added, add full tx hash to zerocoinspend records and write to db
            uint256 txHash = txNew.GetHash();
            for (CZerocoinSpend spend : receipt.GetSpends()) {
                spend.SetTxHash(txHash);

                if (!CWalletDB(strWalletFile).WriteZerocoinSpendSerialEntry(spend)) {
                    receipt.SetStatus(_("Failed to write coin serial number into wallet"), nStatus);
                }
            }

            //turn the finalized transaction into a wallet transaction
            wtxNew = CWalletTx(this, txNew);
            wtxNew.fFromMe = true;
            wtxNew.fTimeReceivedIsTxTime = true;
            wtxNew.nTimeReceived = GetAdjustedTime();
        }
    }

    receipt.SetStatus(_("Transaction Created"), ZPIV_SPEND_OKAY); // Everything okay

    return true;
}

// - ZC Balances

CAmount CWallet::GetZerocoinBalance(bool fMatureOnly) const
{
    if (fMatureOnly) {
        // This code is not removed just for when we back to use zPIV in the future, for now it's useless,
        // every public coin spend is now spendable without need to have new mints on top.

        //if (chainActive.Height() > nLastMaturityCheck)
        //nLastMaturityCheck = chainActive.Height();

        CAmount nBalance = 0;
        std::vector<CMintMeta> vMints = zpivTracker->GetMints(true);
        for (auto meta : vMints) {
            // Every public coin spend is now spendable, no need to mint new coins on top.
            //if (meta.nHeight >= mapMintMaturity.at(meta.denom) || meta.nHeight >= chainActive.Height() || meta.nHeight == 0)
            //    continue;
            nBalance += libzerocoin::ZerocoinDenominationToAmount(meta.denom);
        }
        return nBalance;
    }

    return zpivTracker->GetBalance(false, false);
}

CAmount CWallet::GetUnconfirmedZerocoinBalance() const
{
    return zpivTracker->GetUnconfirmedBalance();
}

CAmount CWallet::GetImmatureZerocoinBalance() const
{
    return GetZerocoinBalance(false) - GetZerocoinBalance(true) - GetUnconfirmedZerocoinBalance();
}

// Get a Map pairing the Denominations with the amount of Zerocoin for each Denomination
std::map<libzerocoin::CoinDenomination, CAmount> CWallet::GetMyZerocoinDistribution() const
{
    std::map<libzerocoin::CoinDenomination, CAmount> spread;
    for (const auto& denom : libzerocoin::zerocoinDenomList)
        spread.insert(std::pair<libzerocoin::CoinDenomination, CAmount>(denom, 0));
    {
        LOCK(cs_wallet);
        std::set<CMintMeta> setMints = zpivTracker->ListMints(true, true, true);
        for (auto& mint : setMints)
            spread.at(mint.denom)++;
    }
    return spread;
}

// zPIV wallet

void CWallet::setZWallet(CzPIVWallet* zwallet)
{
    zwalletMain = zwallet;
    zpivTracker = std::unique_ptr<CzPIVTracker>(new CzPIVTracker(this));
}

CzPIVWallet* CWallet::getZWallet()
{
    return zwalletMain;
}

bool CWallet::IsMyZerocoinSpend(const CBigNum& bnSerial) const
{
    return zpivTracker->HasSerial(bnSerial);
}

bool CWallet::IsMyMint(const CBigNum& bnValue) const
{
    if (zpivTracker->HasPubcoin(bnValue))
        return true;

    return zwalletMain->IsInMintPool(bnValue);
}

std::string CWallet::ResetMintZerocoin()
{
    long updates = 0;
    long deletions = 0;
    CWalletDB walletdb(strWalletFile);

    std::set<CMintMeta> setMints = zpivTracker->ListMints(false, false, true);
    std::vector<CMintMeta> vMintsToFind(setMints.begin(), setMints.end());
    std::vector<CMintMeta> vMintsMissing;
    std::vector<CMintMeta> vMintsToUpdate;

    // search all of our available data for these mints
    FindMints(vMintsToFind, vMintsToUpdate, vMintsMissing);

    // Update the meta data of mints that were marked for updating
    for (CMintMeta meta : vMintsToUpdate) {
        updates++;
        zpivTracker->UpdateState(meta);
    }

    // Delete any mints that were unable to be located on the blockchain
    for (CMintMeta mint : vMintsMissing) {
        deletions++;
        if (!zpivTracker->Archive(mint))
            LogPrintf("%s: failed to archive mint\n", __func__);
    }

    NotifyzPIVReset();

    std::string strResult = _("ResetMintZerocoin finished: ") + std::to_string(updates) + _(" mints updated, ") + std::to_string(deletions) + _(" mints deleted\n");
    return strResult;
}

std::string CWallet::ResetSpentZerocoin()
{
    long removed = 0;
    CWalletDB walletdb(strWalletFile);

    std::set<CMintMeta> setMints = zpivTracker->ListMints(false, false, true);
    std::list<CZerocoinSpend> listSpends = walletdb.ListSpentCoins();
    std::list<CZerocoinSpend> listUnconfirmedSpends;

    for (CZerocoinSpend spend : listSpends) {
        CTransaction tx;
        uint256 hashBlock = UINT256_ZERO;
        if (!GetTransaction(spend.GetTxHash(), tx, hashBlock)) {
            listUnconfirmedSpends.push_back(spend);
            continue;
        }

        //no confirmations
        if (hashBlock.IsNull())
            listUnconfirmedSpends.push_back(spend);
    }

    for (CZerocoinSpend spend : listUnconfirmedSpends) {
        for (CMintMeta meta : setMints) {
            if (meta.hashSerial == GetSerialHash(spend.GetSerial())) {
                removed++;
                meta.isUsed = false;
                zpivTracker->UpdateState(meta);
                walletdb.EraseZerocoinSpendSerialEntry(spend.GetSerial());
                continue;
            }
        }
    }

    NotifyzPIVReset();

    std::string strResult = _("ResetSpentZerocoin finished: ") + std::to_string(removed) + _(" unconfirmed transactions removed\n");
    return strResult;
}

bool IsMintInChain(const uint256& hashPubcoin, uint256& txid, int& nHeight)
{
    if (!IsPubcoinInBlockchain(hashPubcoin, txid))
        return false;

    uint256 hashBlock;
    CTransaction tx;
    if (!GetTransaction(txid, tx, hashBlock))
        return false;

    if (!mapBlockIndex.count(hashBlock) || !chainActive.Contains(mapBlockIndex.at(hashBlock)))
        return false;

    nHeight = mapBlockIndex.at(hashBlock)->nHeight;
    return true;
}

void CWallet::ReconsiderZerocoins(std::list<CZerocoinMint>& listMintsRestored, std::list<CDeterministicMint>& listDMintsRestored)
{
    CWalletDB walletdb(strWalletFile);
    std::list<CZerocoinMint> listMints = walletdb.ListArchivedZerocoins();
    std::list<CDeterministicMint> listDMints = walletdb.ListArchivedDeterministicMints();

    if (listMints.empty() && listDMints.empty())
        return;

    for (CZerocoinMint mint : listMints) {
        uint256 txid;
        int nHeight;
        uint256 hashPubcoin = GetPubCoinHash(mint.GetValue());
        if (!IsMintInChain(hashPubcoin, txid, nHeight))
            continue;

        mint.SetTxHash(txid);
        mint.SetHeight(nHeight);
        mint.SetUsed(IsSerialInBlockchain(mint.GetSerialNumber(), nHeight));

        if (!zpivTracker->UnArchive(hashPubcoin, false)) {
            LogPrintf("%s : failed to unarchive mint %s\n", __func__, mint.GetValue().GetHex());
        } else {
            zpivTracker->UpdateZerocoinMint(mint);
        }
        listMintsRestored.emplace_back(mint);
    }

    for (CDeterministicMint dMint : listDMints) {
        uint256 txid;
        int nHeight;
        if (!IsMintInChain(dMint.GetPubcoinHash(), txid, nHeight))
            continue;

        dMint.SetTxHash(txid);
        dMint.SetHeight(nHeight);
        uint256 txidSpend;
        dMint.SetUsed(IsSerialInBlockchain(dMint.GetSerialHash(), nHeight, txidSpend));

        if (!zpivTracker->UnArchive(dMint.GetPubcoinHash(), true)) {
            LogPrintf("%s : failed to unarchive deterministic mint %s\n", __func__, dMint.GetPubcoinHash().GetHex());
        } else {
            zpivTracker->Add(dMint, true);
        }
        listDMintsRestored.emplace_back(dMint);
    }
}

bool CWallet::GetZerocoinKey(const CBigNum& bnSerial, CKey& key)
{
    CWalletDB walletdb(strWalletFile);
    CZerocoinMint mint;
    if (!GetMint(GetSerialHash(bnSerial), mint))
        return error("%s: could not find serial %s in walletdb!", __func__, bnSerial.GetHex());

    return mint.GetKeyPair(key);
}

bool CWallet::GetMint(const uint256& hashSerial, CZerocoinMint& mint)
{
    if (!zpivTracker->HasSerialHash(hashSerial))
        return error("%s: serialhash %s is not in tracker", __func__, hashSerial.GetHex());

    CWalletDB walletdb(strWalletFile);
    CMintMeta meta = zpivTracker->Get(hashSerial);
    if (meta.isDeterministic) {
        CDeterministicMint dMint;
        if (!walletdb.ReadDeterministicMint(meta.hashPubcoin, dMint))
            return error("%s: failed to read deterministic mint", __func__);
        if (!zwalletMain->RegenerateMint(dMint, mint))
            return error("%s: failed to generate mint", __func__);

        return true;
    } else if (!walletdb.ReadZerocoinMint(meta.hashPubcoin, mint)) {
        return error("%s: failed to read zerocoinmint from database", __func__);
    }

    return true;
}

bool CWallet::GetMintFromStakeHash(const uint256& hashStake, CZerocoinMint& mint)
{
    CMintMeta meta;
    if (!zpivTracker->GetMetaFromStakeHash(hashStake, meta))
        return error("%s: failed to find meta associated with hashStake", __func__);
    return GetMint(meta.hashSerial, mint);
}

bool CWallet::DatabaseMint(CDeterministicMint& dMint)
{
    CWalletDB walletdb(strWalletFile);
    zpivTracker->Add(dMint, true);
    return true;
}

//! Primarily for the scenario that a mint was confirmed and added to the chain and then that block orphaned
bool CWallet::SetMintUnspent(const CBigNum& bnSerial)
{
    uint256 hashSerial = GetSerialHash(bnSerial);
    if (!zpivTracker->HasSerialHash(hashSerial))
        return error("%s: did not find mint", __func__);

    CMintMeta meta = zpivTracker->Get(hashSerial);
    zpivTracker->SetPubcoinNotUsed(meta.hashPubcoin);
    return true;
}

bool CWallet::UpdateMint(const CBigNum& bnValue, const int& nHeight, const uint256& txid, const libzerocoin::CoinDenomination& denom)
{
    uint256 hashValue = GetPubCoinHash(bnValue);
    CZerocoinMint mint;
    if (zpivTracker->HasPubcoinHash(hashValue)) {
        CMintMeta meta = zpivTracker->GetMetaFromPubcoin(hashValue);
        meta.nHeight = nHeight;
        meta.txid = txid;
        return zpivTracker->UpdateState(meta);
    } else {
        //Check if this mint is one that is in our mintpool (a potential future mint from our deterministic generation)
        if (zwalletMain->IsInMintPool(bnValue)) {
            if (zwalletMain->SetMintSeen(bnValue, nHeight, txid, denom))
                return true;
        }
    }

    return false;
}


