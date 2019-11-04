#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
# -*- coding: utf-8 -*-

from io import BytesIO
from struct import pack
from random import randint, choice
import time

from test_framework.authproxy import JSONRPCException
from test_framework.blocktools import create_coinbase_pos, stake_block, stake_next_block, get_prevouts, make_txes, DUMMY_KEY
from test_framework.key import CECKey
from test_framework.messages import CTransaction, CTxOut, CTxIn, COutPoint, COIN, msg_block
from test_framework.mininode import network_thread_start
from test_framework.test_framework import PivxTestFramework
from test_framework.script import CScript, OP_CHECKSIG
from test_framework.util import hash256, bytes_to_hex_str, hex_str_to_bytes, connect_nodes_bi, p2p_port

from .util import TestNode, create_transaction, dir_size
''' -------------------------------------------------------------------------
PIVX_FakeStakeTest CLASS ----------------------------------------------------

General Test Class to be extended by individual tests for each attack test
'''
class PIVX_FakeStakeTest(PivxTestFramework):

    def set_test_params(self):
        ''' Setup test environment
        :param:
        :return:
        '''
        self.setup_clean_chain = True
        self.num_nodes = 1


    def init_test(self):
        ''' Initializes test parameters
        :param:
        :return:
        '''
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, self.description)
        # Global Test parameters (override in run_test)
        self.DEFAULT_FEE = 0.1
        # Spam blocks to send in current test
        self.NUM_BLOCKS = 30

        # Setup the p2p connections and start up the network thread.
        self.test_nodes = []
        for i in range(self.num_nodes):
            self.test_nodes.append(TestNode())
            self.test_nodes[i].peer_connect('127.0.0.1', p2p_port(i))

        network_thread_start()  # Start up network handling in another thread
        self.node = self.nodes[0]

        # Let the test nodes get in sync
        for i in range(self.num_nodes):
            self.test_nodes[i].wait_for_verack()


    def run_test(self):
        ''' Performs the attack of this test - run init_test first.
        :param:
        :return:
        '''
        self.description = ""
        self.init_test()
        return


    def spend_utxo(self, utxo, address_list):
        ''' spend amount from previously unspent output to a provided address
        :param      utxo:           (JSON) returned from listunspent used as input
                    addresslist:    (string) destination address
        :return:    txhash:         (string) tx hash if successful, empty string otherwise
        '''
        try:
            inputs = [{"txid":utxo["txid"], "vout":utxo["vout"]}]
            out_amount = (float(utxo["amount"]) - self.DEFAULT_FEE)/len(address_list)
            outputs = {}
            for address in address_list:
                outputs[address] = out_amount
            spendingTx = self.node.createrawtransaction(inputs, outputs)
            spendingTx_signed = self.node.signrawtransaction(spendingTx)
            if spendingTx_signed["complete"]:
                txhash = self.node.sendrawtransaction(spendingTx_signed["hex"])
                return txhash
            else:
                self.log.warning("Error: %s" % str(spendingTx_signed["errors"]))
                return ""
        except JSONRPCException as e:
            self.log.error("JSONRPCException: %s" % str(e))
            return ""


    def spend_utxos(self, utxo_list, address_list = []):
        ''' spend utxos to provided list of addresses or 10 new generate ones.
        :param      utxo_list:      (JSON list) returned from listunspent used as input
                    address_list:   (string list) [optional] recipient PIVX addresses. if not set,
                                    10 new addresses will be generated from the wallet for each tx.
        :return:    txHashes        (string list) tx hashes
        '''
        txHashes = []

        # If not given, get 10 new addresses from self.node wallet
        if address_list == []:
            for i in range(10):
                address_list.append(self.node.getnewaddress())

        for utxo in utxo_list:
            try:
                # spend current utxo to provided addresses
                txHash = self.spend_utxo(utxo, address_list)
                if txHash != "":
                    txHashes.append(txHash)
            except JSONRPCException as e:
                self.log.error("JSONRPCException: %s" % str(e))
                continue
        return txHashes


    def stake_amplification_step(self, utxo_list, address_list = []):
        ''' spends a list of utxos providing the list of new outputs
        :param      utxo_list:     (JSON list) returned from listunspent used as input
                    address_list:  (string list) [optional] recipient PIVX addresses.
        :return:    new_utxos:     (JSON list) list of new (valid) inputs after the spends
        '''
        self.log.info("--> Stake Amplification step started with %d UTXOs", len(utxo_list))
        txHashes = self.spend_utxos(utxo_list, address_list)
        num_of_txes = len(txHashes)
        new_utxos = []
        if num_of_txes> 0:
            self.log.info("Created %d transactions...Mining 2 blocks to include them..." % num_of_txes)
            self.node.generate(2)
            time.sleep(2)
            new_utxos = self.node.listunspent()

        self.log.info("Amplification step produced %d new \"Fake Stake\" inputs:" % len(new_utxos))
        return new_utxos



    def stake_amplification(self, utxo_list, iterations, address_list = []):
        ''' performs the "stake amplification" which gives higher chances at finding fake stakes
        :param      utxo_list:    (JSON list) returned from listunspent used as input
                    iterations:   (int) amount of stake amplification steps to perform
                    address_list: (string list) [optional] recipient PIVX addresses.
        :return:    all_inputs:   (JSON list) list of all spent inputs
        '''
        self.log.info("** Stake Amplification started with %d UTXOs", len(utxo_list))
        valid_inputs = utxo_list
        all_inputs = []
        for i in range(iterations):
            all_inputs = all_inputs + valid_inputs
            old_inputs = valid_inputs
            valid_inputs = self.stake_amplification_step(old_inputs, address_list)
        self.log.info("** Stake Amplification ended with %d \"fake\" UTXOs", len(all_inputs))
        return all_inputs



    def sign_stake_tx(self, block, stake_in_value, fZPoS=False):
        ''' signs a coinstake transaction
        :param      block:           (CBlock) block with stake to sign
                    stake_in_value:  (int) staked amount
                    fZPoS:           (bool) zerocoin stake
        :return:    stake_tx_signed: (CTransaction) signed tx
        '''
        self.block_sig_key = CECKey()

        if fZPoS:
            self.log.info("Signing zPoS stake...")
            # Create raw zerocoin stake TX (signed)
            raw_stake = self.node.createrawzerocoinstake(block.prevoutStake)
            stake_tx_signed_raw_hex = raw_stake["hex"]
            # Get stake TX private key to sign the block with
            stake_pkey = raw_stake["private-key"]
            self.block_sig_key.set_compressed(True)
            self.block_sig_key.set_secretbytes(bytes.fromhex(stake_pkey))

        else:
            # Create a new private key and get the corresponding public key
            self.block_sig_key.set_secretbytes(hash256(pack('<I', 0xffff)))
            pubkey = self.block_sig_key.get_pubkey()
            # Create the raw stake TX (unsigned)
            scriptPubKey = CScript([pubkey, OP_CHECKSIG])
            outNValue = int(stake_in_value + 2*COIN)
            stake_tx_unsigned = CTransaction()
            stake_tx_unsigned.nTime = block.nTime
            stake_tx_unsigned.vin.append(CTxIn(block.prevoutStake))
            stake_tx_unsigned.vin[0].nSequence = 0xffffffff
            stake_tx_unsigned.vout.append(CTxOut())
            stake_tx_unsigned.vout.append(CTxOut(outNValue, scriptPubKey))
            # Sign the stake TX
            stake_tx_signed_raw_hex = self.node.signrawtransaction(bytes_to_hex_str(stake_tx_unsigned.serialize()))['hex']

        # Deserialize the signed raw tx into a CTransaction object and return it
        stake_tx_signed = CTransaction()
        stake_tx_signed.deserialize(BytesIO(hex_str_to_bytes(stake_tx_signed_raw_hex)))
        return stake_tx_signed



    def log_data_dir_size(self):
        ''' Prints the size of the '/regtest/blocks' directory.
        :param:
        :return:
        '''
        init_size = dir_size(self.node.datadir + "/regtest/blocks")
        self.log.info("Size of data dir: %s kilobytes" % str(init_size))



    def test_spam(self, name, staking_utxo_list,
                  fRandomHeight=False, randomRange=0, randomRange2=0,
                  fDoubleSpend=False, fMustPass=False, fZPoS=False,
                  spending_utxo_list=[]):
        ''' General method to create, send and test the spam blocks
        :param    name:               (string) chain branch (usually either "Main" or "Forked")
                  staking_utxo_list:  (string list) utxos to use for staking
                  fRandomHeight:      (bool) send blocks at random height
                  randomRange:        (int) if fRandomHeight=True, height is >= current-randomRange
                  randomRange2:       (int) if fRandomHeight=True, height is < current-randomRange2
                  fDoubleSpend:       (bool) if true, stake input is double spent in block.vtx
                  fMustPass:          (bool) if true, the blocks must be stored on disk
                  fZPoS:              (bool) stake the block with zerocoin
                  spending_utxo_list: (string list) utxos to use for spending
        :return:  err_msgs:           (string list) reports error messages from the test
                                      or an empty list if test is successful
        '''
        # Create empty error messages list
        err_msgs = []
        # Log initial datadir size
        self.log_data_dir_size()
        # Get latest block number and hash
        block_count = self.node.getblockcount()
        pastBlockHash = self.node.getblockhash(block_count)
        randomCount = block_count
        self.log.info("Current height: %d" % block_count)
        for i in range(0, self.NUM_BLOCKS):
            if i !=0:
                self.log.info("Sent %d blocks out of %d" % (i, self.NUM_BLOCKS))

            # if fRandomHeight=True get a random block number (in range) and corresponding hash
            if fRandomHeight:
                randomCount = randint(block_count - randomRange, block_count - randomRange2)
                pastBlockHash = self.node.getblockhash(randomCount)

            # Make spam txes (if not provided, copy the staking input) to DUMMY_KEY address
            if len(spending_utxo_list) == 0:
                spending_utxo_list = staking_utxo_list
            spending_prevouts = get_prevouts(self.node, spending_utxo_list)
            block_txes = make_txes(self.node, spending_prevouts, DUMMY_KEY.get_pubkey())

            # Get staking prevouts and stake block (use DUMMY_KEY "" to stake)
            current_block_n = randomCount + 1
            stakeInputs = get_prevouts(self.node, staking_utxo_list, fZPoS, current_block_n)
            block = stake_block(current_block_n, pastBlockHash, self.node,
                                stakeInputs, None, "", block_txes, fDoubleSpend)

            # Log time and size of the block
            block_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(block.nTime))
            block_size = len(block.serialize())/1000
            self.log.info("Sending block %d [%s...] - nTime: %s - Size (kb): %.2f",
                          current_block_n, block.hash[:7], block_time, block_size)

            # Try submitblock
            var = self.node.submitblock(bytes_to_hex_str(block.serialize()))
            time.sleep(1)
            if (not fMustPass and var not in [None, "bad-txns-invalid-zpiv"]) or (fMustPass and var != "inconclusive"):
                self.log.error("submitblock [fMustPass=%s] result: %s" % (str(fMustPass), str(var)))
                err_msgs.append("submitblock %d: %s" % (current_block_n, str(var)))

            # Try sending the message block
            msg = msg_block(block)
            try:
                self.test_nodes[0].handle_connect()
                self.test_nodes[0].send_message(msg)
                time.sleep(2)
                block_ret = self.node.getblock(block.hash)
                if not fMustPass and block_ret is not None:
                    self.log.error("Error, block stored in %s chain" % name)
                    err_msgs.append("getblock %d: result not None" % current_block_n)
                if fMustPass:
                    if block_ret is None:
                        self.log.error("Error, block NOT stored in %s chain" % name)
                        err_msgs.append("getblock %d: result is None" % current_block_n)
                    else:
                        self.log.info("Good. Block IS stored on disk.")

            except JSONRPCException as e:
                exc_msg = str(e)
                if exc_msg == "Can't read block from disk (-32603)":
                    if fMustPass:
                        self.log.warning("Bad! Block was NOT stored to disk.")
                        err_msgs.append(exc_msg)
                    else:
                        self.log.info("Good. Block was not stored on disk.")
                else:
                    self.log.warning(exc_msg)
                    err_msgs.append(exc_msg)

            except Exception as e:
                exc_msg = str(e)
                self.log.error(exc_msg)
                err_msgs.append(exc_msg)

            # Remove the used coinstake input
            if fZPoS:
                staking_utxo_list = [x for x in staking_utxo_list if
                                     x['hash stake'] != block.prevoutStake[::-1].hex()]
            else:
                staking_utxo_list = [x for x in staking_utxo_list if
                                     COutPoint(
                                         int(x['txid'], 16), x['vout']
                                     ).serialize_uniqueness() != block.prevoutStake]

        self.log.info("Sent all %s blocks." % str(self.NUM_BLOCKS))
        # Log final datadir size
        self.log_data_dir_size()
        # Return errors list
        return err_msgs
