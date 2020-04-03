#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Base class for RPC testing."""

from enum import Enum
from io import BytesIO
import logging
import optparse
import os
import pdb
import shutil
from struct import pack
import sys
import tempfile
import time

from . import coverage
from .address import wif_to_privkey
from .authproxy import JSONRPCException
from .blocktools import (
    create_block,
    create_coinbase_pos,
    create_transaction_from_outpoint,
    is_zerocoin,
)
from .key import CECKey
from .messages import (
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    hash256,
)
from .script import (
    CScript,
    OP_CHECKSIG,
)
from .test_node import TestNode
from .util import (
    MAX_NODES,
    PortSeed,
    assert_equal,
    assert_greater_than,
    check_json_precision,
    connect_nodes,
    connect_nodes_clique,
    disconnect_nodes,
    DEFAULT_FEE,
    get_datadir_path,
    hex_str_to_bytes,
    bytes_to_hex_str,
    initialize_datadir,
    set_node_times,
    SPORK_ACTIVATION_TIME,
    SPORK_DEACTIVATION_TIME,
    sync_blocks,
    sync_mempools,
    vZC_DENOMS,
)

class TestStatus(Enum):
    PASSED = 1
    FAILED = 2
    SKIPPED = 3

TEST_EXIT_PASSED = 0
TEST_EXIT_FAILED = 1
TEST_EXIT_SKIPPED = 77

TMPDIR_PREFIX = "pivx_func_test_"


