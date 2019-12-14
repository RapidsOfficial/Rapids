#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import sync_blocks, sync_mempools, connect_nodes_bi, \
    p2p_port, assert_equal, assert_raises_rpc_error
import urllib.parse

class ReorgStakeTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [["-minrelaytxfee=0.00001"],[]]


    def generateBatchBlocks(self, nodeid, limit, batch_size = 5):
        i = 0
        while i < limit:
            i += batch_size
            if i <= limit:
                self.nodes[nodeid].generate(batch_size)
            else:
                self.nodes[nodeid].generate(batch_size-i+limit)

    def findUtxoInList(self, txid, vout, utxo_list):
        for x in utxo_list:
            if x["txid"] == txid and x["vout"] == vout:
                return True, x
        return False, None


    def run_test(self):
        # NLAST_POW_BLOCK = 250 - so mine 125 blocks each node (25 consecutive blocks for 5 times)
        NMATURITY = 100
        self.log.info("Mining 250 blocks (125 with node 0 and 125 with node 1)...")
        for i in range(5):
            self.generateBatchBlocks(0, 25)
            sync_blocks(self.nodes)
            self.generateBatchBlocks(1, 25)
            sync_blocks(self.nodes)
        sync_mempools(self.nodes)

        # Check balances
        balance0 = 250.0 * (125 - 50)
        balance1 = 250.0 * (125 - 50)
        # Last two 25-blocks bursts (for each node) are not mature: NMATURITY = 2 * (2 * 25)
        immature_balance0 = 250.0 * 50
        immature_balance1 = 250.0 * 50
        w_info = self.nodes[0].getwalletinfo()
        assert_equal(w_info["balance"], balance0)
        assert_equal(w_info["immature_balance"], immature_balance0)
        self.log.info("Balance for node 0 checks out: %f [%f]" % (balance0, immature_balance0))
        w_info = self.nodes[1].getwalletinfo()
        assert_equal(w_info["balance"], balance1)
        assert_equal(w_info["immature_balance"], immature_balance1)
        self.log.info("Balance for node 1 checks out: %f [%f]" % (balance1, immature_balance1))
        initial_balance = balance0
        initial_immature_balance = immature_balance0
        initial_unspent = self.nodes[0].listunspent()

        # PoS start reached (block 250) - disconnect nodes
        self.nodes[0].disconnectnode(urllib.parse.urlparse(self.nodes[1].url).hostname + ":" + str(p2p_port(1)))
        self.nodes[1].disconnectnode(urllib.parse.urlparse(self.nodes[0].url).hostname + ":" + str(p2p_port(0)))
        self.log.info("Nodes disconnected")

        # Stake one block with node-0 and save the stake input
        self.log.info("Staking 1 block with node 0...")
        self.nodes[0].generate(1)
        last_block = self.nodes[0].getblock(self.nodes[0].getbestblockhash())
        assert(len(last_block["tx"]) > 1)                                       # a PoS block has at least two txes
        coinstake_txid = last_block["tx"][1]
        coinstake_tx = self.nodes[0].getrawtransaction(coinstake_txid, True)
        assert(coinstake_tx["vout"][0]["scriptPubKey"]["hex"] == "")            # first output of coinstake is empty
        stakeinput = coinstake_tx["vin"][0]

        # The stake input was unspent 1 block ago, now it's not
        res, utxo = self.findUtxoInList(stakeinput["txid"], stakeinput["vout"], initial_unspent)
        assert (res and utxo["spendable"])
        res, utxo = self.findUtxoInList(stakeinput["txid"], stakeinput["vout"], self.nodes[0].listunspent())
        assert (not res or not utxo["spendable"])
        self.log.info("Coinstake input %s...%s-%d is no longer spendable." % (
            stakeinput["txid"][:9], stakeinput["txid"][-4:], stakeinput["vout"]))

        # Stake 10 more blocks with node-0 and check balances
        self.log.info("Staking 10 more blocks with node 0...")
        self.generateBatchBlocks(0, 10)
        balance0 = initial_balance + 0          # mined blocks matured (250*11) - staked blocks inputs (250*11)
        immature_balance0 += 250 * 11           # -mined blocks matured (250*11) + staked blocks (500*11)
        w_info = self.nodes[0].getwalletinfo()
        assert_equal(w_info["balance"], balance0)
        assert_equal(w_info["immature_balance"], immature_balance0)
        self.log.info("Balance for node 0 checks out: %f [%f]" % (balance0, immature_balance0))

        # verify that the stakeinput can't be spent
        rawtx_unsigned = self.nodes[0].createrawtransaction(
            [{"txid": str(stakeinput["txid"]), "vout": int(stakeinput["vout"])}],
            {"xxncEuJK27ygNh7imNfaX8JV6ZQUnoBqzN": 249.99})
        rawtx = self.nodes[0].signrawtransaction(rawtx_unsigned)
        assert(rawtx["complete"])
        assert_raises_rpc_error(-25, "Missing inputs",self.nodes[0].sendrawtransaction, rawtx["hex"])

        # Stake 12 blocks with node-1
        self.log.info("Staking 12 blocks with node 1...")
        self.generateBatchBlocks(1, 12)
        balance1 -= 250 * 12                       # 0 - staked blocks inputs (250*12)
        immature_balance1 += 500 * 12              # + staked blocks (500 * 12)
        w_info = self.nodes[1].getwalletinfo()
        assert_equal(w_info["balance"], balance1)
        assert_equal(w_info["immature_balance"], immature_balance1)
        self.log.info("Balance for node 1 checks out: %f [%f]" % (balance1, immature_balance1))
        new_best_hash = self.nodes[1].getbestblockhash()

        # re-connect and sync nodes and check that node-0 gets on the other chain
        self.log.info("Connecting and syncing nodes...")
        connect_nodes_bi(self.nodes, 0, 1)
        sync_blocks(self.nodes)
        assert_equal(self.nodes[0].getbestblockhash(), new_best_hash)

        # check balance of node-0
        balance0 = initial_balance + 250 * 12                       # + mined blocks matured (250*12)
        immature_balance0 = initial_immature_balance - 250 * 12     # - mined blocks matured (250*12)
        w_info = self.nodes[0].getwalletinfo()
        assert_equal(w_info["balance"], balance0)                   # <--- !!! THIS FAILS before PR #1043
        assert_equal(w_info["immature_balance"], immature_balance0)
        self.log.info("Balance for node 0 checks out: %f [%f]" % (balance0, immature_balance0))

        # check that NOW the original stakeinput is present and spendable
        res, utxo = self.findUtxoInList(stakeinput["txid"], stakeinput["vout"], self.nodes[0].listunspent())
        assert (res and utxo["spendable"])                          # <--- !!! THIS FAILS before PR #1043
        self.log.info("Coinstake input %s...%s-%d is spendable again." % (
            stakeinput["txid"][:9], stakeinput["txid"][-4:], stakeinput["vout"]))
        self.nodes[0].sendrawtransaction(rawtx["hex"])
        self.nodes[1].generate(1)
        sync_blocks(self.nodes)
        res, utxo = self.findUtxoInList(stakeinput["txid"], stakeinput["vout"], self.nodes[0].listunspent())
        assert (not res or not utxo["spendable"])


if __name__ == '__main__':
    ReorgStakeTest().main()