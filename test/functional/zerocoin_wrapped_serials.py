#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Covers the 'Wrapped Serials Attack' scenario for Zerocoin Spends
'''

import random
from time import sleep

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    sync_blocks,
    assert_equal,
    assert_raises_rpc_error,
    set_node_times,
    DecimalAmt
)

class zPIVwrappedSerialsTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 3
        # node 0 and node 1 move the chain (node 0 also sets the sporks)
        # node 2 does the spends
        self.extra_args = [['-staking=0']]*self.num_nodes
        self.extra_args[0].append('-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi')

    def setup_chain(self):
        # Start with PoS cache: 330 blocks
        self._initialize_chain(toPosPhase=True)
        self.enable_mocktime()

    def log_title(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        description = "Tests the 'Wrapped Serials Attack' scenario with for Zerocoin Spends"
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, description)

    def setV4SpendEnforcement(self, fEnable=True):
        sporkName = "SPORK_18_ZEROCOIN_PUBLICSPEND_V4"
        # update spork 18 with node[0]
        if fEnable:
            self.log.info("Enabling v4 PublicSpend version with SPORK 18...")
            res = self.activate_spork(0, sporkName)
        else:
            self.log.info("Enabling v3 PublicSpend version with SPORK 18...")
            res = self.deactivate_spork(0, sporkName)
        assert_equal(res, "success")
        sleep(1)
        # check that node[1] receives it
        assert_equal(fEnable, self.is_spork_active(1, sporkName))
        self.log.info("done")


    def run_test(self):

        def get_zerocoin_data(coin):
            return coin["s"], coin["r"], coin["k"], coin["id"], coin["d"], coin["t"]

        def check_balances(denom, zpiv_bal, piv_bal):
            zpiv_bal -= denom
            assert_equal(self.nodes[2].getzerocoinbalance()['Total'], zpiv_bal)
            piv_bal += denom
            wi = self.nodes[2].getwalletinfo()
            assert_equal(wi['balance'] + wi['immature_balance'], piv_bal)
            return zpiv_bal, piv_bal

        def stake_4_blocks(block_time):
            for peer in range(2):
                for i in range(2):
                    block_time = self.generate_pos(peer, block_time)
                sync_blocks(self.nodes)
            return block_time

        q = 73829871667027927151400291810255409637272593023945445234219354687881008052707
        pow2 = 2**256
        K_BITSIZE = 128  # bitsize of the range for random K
        self.log_title()
        block_time = self.mocktime
        set_node_times(self.nodes, block_time)

        # Start with cache balances
        wi = self.nodes[2].getwalletinfo()
        balance = wi['balance'] + wi['immature_balance']
        zpiv_balance = self.nodes[2].getzerocoinbalance()['Total']
        assert_equal(balance, DecimalAmt(13833.92))
        assert_equal(zpiv_balance, 6666)

        # Export zerocoin data
        listmints = self.nodes[2].listmintedzerocoins(True, True)
        serial_ids = [mint["serial hash"] for mint in listmints]
        exported_zerocoins = [x for x in self.nodes[2].exportzerocoins(False) if x["id"] in serial_ids]
        exported_zerocoins.sort(key=lambda x: x["d"], reverse=False)
        assert_equal(8, len(exported_zerocoins))

        # 1) Spend 1 coin and mine two more blocks
        serial_0, randomness_0, privkey_0, id_0, denom_0, tx_0 = get_zerocoin_data(exported_zerocoins[0])
        self.log.info("Spending the minted coin with serial %s..." % serial_0[:16])
        txid = self.nodes[2].spendzerocoin(denom_0, False, False, "", False)['txid']
        # stake 4 blocks - check it gets included on chain and check balances
        block_time = stake_4_blocks(block_time)
        self.check_tx_in_chain(0, txid)
        zpiv_balance, balance = check_balances(denom_0, zpiv_balance, balance)
        self.log.info("Coin spent.")

        # 2) create 5  new coins
        new_coins = []
        for i in range(5):
            K = random.getrandbits(K_BITSIZE)
            new_coins.append({
                "s": hex(int(serial_0, 16) + K*q*pow2)[2:],
                "r": randomness_0,
                "d": denom_0,
                "p": privkey_0,
                "t": tx_0})

        # 3) Spend the new zerocoins (V2)
        for c in new_coins:
            self.log.info("V2 - Spending the wrapping serial %s" % c["s"])
            assert_raises_rpc_error(-4, "CoinSpend: failed check",
                                    self.nodes[2].spendrawzerocoin, c["s"], c["r"], c["d"], c["p"], "", c["t"], False)
        self.log.info("GOOD: It was not possible")



if __name__ == '__main__':
    zPIVwrappedSerialsTest().main()