class PivxTestFramework():
    """Base class for a pivx test script.

    Individual pivx test scripts should subclass this class and override the set_test_params() and run_test() methods.

    Individual tests can also override the following methods to customize the test setup:

    - add_options()
    - setup_chain()
    - setup_network()
    - setup_nodes()

    The __init__() and main() methods should not be overridden.

    This class also contains various public and private helper methods."""

    def __init__(self):
        """Sets test framework defaults. Do not override this method. Instead, override the set_test_params() method"""
        self.setup_clean_chain = False
        self.nodes = []
        self.mocktime = 0
        self.supports_cli = False
        self.set_test_params()

        assert hasattr(self, "num_nodes"), "Test must set self.num_nodes in set_test_params()"

    def main(self):
        """Main function. This should not be overridden by the subclass test scripts."""

        parser = optparse.OptionParser(usage="%prog [options]")
        parser.add_option("--nocleanup", dest="nocleanup", default=False, action="store_true",
                          help="Leave pivxds and test.* datadir on exit or error")
        parser.add_option("--noshutdown", dest="noshutdown", default=False, action="store_true",
                          help="Don't stop pivxds after the test execution")
        parser.add_option("--srcdir", dest="srcdir", default=os.path.normpath(os.path.dirname(os.path.realpath(__file__))+"/../../../src"),
                          help="Source directory containing pivxd/pivx-cli (default: %default)")
        parser.add_option("--cachedir", dest="cachedir", default=os.path.normpath(os.path.dirname(os.path.realpath(__file__)) + "/../../cache"),
                          help="Directory for caching pregenerated datadirs")
        parser.add_option("--tmpdir", dest="tmpdir", help="Root directory for datadirs")
        parser.add_option("-l", "--loglevel", dest="loglevel", default="INFO",
                          help="log events at this level and higher to the console. Can be set to DEBUG, INFO, WARNING, ERROR or CRITICAL. Passing --loglevel DEBUG will output all logs to console. Note that logs at all levels are always written to the test_framework.log file in the temporary test directory.")
        parser.add_option("--tracerpc", dest="trace_rpc", default=False, action="store_true",
                          help="Print out all RPC calls as they are made")
        parser.add_option("--portseed", dest="port_seed", default=os.getpid(), type='int',
                          help="The seed to use for assigning port numbers (default: current process id)")
        parser.add_option("--coveragedir", dest="coveragedir",
                          help="Write tested RPC commands into this directory")
        parser.add_option("--configfile", dest="configfile",
                          help="Location of the test framework config file")
        parser.add_option('--legacywallet', dest="legacywallet", default=False, action="store_true",
                          help='create pre-HD wallets only')
        parser.add_option("--pdbonfailure", dest="pdbonfailure", default=False, action="store_true",
                          help="Attach a python debugger if test fails")
        parser.add_option("--usecli", dest="usecli", default=False, action="store_true",
                          help="use pivx-cli instead of RPC for all commands")
        self.add_options(parser)
        (self.options, self.args) = parser.parse_args()

        PortSeed.n = self.options.port_seed

        os.environ['PATH'] = self.options.srcdir + ":" + self.options.srcdir + "/qt:" + os.environ['PATH']

        check_json_precision()

        self.options.cachedir = os.path.abspath(self.options.cachedir)

        # Set up temp directory and start logging
        if self.options.tmpdir:
            self.options.tmpdir = os.path.abspath(self.options.tmpdir)
            os.makedirs(self.options.tmpdir, exist_ok=False)
        else:
            self.options.tmpdir = tempfile.mkdtemp(prefix=TMPDIR_PREFIX)
        self._start_logging()

        success = TestStatus.FAILED

        try:
            if self.options.usecli and not self.supports_cli:
                raise SkipTest("--usecli specified but test does not support using CLI")
            self.setup_chain()
            self.setup_network()
            time.sleep(5)
            self.run_test()
            success = TestStatus.PASSED
        except JSONRPCException as e:
            self.log.exception("JSONRPC error")
        except SkipTest as e:
            self.log.warning("Test Skipped: %s" % e.message)
            success = TestStatus.SKIPPED
        except AssertionError as e:
            self.log.exception("Assertion failed")
        except KeyError as e:
            self.log.exception("Key error")
        except Exception as e:
            self.log.exception("Unexpected exception caught during testing")
        except KeyboardInterrupt as e:
            self.log.warning("Exiting after keyboard interrupt")

        if success == TestStatus.FAILED and self.options.pdbonfailure:
            print("Testcase failed. Attaching python debugger. Enter ? for help")
            pdb.set_trace()

        if not self.options.noshutdown:
            self.log.info("Stopping nodes")
            if self.nodes:
                self.stop_nodes()
        else:
            for node in self.nodes:
                node.cleanup_on_exit = False
            self.log.info("Note: pivxds were not stopped and may still be running")

        if not self.options.nocleanup and not self.options.noshutdown and success != TestStatus.FAILED:
            self.log.info("Cleaning up")
            shutil.rmtree(self.options.tmpdir)
        else:
            self.log.warning("Not cleaning up dir %s" % self.options.tmpdir)

        if success == TestStatus.PASSED:
            self.log.info("Tests successful")
            exit_code = TEST_EXIT_PASSED
        elif success == TestStatus.SKIPPED:
            self.log.info("Test skipped")
            exit_code = TEST_EXIT_SKIPPED
        else:
            self.log.error("Test failed. Test logging available at %s/test_framework.log", self.options.tmpdir)
            self.log.error("Hint: Call {} '{}' to consolidate all logs".format(os.path.normpath(os.path.dirname(os.path.realpath(__file__)) + "/../combine_logs.py"), self.options.tmpdir))
            exit_code = TEST_EXIT_FAILED
        logging.shutdown()
        sys.exit(exit_code)

    # Methods to override in subclass test scripts.
    def set_test_params(self):
        """Tests must this method to change default values for number of nodes, topology, etc"""
        raise NotImplementedError

    def add_options(self, parser):
        """Override this method to add command-line options to the test"""
        pass

    def setup_chain(self):
        """Override this method to customize blockchain setup"""
        self.log.info("Initializing test directory " + self.options.tmpdir)
        if self.setup_clean_chain:
            self._initialize_chain_clean()
        else:
            self._initialize_chain()

    def setup_network(self):
        """Override this method to customize test network topology"""
        self.setup_nodes()

        # Connect the nodes as a "chain".  This allows us
        # to split the network between nodes 1 and 2 to get
        # two halves that can work on competing chains.
        #
        # Topology looks like this:
        # node0 <-- node1 <-- node2 <-- node3
        #
        # If all nodes are in IBD (clean chain from genesis), node0 is assumed to be the source of blocks (miner). To
        # ensure block propagation, all nodes will establish outgoing connections toward node0.
        # See fPreferredDownload in net_processing.
        #
        # If further outbound connections are needed, they can be added at the beginning of the test with e.g.
        # connect_nodes(self.nodes[1], 2)
        for i in range(self.num_nodes - 1):
            connect_nodes(self.nodes[i + 1], i)
        self.sync_all()

    def setup_nodes(self):
        """Override this method to customize test node setup"""
        extra_args = None
        if hasattr(self, "extra_args"):
            extra_args = self.extra_args
        self.add_nodes(self.num_nodes, extra_args)
        self.start_nodes()

    def run_test(self):
        """Tests must override this method to define test logic"""
        raise NotImplementedError

    # Public helper methods. These can be accessed by the subclass test scripts.

    def add_nodes(self, num_nodes, extra_args=None, rpchost=None, timewait=None, binary=None):
        """Instantiate TestNode objects"""

        if extra_args is None:
            extra_args = [[]] * num_nodes
        # Check wallet version
        if self.options.legacywallet:
            for arg in extra_args:
                arg.append('-legacywallet')
            self.log.info("Running test with legacy (pre-HD) wallet")
        if binary is None:
            binary = [None] * num_nodes
        assert_equal(len(extra_args), num_nodes)
        assert_equal(len(binary), num_nodes)
        for i in range(num_nodes):
            self.nodes.append(TestNode(i, self.options.tmpdir, extra_args[i], rpchost, timewait=timewait, binary=binary[i], stderr=None, mocktime=self.mocktime, coverage_dir=self.options.coveragedir, use_cli=self.options.usecli))

    def start_node(self, i, *args, **kwargs):
        """Start a pivxd"""

        node = self.nodes[i]

        node.start(*args, **kwargs)
        node.wait_for_rpc_connection()

        time.sleep(10)

        if self.options.coveragedir is not None:
            coverage.write_all_rpc_commands(self.options.coveragedir, node.rpc)

    def start_nodes(self, extra_args=None, *args, **kwargs):
        """Start multiple pivxds"""

        if extra_args is None:
            extra_args = [None] * self.num_nodes
        assert_equal(len(extra_args), self.num_nodes)
        try:
            for i, node in enumerate(self.nodes):
                node.start(extra_args[i], *args, **kwargs)
            for node in self.nodes:
                node.wait_for_rpc_connection()
        except:
            # If one node failed to start, stop the others
            self.stop_nodes()
            raise

        time.sleep(10)

        if self.options.coveragedir is not None:
            for node in self.nodes:
                coverage.write_all_rpc_commands(self.options.coveragedir, node.rpc)

    def stop_node(self, i):
        """Stop a pivxd test node"""
        self.nodes[i].stop_node()
        self.nodes[i].wait_until_stopped()

    def stop_nodes(self):
        """Stop multiple pivxd test nodes"""
        for node in self.nodes:
            # Issue RPC to stop nodes
            node.stop_node()

        for node in self.nodes:
            # Wait for nodes to stop
            time.sleep(5)
            node.wait_until_stopped()

    def restart_node(self, i, extra_args=None):
        """Stop and start a test node"""
        self.stop_node(i)
        self.start_node(i, extra_args)

    def assert_start_raises_init_error(self, i, extra_args=None, expected_msg=None, *args, **kwargs):
        with tempfile.SpooledTemporaryFile(max_size=2**16) as log_stderr:
            try:
                self.start_node(i, extra_args, stderr=log_stderr, *args, **kwargs)
                self.stop_node(i)
            except Exception as e:
                assert 'pivxd exited' in str(e)  # node must have shutdown
                self.nodes[i].running = False
                self.nodes[i].process = None
                if expected_msg is not None:
                    log_stderr.seek(0)
                    stderr = log_stderr.read().decode('utf-8')
                    if expected_msg not in stderr:
                        raise AssertionError("Expected error \"" + expected_msg + "\" not found in:\n" + stderr)
            else:
                if expected_msg is None:
                    assert_msg = "pivxd should have exited with an error"
                else:
                    assert_msg = "pivxd should have exited with expected error " + expected_msg
                raise AssertionError(assert_msg)

    def wait_for_node_exit(self, i, timeout):
        self.nodes[i].process.wait(timeout)

    def split_network(self):
        """
        Split the network of four nodes into nodes 0/1 and 2/3.
        """
        disconnect_nodes(self.nodes[1], 2)
        disconnect_nodes(self.nodes[2], 1)
        self.sync_all([self.nodes[:2], self.nodes[2:]])

    def join_network(self):
        """
        Join the (previously split) network halves together.
        """
        connect_nodes(self.nodes[1], 2)
        self.sync_all()

    def sync_all(self, node_groups=None):
        if not node_groups:
            node_groups = [self.nodes]

        for group in node_groups:
            sync_blocks(group)
            sync_mempools(group)

    def enable_mocktime(self):
        """Enable mocktime for the script.

        mocktime may be needed for scripts that use the cached version of the
        blockchain.  If the cached version of the blockchain is used without
        mocktime then the mempools will not sync due to IBD.

        Sets mocktime to Tuesday, October 31, 2017 6:21:20 PM GMT (1572546080)
        """
        self.mocktime = 1572546080

    def disable_mocktime(self):
        self.mocktime = 0

    # Private helper methods. These should not be accessed by the subclass test scripts.

    def _start_logging(self):
        # Add logger and logging handlers
        self.log = logging.getLogger('TestFramework')
        self.log.setLevel(logging.DEBUG)
        # Create file handler to log all messages
        fh = logging.FileHandler(self.options.tmpdir + '/test_framework.log')
        fh.setLevel(logging.DEBUG)
        # Create console handler to log messages to stderr. By default this logs only error messages, but can be configured with --loglevel.
        ch = logging.StreamHandler(sys.stdout)
        # User can provide log level as a number or string (eg DEBUG). loglevel was caught as a string, so try to convert it to an int
        ll = int(self.options.loglevel) if self.options.loglevel.isdigit() else self.options.loglevel.upper()
        ch.setLevel(ll)
        # Format logs the same as pivxd's debug.log with microprecision (so log files can be concatenated and sorted)
        formatter = logging.Formatter(fmt='%(asctime)s.%(msecs)03d000 %(name)s (%(levelname)s): %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
        formatter.converter = time.gmtime
        fh.setFormatter(formatter)
        ch.setFormatter(formatter)
        # add the handlers to the logger
        self.log.addHandler(fh)
        self.log.addHandler(ch)

        if self.options.trace_rpc:
            rpc_logger = logging.getLogger("BitcoinRPC")
            rpc_logger.setLevel(logging.DEBUG)
            rpc_handler = logging.StreamHandler(sys.stdout)
            rpc_handler.setLevel(logging.DEBUG)
            rpc_logger.addHandler(rpc_handler)

    def _initialize_chain(self, toPosPhase=False):
        """Initialize a pre-mined blockchain for use by the test."""

        def create_cachedir(cachedir):
            if os.path.isdir(cachedir):
                shutil.rmtree(cachedir)
            os.makedirs(cachedir)

        def copy_cachedir(origin, destination, num_nodes=MAX_NODES):
            for i in range(num_nodes):
                from_dir = get_datadir_path(origin, i)
                to_dir = get_datadir_path(destination, i)
                shutil.copytree(from_dir, to_dir)
                initialize_datadir(destination, i)  # Overwrite port/rpcport in pivx.conf

        def clone_cache_from_node_1(cachedir, from_num=4):
            """ Clones cache subdir from node 1 to nodes from 'from_num' to MAX_NODES"""
            def copy_and_overwrite(from_path, to_path):
                if os.path.exists(to_path):
                    shutil.rmtree(to_path)
                shutil.copytree(from_path, to_path)
            assert from_num < MAX_NODES
            node_0_datadir = os.path.join(get_datadir_path(cachedir, 0), "regtest")
            for i in range(from_num, MAX_NODES):
                node_i_datadir = os.path.join(get_datadir_path(cachedir, i), "regtest")
                for subdir in ["blocks", "chainstate", "sporks", "zerocoin"]:
                    copy_and_overwrite(os.path.join(node_0_datadir, subdir),
                                    os.path.join(node_i_datadir, subdir))
                initialize_datadir(cachedir, i)  # Overwrite port/rpcport in pivx.conf

        def cachedir_valid(cachedir):
            for i in range(MAX_NODES):
                if not os.path.isdir(get_datadir_path(cachedir, i)):
                    return False
            # nodes directories exist. check if the first one has the .incomplete flagfile
            return (not os.path.exists(os.path.join(get_datadir_path(cachedir, 0), ".incomplete")))

        def clean_cache_subdir(cachedir):
            os.remove(os.path.join(get_datadir_path(cachedir, 0), ".incomplete"))

            def cache_path(n, *paths):
                return os.path.join(get_datadir_path(cachedir, n), "regtest", *paths)

            for i in range(MAX_NODES):
                for entry in os.listdir(cache_path(i)):
                    if entry not in ['wallet.dat', 'chainstate', 'blocks', 'sporks', 'zerocoin', 'backups']:
                        os.remove(cache_path(i, entry))

        def clean_cache_dir():
            if os.path.isdir(self.options.cachedir):
                # migrate old cache dir
                if cachedir_valid(self.options.cachedir):
                    powcachedir = os.path.join(self.options.cachedir, "pow")
                    self.log.info("Found old cachedir. Migrating to %s" % str(powcachedir))
                    copy_cachedir(self.options.cachedir, powcachedir)
                # remove everything except pow and pos subdirs
                for entry in os.listdir(self.options.cachedir):
                    if entry not in ['pow', 'pos']:
                        entry_path = os.path.join(self.options.cachedir, entry)
                        if os.path.isfile(entry_path):
                            os.remove(entry_path)
                        elif os.path.isdir(entry_path):
                            shutil.rmtree(entry_path)
            # no cachedir found
            else:
                os.makedirs(self.options.cachedir)

        def start_nodes_from_dir(ddir, num_nodes=MAX_NODES):
            self.log.info("Starting %d nodes..." % num_nodes)
            for i in range(num_nodes):
                datadir = initialize_datadir(ddir, i)
                if i == 0:
                    # Add .incomplete flagfile
                    # (removed at the end during clean_cache_subdir)
                    open(os.path.join(datadir, ".incomplete"), 'a').close()
                args = [os.getenv("BITCOIND", "pivxd"), "-spendzeroconfchange=1", "-server", "-keypool=1",
                        "-datadir=" + datadir, "-discover=0"]
                self.nodes.append(
                    TestNode(i, ddir, extra_args=[], rpchost=None, timewait=None, binary=None, stderr=None,
                             mocktime=self.mocktime, coverage_dir=None))
                self.nodes[i].args = args
                self.start_node(i)
                self.log.info("Node %d started." % i)
            # Wait for RPC connections to be ready
            self.log.info("Nodes started. Waiting for RPC connections...")
            for node in range(4):
                self.nodes[node].wait_for_rpc_connection()
            self.log.info("Connecting nodes")
            connect_nodes_clique(self.nodes)

        def stop_and_clean_cache_dir(ddir):
            self.stop_nodes()
            self.nodes = []
            # Copy cache for nodes 5 to MAX_NODES
            self.log.info("Copying cache dir to non-started nodes")
            clone_cache_from_node_1(ddir)
            self.log.info("Cleaning up.")
            clean_cache_subdir(ddir)

        def generate_pow_cache():
            ### POW Cache ###
            # Create a 200-block-long chain; each of the 4 first nodes
            # gets 25 mature blocks and 25 immature.
            # Note: To preserve compatibility with older versions of
            # initialize_chain, only 4 nodes will generate coins.
            #
            # blocks are created with timestamps 1 minutes apart
            # starting from 331 minutes in the past

            # Create cache directories, run pivxds:
            create_cachedir(powcachedir)
            self.log.info("Creating 'PoW-chain': 200 blocks")
            start_nodes_from_dir(powcachedir, 4)

            # Mine the blocks
            self.log.info("Mining 200 blocks")
            self.enable_mocktime()
            block_time = self.mocktime - (331 * 60)
            for i in range(2):
                for peer in range(4):
                    for j in range(25):
                        set_node_times(self.nodes, block_time)
                        self.nodes[peer].generate(1)
                        block_time += 60
                    # Must sync before next peer starts generating blocks
                    sync_blocks(self.nodes)

            # Shut them down, and clean up cache directories:
            self.log.info("Stopping nodes")
            stop_and_clean_cache_dir(powcachedir)
            self.log.info("---> pow cache created")
            self.disable_mocktime()


        assert self.num_nodes <= MAX_NODES

        clean_cache_dir()
        powcachedir = os.path.join(self.options.cachedir, "pow")
        is_powcache_valid = cachedir_valid(powcachedir)
        poscachedir = os.path.join(self.options.cachedir, "pos")
        is_poscache_valid = cachedir_valid(poscachedir)

        if not toPosPhase and not is_powcache_valid:
            self.log.info("PoW-CACHE NOT FOUND or INVALID.")
            self.log.info("Creating new cached blockchain data.")
            generate_pow_cache()

        elif toPosPhase and not is_poscache_valid:
            self.log.info("PoS-CACHE NOT FOUND or INVALID.")
            self.log.info("Creating new cached blockchain data.")

            # check if first 200 blocks (pow cache) is present. if not generate it.
            if not is_powcache_valid:
                self.log.info("PoW-CACHE NOT FOUND or INVALID. Generating it first.")
                generate_pow_cache()

            self.enable_mocktime()
            block_time = self.mocktime - (131 * 60)

            ### POS Cache ###
            # Create a 330-block-long chain
            # First 200 PoW blocks are copied from PoW chain.
            # The next 48 PoW blocks are mined in 12-blocks bursts by the first 4 nodes.
            # The last 2 PoW blocks are then mined by the last node (Node 3).
            # Then 80 PoS blocks are generated in 20-blocks bursts by the first 4 nodes.
            #
            # - Node 0 and node 1 get 62 mature blocks (pow) + 20 immmature (pos)
            #   42 rewards spendable (62 mature blocks - 20 spent rewards)
            # - Node 2 gets 56 mature blocks (pow) + 26 immmature (6 pow + 20 pos)
            #   35 rewards spendable (55 mature blocks - 20 spent rewards)
            # - Node 3 gets 50 mature blocks (pow) + 34 immmature (14 pow + 20 pos)
            #   30 rewards spendable (50 mature blocks - 20 spent rewards)
            # - Nodes 2 and 3 mint one zerocoin for each denom (tot 6666 PIV) on block 301/302
            #   8 mature zc + 8/3 rewards spendable (35/30 - 27 spent) + change 83.92
            #
            # Block 331-336 will mature last 6 pow blocks mined by node 2.
            # Then 337-350 will mature last 14 pow blocks mined by node 3.
            # Then staked blocks start maturing at height 351.

            # Create cache directories, run pivxds:
            create_cachedir(poscachedir)
            self.log.info("Creating 'PoS-chain': 330 blocks")
            self.log.info("Copying 200 initial blocks from pow cache")
            copy_cachedir(powcachedir, poscachedir)
            # Change datadir and restart the nodes (only 4 of them)
            start_nodes_from_dir(poscachedir, 4)

            # Mine 50 more blocks to reach PoS start.
            self.log.info("Mining 50 more blocks to reach PoS phase")
            for peer in range(4):
                for j in range(12):
                    set_node_times(self.nodes, block_time)
                    self.nodes[peer].generate(1)
                    block_time += 60
                # Must sync before next peer starts generating blocks
                if peer < 3:
                    sync_blocks(self.nodes)
            set_node_times(self.nodes, block_time)
            self.nodes[3].generate(2)
            block_time += 60
            sync_blocks(self.nodes)

            # Then stake 80 blocks.
            self.log.info("Staking 80 blocks...")
            nBlocks = 250
            res = []    # used to save the two txids for change outputs of mints (locked)
            for peer in range(4):
                for j in range(20):
                    # Stake block
                    block_time = self.generate_pos(peer, block_time)
                    nBlocks += 1
                    # Mint zerocoins with node-2 at block 301 and with node-3 at block 302
                    if nBlocks == 301 or nBlocks == 302:
                        # mints 7 zerocoins, one for each denom (tot 6666 PIV), fee = 0.01 * 8
                        # consumes 27 utxos (tot 6750 PIV), change = 6750 - 6666 - fee
                        res.append(self.nodes[nBlocks-299].mintzerocoin(6666))
                        self.sync_all()
                        # lock the change output (so it's not used as stake input in generate_pos)
                        assert (self.nodes[nBlocks-299].lockunspent(False, [{"txid": res[-1]['txid'], "vout": 8}]))
                # Must sync before next peer starts generating blocks
                sync_blocks(self.nodes)
                time.sleep(1)

            self.log.info("80 blocks staked")

            # Unlock previously locked change outputs
            for i in [2, 3]:
                assert (self.nodes[i].lockunspent(True, [{"txid": res[i-2]['txid'], "vout": 8}]))

            # Verify height and balances
            self.test_PoS_chain_balances()

            # Shut nodes down, and clean up cache directories:
            self.log.info("Stopping nodes")
            stop_and_clean_cache_dir(poscachedir)
            self.log.info("--> pos cache created")
            self.disable_mocktime()

        else:
            self.log.info("CACHE FOUND.")

        # Copy requested cache to tempdir
        if toPosPhase:
            self.log.info("Copying datadir from %s to %s" % (poscachedir, self.options.tmpdir))
            copy_cachedir(poscachedir, self.options.tmpdir, self.num_nodes)
        else:
            self.log.info("Copying datadir from %s to %s" % (powcachedir, self.options.tmpdir))
            copy_cachedir(powcachedir, self.options.tmpdir, self.num_nodes)



    def _initialize_chain_clean(self):
        """Initialize empty blockchain for use by the test.

        Create an empty blockchain and num_nodes wallets.
        Useful if a test case wants complete control over initialization."""
        for i in range(self.num_nodes):
            initialize_datadir(self.options.tmpdir, i)


    ### PIVX Specific TestFramework ###
    ###################################
    def init_dummy_key(self):
        self.DUMMY_KEY = CECKey()
        self.DUMMY_KEY.set_secretbytes(hash256(pack('<I', 0xffff)))

    def test_PoS_chain_balances(self):
        from .util import DecimalAmt
        # 330 blocks
        # - Nodes 0 and 1 get 82 blocks:
        # 62 pow + 20 pos (20 immature)
        # - Nodes 2 gets 82 blocks:
        # 62 pow + 20 pos (26 immature)
        # - Nodes 3 gets 84 blocks:
        # 64 pow + 20 pos (34 immature)
        # - Nodes 2 and 3 have 6666 PIV worth of zerocoins
        zc_tot = sum(vZC_DENOMS)
        zc_fee = len(vZC_DENOMS) * 0.01
        used_utxos = (zc_tot // 250) + 1
        zc_change = 250 * used_utxos - zc_tot - zc_fee

        # check at least 1 node and at most 5
        num_nodes = min(5, len(self.nodes))
        assert_greater_than(num_nodes, 0)

        # each node has the same height and tip
        best_block = self.nodes[0].getbestblockhash()
        for i in range(num_nodes):
            assert_equal(self.nodes[i].getblockcount(), 330)
            if i > 0:
                assert_equal(self.nodes[i].getbestblockhash(), best_block)

        # balance is mature pow blocks rewards minus stake inputs (spent)
        w_info = [self.nodes[i].getwalletinfo() for i in range(num_nodes)]
        assert_equal(w_info[0]["balance"], DecimalAmt(250.0 * (62 - 20)))
        assert_equal(w_info[1]["balance"], DecimalAmt(250.0 * (62 - 20)))
        assert_equal(w_info[2]["balance"], DecimalAmt(250.0 * (56 - 20) - (used_utxos * 250) + zc_change))
        assert_equal(w_info[3]["balance"], DecimalAmt(250.0 * (50 - 20) - (used_utxos * 250) + zc_change))
        for i in range(4, num_nodes):
            # only first 4 nodes have mined/staked
            assert_equal(w_info[i]["balance"], DecimalAmt(0))

        # immature balance is immature pow blocks rewards plus
        # immature stakes (outputs=inputs+rewards)
        assert_equal(w_info[0]["immature_balance"], DecimalAmt(500.0 * 20))
        assert_equal(w_info[1]["immature_balance"], DecimalAmt(500.0 * 20))
        assert_equal(w_info[2]["immature_balance"], DecimalAmt((250.0 * 6) + (500.0 * 20)))
        assert_equal(w_info[3]["immature_balance"], DecimalAmt((250.0 * 14) + (500.0 * 20)))
        for i in range(4, num_nodes):
            # only first 4 nodes have mined/staked
            assert_equal(w_info[i]["immature_balance"], DecimalAmt(0))

        # check zerocoin balances / mints
        for peer in [2, 3]:
            if num_nodes > peer:
                zcBalance = self.nodes[peer].getzerocoinbalance()
                zclist = self.nodes[peer].listmintedzerocoins(True)
                zclist_spendable = self.nodes[peer].listmintedzerocoins(True, True)
                assert_equal(len(zclist), len(vZC_DENOMS))
                assert_equal(zcBalance['Total'], 6666)
                assert_equal(zcBalance['Immature'], 0)
                if peer == 2:
                    assert_equal(len(zclist), len(zclist_spendable))
                assert_equal(set([x['denomination'] for x in zclist]), set(vZC_DENOMS))
                assert_equal([x['confirmations'] for x in zclist], [30-peer] * len(vZC_DENOMS))

        self.log.info("Balances of first %d nodes check out" % num_nodes)


    def get_prevouts(self, node_id, utxo_list, zpos=False, nHeight=-1):
        """ get prevouts (map) for each utxo in a list
        :param   node_id:                   (int) index of the CTestNode used as rpc connection. Must own the utxos.
                 utxo_list: <if zpos=False> (JSON list) utxos returned from listunspent used as input
                            <if zpos=True>  (JSON list) mints returned from listmintedzerocoins used as input
                 zpos:                      (bool) type of utxo_list
                 nHeight:                   (int) height of the previous block. used only if zpos=True for
                                            stake checksum. Optional, if not provided rpc_conn's height is used.
        :return: prevouts:         ({bytes --> (int, bytes, int)} dictionary)
                                   maps CStake "uniqueness" (i.e. serialized COutPoint -or hash stake, for zpiv-)
                                   to (amount, prevScript, timeBlockFrom).
                                   For zpiv prevScript is replaced with serialHash hex string.
        """
        assert_greater_than(len(self.nodes), node_id)
        rpc_conn = self.nodes[node_id]
        prevouts = {}

        for utxo in utxo_list:
            if not zpos:
                outPoint = COutPoint(int(utxo['txid'], 16), utxo['vout'])
                outValue = int(utxo['amount']) * COIN
                prevtx_json = rpc_conn.getrawtransaction(utxo['txid'], 1)
                prevTx = CTransaction()
                prevTx.deserialize(BytesIO(hex_str_to_bytes(prevtx_json['hex'])))
                if (prevTx.is_coinbase() or prevTx.is_coinstake()) and utxo['confirmations'] < 100:
                    # skip immature coins
                    continue
                prevScript = prevtx_json['vout'][utxo['vout']]['scriptPubKey']['hex']
                prevTime = prevtx_json['blocktime']
                prevouts[outPoint.serialize_uniqueness()] = (outValue, prevScript, prevTime)

            else:
                uniqueness = bytes.fromhex(utxo['hash stake'])[::-1]
                prevouts[uniqueness] = (int(utxo["denomination"]) * COIN, utxo["serial hash"], 0)

        return prevouts


    def make_txes(self, node_id, spendingPrevOuts, to_pubKey):
        """ makes a list of CTransactions each spending an input from spending PrevOuts to an output to_pubKey
        :param   node_id:            (int) index of the CTestNode used as rpc connection. Must own spendingPrevOuts.
                 spendingPrevouts:   ({bytes --> (int, bytes, int)} dictionary)
                                     maps CStake "uniqueness" (i.e. serialized COutPoint -or hash stake, for zpiv-)
                                     to (amount, prevScript, timeBlockFrom).
                                     For zpiv prevScript is replaced with serialHash hex string.
                 to_pubKey           (bytes) recipient public key
        :return: block_txes:         ([CTransaction] list)
        """
        assert_greater_than(len(self.nodes), node_id)
        rpc_conn = self.nodes[node_id]
        block_txes = []
        for uniqueness in spendingPrevOuts:
            if is_zerocoin(uniqueness):
                # spend zPIV
                _, serialHash, _ = spendingPrevOuts[uniqueness]
                raw_spend = rpc_conn.createrawzerocoinspend(serialHash, "", False)
            else:
                # spend PIV
                value_out = int(spendingPrevOuts[uniqueness][0] - DEFAULT_FEE * COIN)
                scriptPubKey = CScript([to_pubKey, OP_CHECKSIG])
                prevout = COutPoint()
                prevout.deserialize_uniqueness(BytesIO(uniqueness))
                tx = create_transaction_from_outpoint(prevout, b"", value_out, scriptPubKey)
                # sign tx
                raw_spend = rpc_conn.signrawtransaction(bytes_to_hex_str(tx.serialize()))['hex']
            # add signed tx to the list
            signed_tx = CTransaction()
            signed_tx.from_hex(raw_spend)
            block_txes.append(signed_tx)

        return block_txes

    def stake_block(self, node_id,
            nHeight,
            prevHhash,
            prevModifier,
            stakeableUtxos,
            startTime=None,
            privKeyWIF=None,
            vtx=[],
            fDoubleSpend=False):
        """ manually stakes a block selecting the coinstake input from a list of candidates
        :param   node_id:           (int) index of the CTestNode used as rpc connection. Must own stakeableUtxos.
                 nHeight:           (int) height of the block being produced
                 prevHash:          (string) hex string of the previous block hash
                 prevModifier       (string) hex string of the previous block stake modifier
                 stakeableUtxos:    ({bytes --> (int, bytes, int)} dictionary)
                                    maps CStake "uniqueness" (i.e. serialized COutPoint -or hash stake, for zpiv-)
                                    to (amount, prevScript, timeBlockFrom).
                                    For zpiv prevScript is replaced with serialHash hex string.
                 startTime:         (int) epoch time to be used as blocktime (iterated in solve_stake)
                 privKeyWIF:        (string) private key to be used for staking/signing
                                    If empty string, it will be used the pk from the stake input
                                    (dumping the sk from rpc_conn). If None, then the DUMMY_KEY will be used.
                 vtx:               ([CTransaction] list) transactions to add to block.vtx
                 fDoubleSpend:      (bool) wether any tx in vtx is allowed to spend the coinstake input
        :return: block:             (CBlock) block produced, must be manually relayed
        """
        assert_greater_than(len(self.nodes), node_id)
        rpc_conn = self.nodes[node_id]
        if not len(stakeableUtxos) > 0:
            raise Exception("Need at least one stakeable utxo to stake a block!")
        # Get start time to stake
        if startTime is None:
            startTime = time.time()
        # Create empty block with coinbase
        nTime = int(startTime) & 0xfffffff0
        coinbaseTx = create_coinbase_pos(nHeight)
        block = create_block(int(prevHhash, 16), coinbaseTx, nTime)

        # Find valid kernel hash - iterates stakeableUtxos, then block.nTime
        block.solve_stake(stakeableUtxos, int(prevModifier, 16))

        # Check if this is a zPoS block or regular/cold stake - sign stake tx
        block_sig_key = CECKey()
        isZPoS = is_zerocoin(block.prevoutStake)
        if isZPoS:
            # !TODO: remove me
            raise Exception("zPOS tests discontinued")

        else:
            coinstakeTx_unsigned = CTransaction()
            prevout = COutPoint()
            prevout.deserialize_uniqueness(BytesIO(block.prevoutStake))
            coinstakeTx_unsigned.vin.append(CTxIn(prevout, b"", 0xffffffff))
            coinstakeTx_unsigned.vout.append(CTxOut())
            amount, prevScript, _ = stakeableUtxos[block.prevoutStake]
            outNValue = int(amount + 250 * COIN)
            coinstakeTx_unsigned.vout.append(CTxOut(outNValue, hex_str_to_bytes(prevScript)))
            if privKeyWIF == "":
                # Use dummy key
                if not hasattr(self, 'DUMMY_KEY'):
                    self.init_dummy_key()
                block_sig_key = self.DUMMY_KEY
                # replace coinstake output script
                coinstakeTx_unsigned.vout[1].scriptPubKey = CScript([block_sig_key.get_pubkey(), OP_CHECKSIG])
            else:
                if privKeyWIF == None:
                    # Use pk of the input. Ask sk from rpc_conn
                    rawtx = rpc_conn.getrawtransaction('{:064x}'.format(prevout.hash), True)
                    privKeyWIF = rpc_conn.dumpprivkey(rawtx["vout"][prevout.n]["scriptPubKey"]["addresses"][0])
                # Use the provided privKeyWIF (cold staking).
                # export the corresponding private key to sign block
                privKey, compressed = wif_to_privkey(privKeyWIF)
                block_sig_key.set_compressed(compressed)
                block_sig_key.set_secretbytes(bytes.fromhex(privKey))

            # Sign coinstake TX and add it to the block
            stake_tx_signed_raw_hex = rpc_conn.signrawtransaction(
                bytes_to_hex_str(coinstakeTx_unsigned.serialize()))['hex']

        # Add coinstake to the block
        coinstakeTx = CTransaction()
        coinstakeTx.from_hex(stake_tx_signed_raw_hex)
        block.vtx.append(coinstakeTx)

        # Add provided transactions to the block.
        # Don't add tx doublespending the coinstake input, unless fDoubleSpend=True
        for tx in vtx:
            if not fDoubleSpend:
                # assume txes don't double spend zPIV inputs when fDoubleSpend is false. It needs to
                # be checked outside until a convenient tx.spends(zerocoin) is added to the framework.
                if not isZPoS and tx.spends(prevout):
                    continue
            block.vtx.append(tx)

        # Get correct MerkleRoot and rehash block
        block.hashMerkleRoot = block.calc_merkle_root()
        block.rehash()

        # sign block with block signing key and return it
        block.sign_block(block_sig_key)
        return block


    def stake_next_block(self, node_id,
            stakeableUtxos,
            btime=None,
            privKeyWIF=None,
            vtx=[],
            fDoubleSpend=False):
        """ Calls stake_block appending to the current tip"""
        assert_greater_than(len(self.nodes), node_id)
        nHeight = self.nodes[node_id].getblockcount()
        prevHhash = self.nodes[node_id].getblockhash(nHeight)
        prevModifier = self.nodes[node_id].getblock(prevHhash)['stakeModifier']
        return self.stake_block(node_id,
                                nHeight+1,
                                prevHhash,
                                prevModifier,
                                stakeableUtxos,
                                btime,
                                privKeyWIF,
                                vtx,
                                fDoubleSpend)


    def check_tx_in_chain(self, node_id, txid):
        assert_greater_than(len(self.nodes), node_id)
        rawTx = self.nodes[node_id].getrawtransaction(txid, 1)
        assert_greater_than(rawTx["confirmations"], 0)


    def spend_inputs(self, node_id, inputs, outputs):
        """ auxiliary function used by spend_utxo / spend_utxos """
        assert_greater_than(len(self.nodes), node_id)
        rpc_conn = self.nodes[node_id]
        spendingTx = rpc_conn.createrawtransaction(inputs, outputs)
        spendingTx_signed = rpc_conn.signrawtransaction(spendingTx)
        if spendingTx_signed["complete"]:
            txhash = rpc_conn.sendrawtransaction(spendingTx_signed["hex"])
            return txhash
        else:
            return ""


    def spend_utxo(self, node_id, utxo, recipient=''):
        """ spend amount from previously unspent output to a provided address
        :param    node_id:    (int) index of the CTestNode used as rpc connection. Must own the utxo.
                  utxo:       (JSON) returned from listunspent used as input
                  recipient:  (string) destination address (new one if not provided)
        :return:  txhash:     (string) tx hash if successful, empty string otherwise
        """
        assert_greater_than(len(self.nodes), node_id)
        rpc_conn = self.nodes[node_id]
        inputs = [{"txid": utxo["txid"], "vout": utxo["vout"]}]
        out_amount = float(utxo["amount"]) - DEFAULT_FEE
        outputs = {}
        if recipient == '':
            recipient = rpc_conn.getnewaddress()
        outputs[recipient] = out_amount
        return self.spend_inputs(node_id, inputs, outputs)


    def spend_utxos(self, node_id, utxo_list, recipient='', fMultiple=False):
        """ spend utxos to provided list of addresses or 10 new generate ones.
        :param    node_id:     (int) index of the CTestNode used as rpc connection. Must own the utxo.
                  utxo_list:   (JSON list) returned from listunspent used as input
                  recipient:   (string, optional) destination address (new one if not provided)
                  fMultiple:   (boolean, optional, default=false) spend each utxo on a different tx
        :return:  txHashes:    (string list) list of hashes of completed txs
        """
        assert_greater_than(len(self.nodes), node_id)
        rpc_conn = self.nodes[node_id]
        txHashes = []

        # If no recipient is given, create a new one
        if recipient == '':
            recipient = rpc_conn.getnewaddress()

        # If fMultiple=True send one tx for each utxo
        if fMultiple:
            for utxo in utxo_list:
                txHash = self.spend_utxo(node_id, utxo, recipient)
                if txHash != "":
                    txHashes.append(txHash)

        # Otherwise make a single tx with all the inputs
        else:
            inputs = [{"txid": x["txid"], "vout": x["vout"]} for x in utxo_list]
            out_amount = sum([float(x["amount"]) for x in utxo_list]) - DEFAULT_FEE
            outputs = {}
            if recipient == '':
                recipient = rpc_conn.getnewaddress()
            outputs[recipient] = out_amount
            txHash = self.spend_inputs(node_id, inputs, outputs)
            if txHash != "":
                txHashes.append(txHash)

        return txHashes


    def generate_pos(self, node_id, btime=None):
        """ stakes a block using generate on nodes[node_id]"""
        assert_greater_than(len(self.nodes), node_id)
        rpc_conn = self.nodes[node_id]
        ss = rpc_conn.getstakingstatus()
        assert ss["walletunlocked"]
        assert ss["stakeablecoins"] > 0
        assert ss["stakingbalance"] > 0.0
        if btime is not None:
            next_btime = btime + 60
        fStaked = False
        failures = 0
        while not fStaked:
            try:
                rpc_conn.generate(1)
                fStaked = True
            except JSONRPCException as e:
                if ("Couldn't create new block" in str(e)):
                    failures += 1
                    # couldn't generate block. check that this node can still stake (after 60 failures)
                    if failures > 60:
                        ss = rpc_conn.getstakingstatus()
                        if not (ss["walletunlocked"] and ss["stakeablecoins"] > 0 and ss["stakingbalance"] > 0.0):
                            raise AssertionError("Node %d unable to stake!" % node_id)
                    # try to stake one sec in the future
                    if btime is not None:
                        btime += 1
                        set_node_times(self.nodes, btime)
                    else:
                        time.sleep(1)
                else:
                    raise e
        # block generated. adjust block time
        if btime is not None:
            btime = max(btime + 1, next_btime)
            set_node_times(self.nodes, btime)
            return btime
        else:
            return None


    def generate_pow(self, node_id, btime=None):
        """ stakes a block using generate on nodes[node_id]"""
        assert_greater_than(len(self.nodes), node_id)
        self.nodes[node_id].generate(1)
        if btime is not None:
            btime += 60
            set_node_times(self.nodes, btime)
        return btime


    def set_spork(self, node_id, sporkName, value):
        assert_greater_than(len(self.nodes), node_id)
        return self.nodes[node_id].spork(sporkName, value)


    def get_spork(self, node_id, sporkName):
        assert_greater_than(len(self.nodes), node_id)
        return self.nodes[node_id].spork("show")[sporkName]


    def activate_spork(self, node_id, sporkName):
        return self.set_spork(node_id, sporkName, SPORK_ACTIVATION_TIME)


    def deactivate_spork(self, node_id, sporkName):
        return self.set_spork(node_id, sporkName, SPORK_DEACTIVATION_TIME)


    def is_spork_active(self, node_id, sporkName):
        assert_greater_than(len(self.nodes), node_id)
        return self.nodes[node_id].spork("active")[sporkName]



### ------------------------------------------------------

class ComparisonTestFramework(PivxTestFramework):
    """Test framework for doing p2p comparison testing

    Sets up some pivxd binaries:
    - 1 binary: test binary
    - 2 binaries: 1 test binary, 1 ref binary
    - n>2 binaries: 1 test binary, n-1 ref binaries"""

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def add_options(self, parser):
        parser.add_option("--testbinary", dest="testbinary",
                          default=os.getenv("BITCOIND", "pivxd"),
                          help="pivxd binary to test")
        parser.add_option("--refbinary", dest="refbinary",
                          default=os.getenv("BITCOIND", "pivxd"),
                          help="pivxd binary to use for reference nodes (if any)")

    def setup_network(self):
        extra_args = [['-whitelist=127.0.0.1']] * self.num_nodes
        if hasattr(self, "extra_args"):
            extra_args = self.extra_args
        self.add_nodes(self.num_nodes, extra_args,
                       binary=[self.options.testbinary] +
                       [self.options.refbinary] * (self.num_nodes - 1))
        self.start_nodes()

class SkipTest(Exception):
    """This exception is raised to skip a test"""
    def __init__(self, message):
        self.message = message
