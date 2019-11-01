#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Tests a valid publicCoinSpend spend
'''


from time import sleep

from fake_stake.util import TestNode

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import PivxTestFramework
from test_framework.util import sync_chain, assert_equal, assert_raises_rpc_error, \
    assert_greater_than, generate_pos, set_node_times


class zPIVValidCoinSpendTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        # node 0 moves the chain and sets the sporks - node 1 does the spends
        self.extra_args = [['-staking=0', '-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi'],
                           ['-staking=0']]

    def setup_chain(self):
        # Start with PoS cache: 330 blocks
        self._initialize_chain(toPosPhase=True)
        self.enable_mocktime()

    def log_title(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        description = "Tests a valid publicCoinSpend spend."
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, description)

    def setV4SpendEnforcement(self, btime, fEnable=True):
        new_val = 1563253447 if fEnable else 4070908800
        # update spork 18 with node[0]
        mess = "Enabling v4" if fEnable else "Enabling v3"
        mess += " PublicSpend version with SPORK 18..."
        self.log.info(mess)
        res = self.nodes[0].spork("SPORK_18_ZEROCOIN_PUBLICSPEND_V4", new_val)
        assert_equal(res, "success")
        sleep(1)
        # check that node[1] receives it
        assert_equal(new_val, self.nodes[1].spork("show")["SPORK_18_ZEROCOIN_PUBLICSPEND_V4"])
        self.log.info("done")

    def run_test(self):
        self.log_title()
        block_time = self.mocktime
        set_node_times(self.nodes, block_time)
        DENOM = 1  # zc denomination used for mints

        # 1) Mint 5 zerocoins
        self.log.info("Minting 5 %d-denom zPIVs in successive blocks..." % DENOM)
        for i in range(5):
            self.nodes[1].mintzerocoin(DENOM)
            block_time = generate_pos(self.nodes, 0, block_time)
        sync_chain(self.nodes)

        # 2) Stake 25 blocks (till block 360) to mature the mints (GetMintMaturityHeight)
        # 359 - 20 - (339 % 10) = 330 (no mints yet)
        # 360 - 20 - (340 % 10) = 340 (all mints included)
        self.log.info("Staking 25 blocks to mature the mints")
        for i in range(24):
            block_time = generate_pos(self.nodes, 0, block_time)
        sync_chain(self.nodes)
        # stakes are still immature
        assert_equal(5, len(self.nodes[1].listmintedzerocoins(True, False)))
        assert_equal(0, len(self.nodes[1].listmintedzerocoins(True, True)))
        block_time = generate_pos(self.nodes, 0, block_time)
        sync_chain(self.nodes)
        list = self.nodes[1].listmintedzerocoins(True, True)
        # mature now (except last one added)
        assert_equal(5, len(self.nodes[1].listmintedzerocoins(True, False)))
        assert_equal(4, len(list))
        self.log.info("collecting the zerocoins...")
        serial_ids = [mint["serial hash"] for mint in list]

        # 3) Get the raw zerocoin data - save a v3 spend for later
        exported_zerocoins = [x for x in self.nodes[1].exportzerocoins(False) if x["id"] in serial_ids]
        assert_equal(4, len(exported_zerocoins))
        saved_mint = exported_zerocoins[2]["id"]
        old_spend_v3 = self.nodes[1].createrawzerocoinpublicspend(saved_mint)

        # 4) Spend one minted coin - spend v3
        def get_zerocoin_data(coin):
            return coin["s"], coin["r"], coin["k"], coin["id"], coin["d"], coin["t"]
        serial_0, randomness_0, privkey_0, id_0, denom_0, tx_0 = get_zerocoin_data(exported_zerocoins[0])
        self.log.info("Spending the minted coin with serial %s..." % serial_0)
        txid = self.nodes[1].spendzerocoinmints([id_0])['txid']
        self.log.info("Spent on tx %s" % txid)
        self.log.info("Staking 5 blocks to include the spend...")
        for i in range(5):
            block_time = generate_pos(self.nodes, 0, block_time)
        sync_chain(self.nodes)
        rawTx = self.nodes[0].getrawtransaction(txid, 1)
        assert_greater_than(rawTx["confirmations"], 0)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v3) PASSED" % self.__class__.__name__)

        # 5) Check double spends - spend v3
        self.log.info("%s: Trying to spend the serial twice now" % self.__class__.__name__)
        assert_raises_rpc_error(-4, "Trying to spend an already spent serial",
                                self.nodes[1].spendrawzerocoin, serial_0, randomness_0, denom_0, privkey_0, "", tx_0)
        self.log.info("GOOD: Double-spending transaction did not verify.")

        # 6) Check spend v2 disabled
        self.log.info("%s: Trying to spend using the old coin spend method.." % self.__class__.__name__)
        try:
            self.nodes[1].spendzerocoin(DENOM, False, False, "", False)
        except JSONRPCException as e:
            # JSONRPCException was thrown as expected. Check the code and message values are correct.
            if e.error["code"] != -4:
                raise AssertionError("Unexpected JSONRPC error code %i" % e.error["code"])
            if ([x for x in ["Couldn't generate the accumulator witness",
                             "The transaction was rejected!"] if x in e.error['message']] == []):
                raise AssertionError("Expected substring not found:" + e.error['message'])
        except Exception as e:
            raise AssertionError("Unexpected exception raised: " + type(e).__name__)
        self.log.info("GOOD: spendzerocoin old spend did not verify.")

        # 7) Activate v4 spends with SPORK_18
        self.log.info("Activating V4 spends with SPORK_18...")
        self.setV4SpendEnforcement(block_time)

        # 8) Spend the minted coin - spend v4
        serial_1, randomness_1, privkey_1, id_1, denom_1, tx_1 = get_zerocoin_data(exported_zerocoins[1])
        self.log.info("Spending the minted coin with serial %s..." % serial_0)
        txid = self.nodes[1].spendzerocoinmints([id_1])['txid']
        self.log.info("Spent on tx %s" % txid)
        self.log.info("Staking 5 blocks to include the spend...")
        for i in range(5):
            block_time = generate_pos(self.nodes, 0, block_time)
        sync_chain(self.nodes)
        rawTx = self.nodes[0].getrawtransaction(txid, 1)
        assert_greater_than(rawTx["confirmations"], 0)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v4) PASSED" % self.__class__.__name__)

        # 9) Check double spends - spend v4
        self.log.info("%s: Trying to spend the serial twice now" % self.__class__.__name__)
        assert_raises_rpc_error(-4, "Trying to spend an already spent serial",
                                self.nodes[1].spendrawzerocoin, serial_1, randomness_1, denom_1, privkey_1, "", tx_1)
        self.log.info("GOOD: Double-spending transaction did not verify.")

        # 10) Try to relay old v3 spend now
        self.log.info("%s: Trying to send old v3 spend now" % self.__class__.__name__)
        assert_raises_rpc_error(-26, "bad-txns-invalid-zpiv",
                                self.nodes[1].sendrawtransaction, old_spend_v3)
        self.log.info("GOOD: Old transaction not sent.")

        # 11) Try to double spend with v4 a mint already spent with v3
        self.log.info("%s: Trying to double spend v4 against v3" % self.__class__.__name__)
        assert_raises_rpc_error(-4, "Trying to spend an already spent serial",
                                self.nodes[1].spendrawzerocoin, serial_0, randomness_0, denom_0, privkey_0, "", tx_0)
        self.log.info("GOOD: Double-spending transaction did not verify.")

        # 12) Reactivate v3 spends and try to spend the old saved one
        self.log.info("Activating V3 spends with SPORK_18...")
        self.setV4SpendEnforcement(block_time, False)
        self.log.info("%s: Trying to send old v3 spend now" % self.__class__.__name__)
        txid = self.nodes[1].sendrawtransaction(old_spend_v3)
        self.log.info("Spent on tx %s" % txid)
        self.log.info("Staking 5 blocks to include the spend...")
        for i in range(5):
            block_time = generate_pos(self.nodes, 0, block_time)
        sync_chain(self.nodes)
        rawTx = self.nodes[0].getrawtransaction(txid, 2)
        assert_greater_than(rawTx["confirmations"], 0)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v3) PASSED" % self.__class__.__name__)


if __name__ == '__main__':
    zPIVValidCoinSpendTest().main()
