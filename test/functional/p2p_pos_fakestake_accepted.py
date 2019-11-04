#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Covers the scenario of a valid PoS block where the coinstake input prevout is spent on main chain,
but not on the fork branch. These blocks must be accepted.
'''

from time import sleep

from fake_stake.base_test import PIVX_FakeStakeTest

class PoSFakeStakeAccepted(PIVX_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the scenario of a valid PoS block where the coinstake input prevout is spent on main chain, but not on the fork branch. These blocks must be accepted."
        self.init_test()

        INITAL_MINED_BLOCKS = 250   # First mined blocks
        FORK_DEPTH = 50             # number of blocks after INITIAL_MINED_BLOCKS before the coins are spent
        self.NUM_BLOCKS = 3         # Number of spammed blocks

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks.." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)

        # 2) Collect the utxos generated on first 50 blocks and lock them
        self.log.info("Collecting all unspent coins which we generated from mining...")
        staking_utxo_list = self.node.listunspent(INITAL_MINED_BLOCKS-50+1)
        sleep(2)
        assert(self.node.lockunspent(False, [{"txid": x['txid'], "vout": x['vout']} for x in staking_utxo_list]))

        # 3) Mine more blocks
        self.log.info("Mining %d more blocks.." % FORK_DEPTH)
        self.node.generate(FORK_DEPTH)
        sleep(2)

        # 4) Spend the coins collected in 2 (mined in the first 50 blocks)
        assert (self.node.lockunspent(True, [{"txid": x['txid'], "vout": x['vout']} for x in staking_utxo_list]))
        self.log.info("Spending the coins mined in the first 50 blocks...")
        tx_hashes = self.spend_utxos(staking_utxo_list)
        self.log.info("Spent %d transactions" % len(tx_hashes))
        sleep(2)

        # 5) Mine more blocks
        self.node.generate(1)
        sleep(2)

        # 6) Create "Fake Stake" blocks and send them
        self.log.info("Creating Fake stake blocks")
        err_msgs = self.test_spam("Fork",
                                  staking_utxo_list,
                                  fRandomHeight=True,
                                  randomRange=FORK_DEPTH,
                                  randomRange2=1,
                                  fMustPass=True)
        if not len(err_msgs) == 0:
            self.log.error("result: " + " | ".join(err_msgs))
            raise AssertionError("TEST FAILED")

        self.log.info("%s PASSED" % self.__class__.__name__)

if __name__ == '__main__':
    PoSFakeStakeAccepted().main()
