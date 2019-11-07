#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
# -*- coding: utf-8 -*-

from io import BytesIO
from time import sleep

from test_framework.authproxy import JSONRPCException
from test_framework.messages import CTransaction, CTxIn, CTxOut, COIN, COutPoint
from test_framework.mininode import network_thread_start
from test_framework.pivx_node import PivxTestNode
from test_framework.script import CScript, OP_CHECKSIG
from test_framework.test_framework import PivxTestFramework
from test_framework.util import connect_nodes_bi, p2p_port, bytes_to_hex_str, \
    assert_equal, assert_greater_than, sync_blocks, assert_raises_rpc_error

# filter utxos based on first 5 bytes of scriptPubKey
def getDelegatedUtxos(utxos):
    return [x for x in utxos if x["scriptPubKey"][:10] == '76a97b63d1']


class PIVX_ColdStakingTest(PivxTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [['-staking=1']] * self.num_nodes
        self.extra_args[0].append('-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi')


    def setup_network(self):
        ''' Can't rely on syncing all the nodes when staking=1
        '''
        self.setup_nodes()
        for i in range(self.num_nodes - 1):
            for j in range(i+1, self.num_nodes):
                connect_nodes_bi(self.nodes, i, j)

    def init_test(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, self.description)
        self.DEFAULT_FEE = 0.05
        # Setup the p2p connections and start up the network thread.
        self.test_nodes = []
        for i in range(self.num_nodes):
            self.test_nodes.append(PivxTestNode())
            self.test_nodes[i].peer_connect('127.0.0.1', p2p_port(i))

        network_thread_start()  # Start up network handling in another thread

        # Let the test nodes get in sync
        for i in range(self.num_nodes):
            self.test_nodes[i].wait_for_verack()

    def setColdStakingEnforcement(self, fEnable=True):
        sporkName = "SPORK_17_COLDSTAKING_ENFORCEMENT"
        # update spork 17 with node[0]
        if fEnable:
            self.log.info("Enabling cold staking with SPORK 17...")
            res = self.activate_spork(0, sporkName)
        else:
            self.log.info("Disabling cold staking with SPORK 17...")
            res = self.deactivate_spork(0, sporkName)
        assert_equal(res, "success")
        sleep(1)
        # check that node[1] receives it
        assert_equal(fEnable, self.is_spork_active(1, sporkName))
        self.log.info("done")

    def isColdStakingEnforced(self):
        # verify from node[1]
        return self.is_spork_active(1, "SPORK_17_COLDSTAKING_ENFORCEMENT")



    def run_test(self):
        self.description = "Performs tests on the Cold Staking P2CS implementation"
        self.init_test()
        LAST_POW_BLOCK = 250
        NUM_OF_INPUTS = 20
        INPUT_VALUE = 50
        INITAL_MINED_BLOCKS = LAST_POW_BLOCK + 1

        # nodes[0] - coin-owner
        # nodes[1] - cold-staker

        # 1) nodes[0] mines 20 blocks. nodes[2] mines 231 blocks.
        # -----------------------------------------------------------
        # Check that SPORK 17 is disabled
        assert (not self.isColdStakingEnforced())
        print("*** 1 ***")
        self.log.info("Mining %d blocks..." % INITAL_MINED_BLOCKS)
        self.generateBlock(20, 0)
        sync_blocks(self.nodes)
        self.log.info("20 Blocks mined.")
        self.generateBlock(INITAL_MINED_BLOCKS-20)
        sync_blocks(self.nodes)
        self.log.info("251 Blocks mined.")

        # 2) nodes[0] generates a owner address
        #    nodes[1] generates a cold-staking address.
        # ---------------------------------------------
        print("*** 2 ***")
        owner_address = self.nodes[0].getnewaddress()
        self.log.info("Owner Address: %s" % owner_address)
        staker_address = self.nodes[1].getnewstakingaddress()
        staker_privkey = self.nodes[1].dumpprivkey(staker_address)
        self.log.info("Staking Address: %s" % staker_address)

        # 3) Check enforcement.
        # ---------------------
        print("*** 3 ***")
        self.log.info("Creating a stake-delegation tx before cold staking enforcement...")
        assert_raises_rpc_error(-4, "The transaction was rejected!",
                                self.nodes[0].delegatestake, staker_address, INPUT_VALUE, owner_address, False, False, True)
        self.log.info("Good. Cold Staking NOT ACTIVE yet.")

        # Enable SPORK
        self.setColdStakingEnforcement()
        # double check
        assert (self.isColdStakingEnforced())

        # 4) nodes[0] delegates a number of inputs for nodes[1] to stake em.
        # ------------------------------------------------------------------
        print("*** 4 ***")
        self.log.info("First check warning when using external addresses...")
        assert_raises_rpc_error(-5, "Only the owner of the key to owneraddress will be allowed to spend these coins",
                                self.nodes[0].delegatestake, staker_address, INPUT_VALUE, "yCgCXC8N5VThhfiaVuKaNLkNnrWduzVnoT")
        self.log.info("Good. Warning triggered.")

        self.log.info("Now force the use of external address creating (but not sending) the delegation...")
        res = self.nodes[0].rawdelegatestake(staker_address, INPUT_VALUE, "yCgCXC8N5VThhfiaVuKaNLkNnrWduzVnoT", True)
        assert(res is not None and res != "")
        self.log.info("Good. Warning NOT triggered.")

        self.log.info("Now delegate with internal owner address..")
        self.log.info("Try first with a value (0.99) below the threshold")
        assert_raises_rpc_error(-8, "Invalid amount",
                                self.nodes[0].delegatestake, staker_address, 0.99, owner_address)
        self.log.info("Nice. it was not possible.")
        self.log.info("Then try (creating but not sending) with the threshold value (1.00)")
        res = self.nodes[0].rawdelegatestake(staker_address, 1.00, owner_address)
        assert(res is not None and res != "")
        self.log.info("Good. Warning NOT triggered.")

        self.log.info("Now creating %d real stake-delegation txes..." % NUM_OF_INPUTS)
        for i in range(NUM_OF_INPUTS):
            res = self.nodes[0].delegatestake(staker_address, INPUT_VALUE, owner_address)
            assert(res != None and res["txid"] != None and res["txid"] != "")
            assert_equal(res["owner_address"], owner_address)
            assert_equal(res["staker_address"], staker_address)
        self.generateBlock()
        sync_blocks(self.nodes)
        self.log.info("%d Txes created." % NUM_OF_INPUTS)
        # check balances:
        self.expected_balance = NUM_OF_INPUTS * INPUT_VALUE
        self.expected_immature_balance = 0
        self.checkBalances()

        # 5) check that the owner (nodes[0]) can spend the coins.
        # -------------------------------------------------------
        print("*** 5 ***")
        self.log.info("Spending back one of the delegated UTXOs...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        assert_equal(20, len(delegated_utxos))
        assert_equal(len(delegated_utxos), len(self.nodes[0].listcoldutxos()))
        u = delegated_utxos[0]
        txhash = self.spendUTXOwithNode(u, 0)
        assert(txhash != None)
        self.log.info("Good. Owner was able to spend - tx: %s" % str(txhash))

        self.generateBlock()
        sync_blocks(self.nodes)
        # check balances after spend.
        self.expected_balance -= float(u["amount"])
        self.checkBalances()
        self.log.info("Balances check out after spend")
        assert_equal(19, len(self.nodes[0].listcoldutxos()))

        # 6) check that the staker CANNOT use the coins to stake yet.
        # He needs to whitelist the owner first.
        # -----------------------------------------------------------
        print("*** 6 ***")
        self.log.info("Trying to generate a cold-stake block before whitelisting the owner...")
        assert_equal(self.nodes[1].getstakingstatus()["mintablecoins"], False)
        self.log.info("Nice. Cold staker was NOT able to create the block yet.")

        self.log.info("Whitelisting the owner...")
        ret = self.nodes[1].delegatoradd(owner_address)
        assert(ret)
        self.log.info("Delegator address %s whitelisted" % owner_address)

        # 7) check that the staker CANNOT spend the coins.
        # ------------------------------------------------
        print("*** 7 ***")
        self.log.info("Trying to spend one of the delegated UTXOs with the cold-staking key...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        assert_greater_than(len(delegated_utxos), 0)
        u = delegated_utxos[0]
        assert_raises_rpc_error(-26, "mandatory-script-verify-flag-failed (Script failed an OP_CHECKCOLDSTAKEVERIFY operation",
                                self.spendUTXOwithNode, u, 1)
        self.log.info("Good. Cold staker was NOT able to spend (failed OP_CHECKCOLDSTAKEVERIFY)")
        self.generateBlock()
        sync_blocks(self.nodes)

        # 8) check that the staker can use the coins to stake a block with internal miner.
        # --------------------------------------------------------------------------------
        print("*** 8 ***")
        assert_equal(self.nodes[1].getstakingstatus()["mintablecoins"], True)
        self.log.info("Generating one valid cold-stake block...")
        self.generateBlock(1, 1)
        self.log.info("New block created by cold-staking. Trying to submit...")
        newblockhash = self.nodes[1].getbestblockhash()
        self.log.info("Block %s submitted" % newblockhash)

        # Verify that nodes[0] accepts it
        sync_blocks(self.nodes)
        assert_equal(self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        assert_equal(newblockhash, self.nodes[0].getbestblockhash())
        self.log.info("Great. Cold-staked block was accepted!")

        # check balances after staked block.
        self.expected_balance -= 50
        self.expected_immature_balance += 300
        self.checkBalances()
        self.log.info("Balances check out after staked block")

        # 9) check that the staker can use the coins to stake a block with a rawtransaction.
        # ----------------------------------------------------------------------------------
        print("*** 9 ***")
        self.log.info("Generating another valid cold-stake block...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInputs = self.get_prevouts(1, stakeable_coins)
        assert_greater_than(len(stakeInputs), 0)
        # Create the block
        new_block = self.stake_next_block(1, stakeInputs, None, staker_privkey)
        self.log.info("New block created (rawtx) by cold-staking. Trying to submit...")
        # Try to submit the block
        ret = self.nodes[1].submitblock(bytes_to_hex_str(new_block.serialize()))
        self.log.info("Block %s submitted." % new_block.hash)
        assert(ret is None)

        # Verify that nodes[0] accepts it
        sync_blocks(self.nodes)
        assert_equal(self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        assert_equal(new_block.hash, self.nodes[0].getbestblockhash())
        self.log.info("Great. Cold-staked block was accepted!")

        # check balances after staked block.
        self.expected_balance -= 50
        self.expected_immature_balance += 300
        self.checkBalances()
        self.log.info("Balances check out after staked block")

        # 10) check that the staker cannot stake a block changing the coinstake scriptPubkey.
        # ----------------------------------------------------------------------------------
        print("*** 10 ***")
        self.log.info("Generating one invalid cold-stake block (changing first coinstake output)...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInputs = self.get_prevouts(1, stakeable_coins)
        assert_greater_than(len(stakeInputs), 0)
        # Create the block (with dummy key)
        new_block = self.stake_next_block(1, stakeInputs, None, "")
        self.log.info("New block created (rawtx) by cold-staking. Trying to submit...")
        # Try to submit the block
        ret = self.nodes[1].submitblock(bytes_to_hex_str(new_block.serialize()))
        self.log.info("Block %s submitted." % new_block.hash)
        assert("rejected" in ret)

        # Verify that nodes[0] rejects it
        sync_blocks(self.nodes)
        assert_raises_rpc_error(-5, "Block not found", self.nodes[0].getblock, new_block.hash)
        self.log.info("Great. Malicious cold-staked block was NOT accepted!")
        self.checkBalances()
        self.log.info("Balances check out after (non) staked block")

        # 11) neither adding different outputs to the coinstake.
        # ------------------------------------------------------
        print("*** 11 ***")
        self.log.info("Generating another invalid cold-stake block (adding coinstake output)...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInputs = self.get_prevouts(1, stakeable_coins)
        assert_greater_than(len(stakeInputs), 0)
        # Create the block
        new_block = self.stake_next_block(1, stakeInputs, None, staker_privkey)
        # Add output (dummy key address) to coinstake (taking 100 PIV from the pot)
        self.add_output_to_coinstake(new_block, 100)
        self.log.info("New block created (rawtx) by cold-staking. Trying to submit...")
        # Try to submit the block
        ret = self.nodes[1].submitblock(bytes_to_hex_str(new_block.serialize()))
        self.log.info("Block %s submitted." % new_block.hash)
        assert_equal(ret, "bad-p2cs-outs")

        # Verify that nodes[0] rejects it
        sync_blocks(self.nodes)
        assert_raises_rpc_error(-5, "Block not found", self.nodes[0].getblock, new_block.hash)
        self.log.info("Great. Malicious cold-staked block was NOT accepted!")
        self.checkBalances()
        self.log.info("Balances check out after (non) staked block")

        # 12) Now node[0] gets mad and spends all the delegated coins, voiding the P2CS contracts.
        # ----------------------------------------------------------------------------------------
        self.log.info("Let's void the contracts.")
        self.generateBlock()
        sync_blocks(self.nodes)
        print("*** 12 ***")
        self.log.info("Cancel the stake delegation spending the delegated utxos...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        # remove one utxo to spend later
        final_spend = delegated_utxos.pop()
        txhash = self.spendUTXOsWithNode(delegated_utxos, 0)
        assert(txhash != None)
        self.log.info("Good. Owner was able to void the stake delegations - tx: %s" % str(txhash))
        self.generateBlock()
        sync_blocks(self.nodes)

        # deactivate SPORK 17 and check that the owner can still spend the last utxo
        self.setColdStakingEnforcement(False)
        assert (not self.isColdStakingEnforced())
        txhash = self.spendUTXOsWithNode([final_spend], 0)
        assert(txhash != None)
        self.log.info("Good. Owner was able to void a stake delegation (with SPORK 17 disabled) - tx: %s" % str(txhash))
        self.generateBlock()
        sync_blocks(self.nodes)

        # check balances after big spend.
        self.expected_balance = 0
        self.checkBalances()
        self.log.info("Balances check out after the delegations have been voided.")
        # re-activate SPORK17
        self.setColdStakingEnforcement()
        assert (self.isColdStakingEnforced())

        # 13) check that coinstaker is empty and can no longer stake.
        # -----------------------------------------------------------
        print("*** 13 ***")
        self.log.info("Trying to generate one cold-stake block again...")
        assert_equal(self.nodes[1].getstakingstatus()["mintablecoins"], False)
        self.log.info("Cigar. Cold staker was NOT able to create any more blocks.")

        # 14) check balances when mature.
        # -----------------------------------------------------------
        print("*** 14 ***")
        self.log.info("Staking 100 blocks to mature the cold stakes...")
        self.generateBlock(100)
        self.expected_balance = self.expected_immature_balance
        self.expected_immature_balance = 0
        self.checkBalances()
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        txhash = self.spendUTXOsWithNode(delegated_utxos, 0)
        assert (txhash != None)
        self.log.info("Good. Owner was able to spend the cold staked coins - tx: %s" % str(txhash))
        self.generateBlock()
        sync_blocks(self.nodes)
        self.expected_balance = 0
        self.checkBalances()

    def generateBlock(self, n=1, nodeid=2):
        fStaked = False
        while (not fStaked):
            try:
                self.nodes[nodeid].generate(n)
                fStaked = True
            except JSONRPCException as e:
                if ("Couldn't create new block" in str(e)):
                    # Sleep two seconds and retry
                    sleep(2)
                else:
                    raise e

    def checkBalances(self):
        w_info = self.nodes[0].getwalletinfo()
        self.log.info("OWNER - Delegated %f / Cold %f   [%f / %f]" % (
            float(w_info["delegated_balance"]), w_info["cold_staking_balance"],
            float(w_info["immature_delegated_balance"]), w_info["immature_cold_staking_balance"]))
        assert_equal(float(w_info["delegated_balance"]), self.expected_balance)
        assert_equal(float(w_info["immature_delegated_balance"]), self.expected_immature_balance)
        assert_equal(float(w_info["cold_staking_balance"]), 0)
        w_info = self.nodes[1].getwalletinfo()
        self.log.info("STAKER - Delegated %f / Cold %f   [%f / %f]" % (
            float(w_info["delegated_balance"]), w_info["cold_staking_balance"],
            float(w_info["immature_delegated_balance"]), w_info["immature_cold_staking_balance"]))
        assert_equal(float(w_info["delegated_balance"]), 0)
        assert_equal(float(w_info["cold_staking_balance"]), self.expected_balance)
        assert_equal(float(w_info["immature_cold_staking_balance"]), self.expected_immature_balance)

    def spendUTXOwithNode(self, utxo, node_n):
        new_addy = self.nodes[node_n].getnewaddress()
        inputs = [{"txid": utxo["txid"], "vout": utxo["vout"]}]
        out_amount = (float(utxo["amount"]) - self.DEFAULT_FEE)
        outputs = {}
        outputs[new_addy] = out_amount
        spendingTx = self.nodes[node_n].createrawtransaction(inputs, outputs)
        spendingTx_signed = self.nodes[node_n].signrawtransaction(spendingTx)
        return self.nodes[node_n].sendrawtransaction(spendingTx_signed["hex"])

    def spendUTXOsWithNode(self, utxos, node_n):
        new_addy = self.nodes[node_n].getnewaddress()
        inputs = []
        outputs = {}
        outputs[new_addy] = 0
        for utxo in utxos:
            inputs.append({"txid": utxo["txid"], "vout": utxo["vout"]})
            outputs[new_addy] += float(utxo["amount"])
        outputs[new_addy] -= self.DEFAULT_FEE
        spendingTx = self.nodes[node_n].createrawtransaction(inputs, outputs)
        spendingTx_signed = self.nodes[node_n].signrawtransaction(spendingTx)
        return self.nodes[node_n].sendrawtransaction(spendingTx_signed["hex"])

    def add_output_to_coinstake(self, block, value, peer=1):
        coinstake = block.vtx[1]
        if not hasattr(self, 'DUMMY_KEY'):
            self.init_dummy_key()
        coinstake.vout.append(
            CTxOut(value * COIN, CScript([self.DUMMY_KEY.get_pubkey(), OP_CHECKSIG])))
        coinstake.vout[1].nValue -= value * COIN
        # re-sign coinstake
        prevout = COutPoint()
        prevout.deserialize_uniqueness(BytesIO(block.prevoutStake))
        coinstake.vin[0] = CTxIn(prevout)
        stake_tx_signed_raw_hex = self.nodes[peer].signrawtransaction(
            bytes_to_hex_str(coinstake.serialize()))['hex']
        block.vtx[1] = CTransaction()
        block.vtx[1].from_hex(stake_tx_signed_raw_hex)
        # re-sign block
        block.hashMerkleRoot = block.calc_merkle_root()
        block.rehash()
        block.re_sign_block()





if __name__ == '__main__':
    PIVX_ColdStakingTest().main()
