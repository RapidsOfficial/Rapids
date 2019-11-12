#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from time import sleep
import urllib

from test_framework.authproxy import JSONRPCException
from test_framework.messages import COutPoint
from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    sync_blocks,
    assert_equal,
    assert_raises_rpc_error,
    connect_nodes_bi,
    connect_nodes_clique,
    disconnect_nodes,
    bytes_to_hex_str,
    set_node_times,
    DecimalAmt,
)

class ReorgStakeTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 3
        # node 0 and 1 stake the blocks, node 2 makes the zerocoin spends
        self.extra_args = [['-staking=0']] * self.num_nodes

    def setup_chain(self):
        # Start with PoS cache: 330 blocks
        self._initialize_chain(toPosPhase=True)
        self.enable_mocktime()

    def setup_network(self):
        # connect all nodes between each other
        self.setup_nodes()
        connect_nodes_clique(self.nodes)
        self.sync_all()

    def log_title(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        description = "Tests reorganisation for PoS blocks."
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, description)

    def disconnect_all(self):
        self.log.info("Disconnecting nodes...")
        for i in range(self.num_nodes):
            for j in range(self.num_nodes):
                if j != i:
                    disconnect_nodes(self.nodes[i], j)
        self.log.info("Nodes disconnected")

    def get_tot_balance(self, nodeid):
        wi = self.nodes[nodeid].getwalletinfo()
        return wi['balance'] + wi['immature_balance']


    def run_test(self):

        def findUtxoInList(txid, vout, utxo_list):
            for x in utxo_list:
                if x["txid"] == txid and x["vout"] == vout:
                    return True, x
            return False, None

        # Stake with node 0 and node 1 up to public spend activation (400)
        # 70 blocks: 5 blocks each (x7)
        self.log.info("Staking 70 blocks to reach public spends activation...")
        set_node_times(self.nodes, self.mocktime)
        for i in range(7):
            for peer in range(2):
                for nblock in range(5):
                    self.mocktime = self.generate_pos(peer, self.mocktime)
                sync_blocks(self.nodes)
                set_node_times(self.nodes, self.mocktime)
        block_time_0 = block_time_1 = self.mocktime
        self.log.info("Blocks staked.")

        # Check balances
        self.log.info("Checking balances...")
        initial_balance = [self.get_tot_balance(i) for i in range(self.num_nodes)]
        # --node 0: 64 pow blocks + 55 pos blocks
        assert_equal(initial_balance[0], DecimalAmt(29750))
        # --node 1: 62 pow blocks + 55 pos blocks
        assert_equal(initial_balance[1], DecimalAmt(29250))
        # --node 2: 62 pow blocks + 20 pos blocks - zc minted - zcfee
        assert_equal(initial_balance[2], DecimalAmt(13833.92))
        assert_equal(self.nodes[2].getzerocoinbalance()['Total'], DecimalAmt(6666))
        self.log.info("Balances ok.")

        # create the raw zerocoin spend txes
        self.log.info("Creating the raw zerocoin public spends...")
        mints = self.nodes[2].listmintedzerocoins(True, True)
        tx_A0 = self.nodes[2].createrawzerocoinspend(mints[0]["serial hash"])
        tx_A1 = self.nodes[2].createrawzerocoinspend(mints[1]["serial hash"])
        # Spending same coins to different recipients to get different txids
        my_addy = "yAVWM5urwaTyhiuFQHP2aP47rdZsLUG5PH"
        tx_B0 = self.nodes[2].createrawzerocoinspend(mints[0]["serial hash"], my_addy)
        tx_B1 = self.nodes[2].createrawzerocoinspend(mints[1]["serial hash"], my_addy)

        # Disconnect nodes
        self.disconnect_all()

        # Stake one block with node-0 and save the stake input
        self.log.info("Staking 1 block with node 0...")
        initial_unspent_0 = self.nodes[0].listunspent()
        self.nodes[0].generate(1)
        block_time_0 += 60
        set_node_times(self.nodes, block_time_0)
        last_block = self.nodes[0].getblock(self.nodes[0].getbestblockhash())
        assert(len(last_block["tx"]) > 1)                                       # a PoS block has at least two txes
        coinstake_txid = last_block["tx"][1]
        coinstake_tx = self.nodes[0].getrawtransaction(coinstake_txid, True)
        assert(coinstake_tx["vout"][0]["scriptPubKey"]["hex"] == "")            # first output of coinstake is empty
        stakeinput = coinstake_tx["vin"][0]

        # The stake input was unspent 1 block ago, now it's not
        res, utxo = findUtxoInList(stakeinput["txid"], stakeinput["vout"], initial_unspent_0)
        assert (res and utxo["spendable"])
        res, utxo = findUtxoInList(stakeinput["txid"], stakeinput["vout"], self.nodes[0].listunspent())
        assert (not res or not utxo["spendable"])
        self.log.info("Coinstake input %s...%s-%d is no longer spendable." % (
            stakeinput["txid"][:9], stakeinput["txid"][-4:], stakeinput["vout"]))

        # Relay zerocoin spends
        txid_A0 = self.nodes[0].sendrawtransaction(tx_A0)
        txid_A1 = self.nodes[0].sendrawtransaction(tx_A1)

        # Stake 10 more blocks with node-0 and check balances
        self.log.info("Staking 10 more blocks with node 0...")
        for i in range(10):
            block_time_0 = self.generate_pos(0, block_time_0)
        expected_balance_0 = initial_balance[0] + DecimalAmt(11 * 250.0)
        assert_equal(self.get_tot_balance(0), expected_balance_0)
        self.log.info("Balance for node 0 checks out.")

        # Connect with node 2, sync and check zerocoin balance
        connect_nodes_bi(self.nodes, 0, 2)
        sync_blocks([self.nodes[i] for i in [0, 2]])
        assert_equal(self.get_tot_balance(2), initial_balance[2] + DecimalAmt(6666))
        assert_equal(self.nodes[2].getzerocoinbalance()['Total'], DecimalAmt(0))

        # Double spending txes not possible
        assert_raises_rpc_error(-26, "bad-txns-invalid-zpiv",
                                self.nodes[0].sendrawtransaction, tx_B0)
        assert_raises_rpc_error(-26, "bad-txns-invalid-zpiv",
                                self.nodes[0].sendrawtransaction, tx_B1)

        # verify that the stakeinput can't be spent
        rawtx_unsigned = self.nodes[0].createrawtransaction(
            [{"txid": str(stakeinput["txid"]), "vout": int(stakeinput["vout"])}],
            {"xxncEuJK27ygNh7imNfaX8JV6ZQUnoBqzN": 249.99})
        rawtx = self.nodes[0].signrawtransaction(rawtx_unsigned)
        assert(rawtx["complete"])
        assert_raises_rpc_error(-25, "Missing inputs",self.nodes[0].sendrawtransaction, rawtx["hex"])

        # Spend tx_B0 and tx_B1 on the other chain
        txid_B0 = self.nodes[1].sendrawtransaction(tx_B0)
        txid_B1 = self.nodes[1].sendrawtransaction(tx_B1)

        # Stake 12 blocks with node-1
        set_node_times(self.nodes, block_time_1)
        self.log.info("Staking 12 blocks with node 1...")
        for i in range(12):
            block_time_1 = self.generate_pos(1, block_time_1)
        expected_balance_1 = initial_balance[1] + DecimalAmt(12 * 250.0)
        w_info = self.nodes[1].getwalletinfo()
        assert_equal(w_info["balance"] + w_info["immature_balance"], expected_balance_1)
        self.log.info("Balance for node 1 checks out.")

        # re-connect and sync nodes and check that node-0 and node-2 get on the other chain
        new_best_hash = self.nodes[1].getbestblockhash()
        self.log.info("Connecting and syncing nodes...")
        set_node_times(self.nodes, block_time_1)
        connect_nodes_clique(self.nodes)
        sync_blocks(self.nodes)
        for i in [0, 2]:
            assert_equal(self.nodes[i].getbestblockhash(), new_best_hash)

        # check that old zc spends have been removed and new one are present
        for i in range(self.num_nodes):
            print(i)
            try:
                print(self.nodes[i].getrawtransaction(txid_A0))
                print(self.nodes[i].getrawtransaction(txid_A1))
            except Exception as e:
                print(str(e))

        # check balance of node-0
        balance_0 = initial_balance[0]
        w_info = self.nodes[0].getwalletinfo()
        assert_equal(w_info["balance"] + w_info["immature_balance"], initial_balance[0])
        self.log.info("Balance for node 0 checks out.")

        # check that NOW the original stakeinput is present and spendable
        res, utxo = findUtxoInList(stakeinput["txid"], stakeinput["vout"], self.nodes[0].listunspent())
        assert (res and utxo["spendable"])
        self.log.info("Coinstake input %s...%s-%d is spendable again." % (
            stakeinput["txid"][:9], stakeinput["txid"][-4:], stakeinput["vout"]))
        self.nodes[0].sendrawtransaction(rawtx["hex"])
        self.nodes[1].generate(1)
        sync_blocks(self.nodes)
        res, utxo = findUtxoInList(stakeinput["txid"], stakeinput["vout"], self.nodes[0].listunspent())
        assert (not res or not utxo["spendable"])


if __name__ == '__main__':
    ReorgStakeTest().main()