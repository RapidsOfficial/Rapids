#!/usr/bin/env python3
# -*- coding: utf-8 -*-
'''
Covers the scenario of a valid PoS block where the coinstake input prevout is spent on main chain,
but not on the fork branch. These blocks must be accepted.
'''
from random import randint
import time

from test_framework.util import bytes_to_hex_str,assert_equal

from base_test import PIVX_FakeStakeTest
from util import dir_size

class Test_02(PIVX_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the scenario of a valid PoS block where the coinstake input prevout is spent on main chain, but not on the fork branch. These blocks must be accepted."
        self.init_test()
        INITAL_MINED_BLOCKS = 200   # First mined blocks (rewards collected to spend)
        FORK_DEPTH = 50             # number of blocks after INITIAL_MINED_BLOCKS before the coins are spent
        MORE_MINED_BLOCKS = 10      # number of blocks after spending of the collected coins
        self.NUM_BLOCKS = 3         # Number of spammed blocks

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks.." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)

        # 2) Collect the possible prevouts
        self.log.info("Collecting all unspent coins which we generated from mining...")
        utxo_list = self.node.listunspent()
        time.sleep(2)

        # 3) Mine more blocks
        self.log.info("Mining %d more blocks.." % (FORK_DEPTH+1))
        self.node.generate(FORK_DEPTH+1)
        time.sleep(2)

        # 4) Spend the coins collected in 2 (mined in the first 100 blocks)
        self.log.info("Spending the coins mined in the first %d blocks..." % INITAL_MINED_BLOCKS)
        stakingPrevOuts = self.get_prevouts(utxo_list)
        tx_hashes = self.spend_utxos(utxo_list)
        self.log.info("Spent %d transactions" % len(tx_hashes))
        time.sleep(2)

        # 5) Mine 10 more blocks
        self.log.info("Mining %d more blocks to include the TXs in chain..." % MORE_MINED_BLOCKS)
        self.node.generate(MORE_MINED_BLOCKS)
        time.sleep(2)

        # 6) Create "Fake Stake" blocks and send them
        self.log.info("Creating Fake stake blocks")
        err_msgs = self.test_spam("Fork", stakingPrevOuts, fRandomHeight=True, randomRange=FORK_DEPTH, randomRange2=MORE_MINED_BLOCKS-2, fMustPass=True)
        if not len(err_msgs) == 0:
            self.log.error("result: " + " | ".join(err_msgs))
            raise AssertionError("TEST FAILED")

        self.log.info("%s PASSED" % self.__class__.__name__)
