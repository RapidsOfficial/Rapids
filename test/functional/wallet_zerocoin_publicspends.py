#!/usr/bin/env python3
# Copyright (c) 2019-2020 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Tests v2, v3 and v4 Zerocoin Spends
'''

from time import sleep

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    sync_blocks,
    sync_mempools,
    assert_equal,
    assert_raises_rpc_error,
    set_node_times,
    DecimalAmt
)


class ZerocoinSpendTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 3
        # node 0 and node 1 move the chain (node 0 also sets the sporks)
        # node 2 does the spends
        self.extra_args = [[]]*self.num_nodes
        self.extra_args[0].append('-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi')

    def setup_chain(self):
        # Start with PoS cache: 330 blocks
        self._initialize_chain(toPosPhase=True)
        self.enable_mocktime()

    def log_title(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        description = "Tests v2, v3 and v4 Zerocoin Spends."
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
            sync_mempools(self.nodes)
            for peer in range(2):
                for i in range(2):
                    block_time = self.generate_pos(peer, block_time)
                sync_blocks(self.nodes)
            return block_time

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

        # 1) stake more blocks - save a v3 spend for later (serial_1)
        serial_1, randomness_1, privkey_1, id_1, denom_1, tx_1 = get_zerocoin_data(exported_zerocoins[1])
        self.log.info("Staking 70 blocks to get to public spend activation")
        for j in range(5):
            for peer in range(2):
                for i in range(7):
                    block_time = self.generate_pos(peer, block_time)
                sync_blocks(self.nodes)
        old_spend_v3 = self.nodes[2].createrawzerocoinspend(id_1)

        # 2) Spend one minted coin - spend v3 (serial_2)
        serial_2, randomness_2, privkey_2, id_2, denom_2, tx_2 = get_zerocoin_data(exported_zerocoins[2])
        self.log.info("Spending the minted coin with serial %s..." % serial_2[:16])
        txid = self.nodes[2].spendzerocoinmints([id_2])['txid']
        # stake 4 blocks - check it gets included on chain and check balances
        block_time = stake_4_blocks(block_time)
        self.check_tx_in_chain(0, txid)
        zpiv_balance, balance = check_balances(denom_2, zpiv_balance, balance)
        self.log.info("--> VALID PUBLIC COIN SPEND (v3) PASSED")

        # 3) Check double spends - spend v3
        self.log.info("Trying to spend the serial twice now...")
        assert_raises_rpc_error(-4, "Trying to spend an already spent serial",
                                self.nodes[2].spendrawzerocoin, serial_2, randomness_2, denom_2, privkey_2, "", tx_2)


        # 4) Activate v4 spends with SPORK_18
        self.setV4SpendEnforcement()

        # 5) Spend one minted coin - spend v4 (serial_3)
        serial_3, randomness_3, privkey_3, id_3, denom_3, tx_3 = get_zerocoin_data(exported_zerocoins[3])
        self.log.info("Spending the minted coin with serial %s..." % serial_3[:16])
        txid = self.nodes[2].spendzerocoinmints([id_3])['txid']
        # stake 4 blocks - check it gets included on chain and check balances
        block_time = stake_4_blocks(block_time)
        self.check_tx_in_chain(0, txid)
        zpiv_balance, balance = check_balances(denom_3, zpiv_balance, balance)
        self.log.info("--> VALID PUBLIC COIN SPEND (v4) PASSED")

        # 6) Check double spends - spend v4
        self.log.info("Trying to spend the serial twice now...")
        assert_raises_rpc_error(-4, "Trying to spend an already spent serial",
                                self.nodes[2].spendrawzerocoin, serial_3, randomness_3, denom_3, privkey_3, "", tx_3)

        # 7) Try to relay old v3 spend now (serial_1)
        self.log.info("Trying to send old v3 spend now...")
        assert_raises_rpc_error(-26, "bad-zc-spend-version",
                                self.nodes[2].sendrawtransaction, old_spend_v3)
        self.log.info("GOOD: Old transaction not sent.")

        # 8) Try to double spend with v4 a mint already spent with v3 (serial_2)
        self.log.info("Trying to double spend v4 against v3...")
        assert_raises_rpc_error(-4, "Trying to spend an already spent serial",
                                self.nodes[2].spendrawzerocoin, serial_2, randomness_2, denom_2, privkey_2, "", tx_2)
        self.log.info("GOOD: Double-spending transaction did not verify.")

        # 9) Reactivate v3 spends and try to spend the old saved one (serial_1) again
        self.setV4SpendEnforcement(False)
        self.log.info("Trying to send old v3 spend now (serial: %s...)" % serial_1[:16])
        txid = self.nodes[2].sendrawtransaction(old_spend_v3)
        # stake 4 blocks - check it gets included on chain and check balances
        _ = stake_4_blocks(block_time)
        self.check_tx_in_chain(0, txid)
        # need to reset spent mints since this was a raw broadcast
        self.nodes[2].resetmintzerocoin()
        _, _ = check_balances(denom_1, zpiv_balance, balance)
        self.log.info("--> VALID PUBLIC COIN SPEND (v3) PASSED")


if __name__ == '__main__':
    ZerocoinSpendTest().main()
