#!/usr/bin/env python3

from test_framework.test_framework import PivxTestFramework
from test_framework.util import *

"""
Simple test checking chain movement after v5 enforcement.
"""

class MiningV5UpgradeTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[]]
        self.setup_clean_chain = True

    def run_test(self):
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['v5 dummy']['status'], 'pending')
        self.nodes[0].generate(300) # v5 activation height
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['v5 dummy']['status'], 'active')
        self.nodes[0].generate(25) # 25 more to check chain movement
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['v5 dummy']['status'], 'active')
        assert_equal(self.nodes[0].getblockcount(), 325)


if __name__ == '__main__':
    MiningV5UpgradeTest().main()