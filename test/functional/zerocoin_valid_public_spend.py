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
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import p2p_port, assert_equal, assert_raises_rpc_error, assert_greater_than_or_equal


class zPIVValidCoinSpendTest(BitcoinTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [['-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi']]


    def setup_network(self):
        self.setup_nodes()


    def init_test(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, self.description)

        # Setup the p2p connections and start up the network thread.
        self.test_nodes = []
        for i in range(self.num_nodes):
            self.test_nodes.append(TestNode())
            self.test_nodes[i].peer_connect('127.0.0.1', p2p_port(i))


    def generateBlocks(self, n=1):
        fStaked = False
        while (not fStaked):
            try:
                self.nodes[0].generate(n)
                fStaked = True
            except JSONRPCException as e:
                if ("Couldn't create new block" in str(e)):
                    # Sleep 2 seconds and retry
                    self.log.info("waiting...")
                    sleep(2)
                else:
                    raise e


    def mintZerocoin(self, denom):
        self.nodes[0].mintzerocoin(denom)
        self.generateBlocks(5)


    def setV4SpendEnforcement(self, fEnable=True):
        new_val = 1563253447 if fEnable else 4070908800
        # update spork 18 and mine 1 more block
        mess = "Enabling v4" if fEnable else "Enabling v3"
        mess += " PublicSpend version with SPORK 18..."
        self.log.info(mess)
        res = self.nodes[0].spork("SPORK_18_ZEROCOIN_PUBLICSPEND_V4", new_val)
        self.log.info(res)
        assert_equal(res, "success")
        sleep(1)


    def run_test(self):
        self.description = "Tests a valid publicCoinSpend spend."
        self.init_test()

        INITAL_MINED_BLOCKS = 301   # Blocks mined before minting
        MORE_MINED_BLOCKS = 26      # Blocks mined after minting (before spending)
        DENOM_TO_USE = 1         # zc denomination used for double spending attack

        # 1) Start mining blocks
        self.log.info("Mining/Staking %d first blocks..." % INITAL_MINED_BLOCKS)
        self.generateBlocks(INITAL_MINED_BLOCKS)


        # 2) Mint zerocoins
        self.log.info("Minting %d-denom zPIVs..." % DENOM_TO_USE)
        for i in range(5):
            self.mintZerocoin(DENOM_TO_USE)


        # 3) Mine more blocks and collect the mint
        self.log.info("Mining %d more blocks..." % MORE_MINED_BLOCKS)
        self.generateBlocks(MORE_MINED_BLOCKS)
        list = self.nodes[0].listmintedzerocoins(True, True)
        serial_ids = [mint["serial hash"] for mint in list]
        assert_greater_than_or_equal(len(serial_ids), 3)


        # 4) Get the raw zerocoin data - save a v3 spend for later
        exported_zerocoins = self.nodes[0].exportzerocoins(False)
        zc = [x for x in exported_zerocoins if x["id"] in serial_ids]
        assert_greater_than_or_equal(len(zc), 3)
        saved_mint = zc[2]["id"]
        old_spend_v3 = self.nodes[0].createrawzerocoinpublicspend(saved_mint)


        # 5) Spend the minted coin (mine six more blocks) - spend v3
        serial_0 = zc[0]["s"]
        randomness_0 = zc[0]["r"]
        privkey_0 = zc[0]["k"]
        self.log.info("Spending the minted coin with serial %s and mining six more blocks..." % serial_0)
        txid = self.nodes[0].spendzerocoinmints([zc[0]["id"]])['txid']
        self.log.info("Spent on tx %s" % txid)
        self.generateBlocks(6)
        rawTx = self.nodes[0].getrawtransaction(txid, 1)
        if rawTx is None:
            self.log.warning("rawTx not found for: %s" % txid)
            raise AssertionError("TEST FAILED")
        else:
            assert_equal(rawTx["confirmations"], 6)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v3) PASSED" % self.__class__.__name__)


        # 6) Check double spends - spend v3
        self.log.info("%s: Trying to spend the serial twice now" % self.__class__.__name__)
        assert_raises_rpc_error(-4, "Trying to spend an already spent serial",
                                self.nodes[0].spendrawzerocoin, serial_0, randomness_0, DENOM_TO_USE, privkey_0)
        self.log.info("GOOD: Double-spending transaction did not verify.")


        # 7) Check spend v2 disabled
        self.log.info("%s: Trying to spend using the old coin spend method.." % self.__class__.__name__)
        try:
            res = self.nodes[0].spendzerocoin(DENOM_TO_USE, False, False, "", False)
        except JSONRPCException as e:
            # JSONRPCException was thrown as expected. Check the code and message values are correct.
            if e.error["code"] != -4:
                raise AssertionError("Unexpected JSONRPC error code %i" % e.error["code"])
            if ("Couldn't generate the accumulator witness" not in e.error['message'])\
                    and ("The transaction was rejected!" not in e.error['message']):
                raise AssertionError("Expected substring not found:" + e.error['message'])
        except Exception as e:
            raise AssertionError("Unexpected exception raised: " + type(e).__name__)
        self.log.info("GOOD: spendzerocoin old spend did not verify.")


        # 8) Activate v4 spends with SPORK_18
        self.log.info("Activating V4 spends with SPORK_18...")
        self.setV4SpendEnforcement(True)
        self.generateBlocks(2)


        # 9) Spend the minted coin (mine six more blocks) - spend v4
        serial_1 = zc[1]["s"]
        randomness_1 = zc[1]["r"]
        privkey_1 = zc[1]["k"]
        self.log.info("Spending the minted coin with serial %s and mining six more blocks..." % serial_1)
        txid = self.nodes[0].spendzerocoinmints([zc[1]["id"]])['txid']
        self.log.info("Spent on tx %s" % txid)
        self.generateBlocks(6)
        rawTx = self.nodes[0].getrawtransaction(txid, 1)
        if rawTx is None:
            self.log.warning("rawTx not found for: %s" % txid)
            raise AssertionError("TEST FAILED")
        else:
            assert_equal(rawTx["confirmations"], 6)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v4) PASSED" % self.__class__.__name__)


        # 10) Check double spends - spend v4
        self.log.info("%s: Trying to spend the serial twice now" % self.__class__.__name__)
        assert_raises_rpc_error(-4, "Trying to spend an already spent serial",
                                self.nodes[0].spendrawzerocoin, serial_1, randomness_1, DENOM_TO_USE, privkey_1)
        self.log.info("GOOD: Double-spending transaction did not verify.")


        # 11) Try to relay old v3 spend now
        self.log.info("%s: Trying to send old v3 spend now" % self.__class__.__name__)
        assert_raises_rpc_error(-26, "bad-txns-invalid-zpiv",
                                self.nodes[0].sendrawtransaction, old_spend_v3)
        self.log.info("GOOD: Old transaction not sent.")


        # 12) Try to double spend with v4 a mint already spent with v3
        self.log.info("%s: Trying to double spend v4 against v3" % self.__class__.__name__)
        assert_raises_rpc_error(-4, "Trying to spend an already spent serial",
                                self.nodes[0].spendrawzerocoin, serial_0, randomness_0, DENOM_TO_USE, privkey_0)
        self.log.info("GOOD: Double-spending transaction did not verify.")


        # 13) Reactivate v3 spends and try to spend the old saved one
        self.log.info("Activating V3 spends with SPORK_18...")
        self.setV4SpendEnforcement(False)
        self.generateBlocks(2)
        self.log.info("%s: Trying to send old v3 spend now" % self.__class__.__name__)
        txid = self.nodes[0].sendrawtransaction(old_spend_v3)
        self.log.info("Spent on tx %s" % txid)
        self.generateBlocks(6)
        rawTx = self.nodes[0].getrawtransaction(txid, 1)
        if rawTx is None:
            self.log.warning("rawTx not found for: %s" % txid)
            raise AssertionError("TEST FAILED")
        else:
            assert_equal(rawTx["confirmations"], 6)
        self.log.info("%s: VALID PUBLIC COIN SPEND (v3) PASSED" % self.__class__.__name__)




if __name__ == '__main__':
    zPIVValidCoinSpendTest().main()
