// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_test_fixture.h"

#include "rpc/server.h"
#include "wallet/db.h"
#include "wallet/wallet.h"

void clean()
{
    delete pwalletMain;
    pwalletMain = nullptr;

    bitdb.Flush(true);
    bitdb.Reset();
}

WalletTestingSetup::WalletTestingSetup(): TestingSetup()
{
    clean(); // todo: research why we have an initialized bitdb here.
    bitdb.MakeMock();

    bool fFirstRun;
    pwalletMain = new CWallet("test_wallet.dat");
    pwalletMain->LoadWallet(fFirstRun);
    RegisterValidationInterface(pwalletMain);

    // todo: back port method.
    //RegisterWalletRPCCommands(tableRPC);
}

WalletTestingSetup::~WalletTestingSetup()
{
    UnregisterValidationInterface(pwalletMain);
    clean();
}
