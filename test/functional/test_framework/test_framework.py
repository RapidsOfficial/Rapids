#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Base class for RPC testing."""

from enum import Enum
import logging
import optparse
import os
import pdb
import shutil
import sys
import tempfile
import time

from .authproxy import JSONRPCException
from .blocktools import vZC_DENOMS
from . import coverage
from .test_node import TestNode
from .util import (
    MAX_NODES,
    PortSeed,
    assert_equal,
    assert_greater_than,
    check_json_precision,
    connect_nodes_bi,
    disconnect_nodes,
    get_datadir_path,
    generate_pos,
    initialize_datadir,
    set_node_times,
    sync_blocks,
    sync_mempools,
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
        for i in range(self.num_nodes - 1):
            connect_nodes_bi(self.nodes, i, i + 1)
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
        connect_nodes_bi(self.nodes, 1, 2)
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
        """Initialize a pre-mined blockchain for use by the test.

        Create a cache of a 200-block-long chain (with wallet) for MAX_NODES
        Afterward, create num_nodes copies from the cache."""

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

        def start_nodes_from_dir(ddir):
            self.log.info("Starting %d nodes..." % MAX_NODES)
            for i in range(MAX_NODES):
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
            for node in range(4):
                for j in range(node+1, MAX_NODES):
                    connect_nodes_bi(self.nodes, node, j)

        def stop_and_clean_cache_dir(ddir):
            self.stop_nodes()
            self.nodes = []
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
            start_nodes_from_dir(powcachedir)

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
            # The next 48 PoW blocks are mined in 6-blocks bursts by the first 4 nodes (2 times).
            # The last 2 PoW blocks are then mined by the first node (Node 0).
            # Then 80 PoS blocks are generated in 5-blocks bursts by the first 4 nodes (4 times).
            #
            # - Node 0 gets 84 blocks:
            # 62 mature blocks (pow) and 22 immature (2 pow + 20 pos)
            # 42 rewards spendable (62 mature blocks - 20 spent rewards)
            # - Nodes 1 to 3 get 82 blocks each:
            # 56 mature blocks (pow) and 26 immature (6 pow + 20 pos)
            # 36 rewards spendable (56 mature blocks - 20 spent rewards)
            # - Nodes 2 and 3 both mint one zerocoin for each denom (tot 6666 PIV) on block 301/302
            # 8 mature zc + 9 rewards spendable (36 - 27 spent) + change 83.92
            #
            # Block 331-332 will mature last 2 pow blocks mined by node 0.
            # Then 333-338 will mature last 6 pow blocks mined by node 1.
            # Then 339-344 will mature last 6 pow blocks mined by node 2.
            # Then 345-350 will mature last 6 pow blocks mined by node 3.
            # Then staked blocks start maturing at height 351.

            # Create cache directories, run pivxds:
            create_cachedir(poscachedir)
            self.log.info("Creating 'PoS-chain': 330 blocks")
            self.log.info("Copying 200 initial blocks from pow cache")
            copy_cachedir(powcachedir, poscachedir)
            # Change datadir and restart the nodes
            start_nodes_from_dir(poscachedir)

            # Mine 50 more blocks to reach PoS start.
            self.log.info("Mining 50 more blocks to reach PoS phase")
            for i in range(2):
                for peer in range(4):
                    for j in range(6):
                        set_node_times(self.nodes, block_time)
                        self.nodes[peer].generate(1)
                        block_time += 60
                    # Must sync before next peer starts generating blocks
                    sync_blocks(self.nodes)
            set_node_times(self.nodes, block_time)
            self.nodes[0].generate(2)
            block_time += 60
            sync_blocks(self.nodes)

            # Then stake 80 blocks.
            self.log.info("Staking 80 blocks...")
            nBlocks = 250
            res = []    # used to save the two txids for change outputs of mints (locked)
            for i in range(4):
                for peer in range(4):
                    for j in range(5):
                        # Stake block
                        block_time = generate_pos(self.nodes, peer, block_time)
                        nBlocks += 1
                        # Mint zerocoins with node-2 at block 301
                        # Mint zerocoins with node-3 at block 302
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

                self.log.info("%d blocks staked" % int((i+1)*20))

            # Unlock previously locked change outputs
            for peer in [2, 3]:
                assert (self.nodes[peer].lockunspent(True, [{"txid": res[peer-2]['txid'], "vout": 8}]))

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


    def test_PoS_chain_balances(self):
        from .util import DecimalAmt
        # 330 blocks
        # - Node 0 gets 84 blocks:
        # 64 pow + 20 pos (22 immature)
        # - Nodes 1 to 3 get 82 blocks each:
        # 62 pow + 20 pos (26 immature)
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
        assert_equal(w_info[0]["balance"], 250.0 * (62 - 20))
        mature_balance = 250.0 * (56 - 20)
        for i in range(1, num_nodes):
            if i < 4:
                if i < 2:
                    assert_equal(w_info[i]["balance"], DecimalAmt(mature_balance))
                else:
                    # node 2 and 3 have minted zerocoins
                    assert_equal(w_info[i]["balance"], DecimalAmt(mature_balance - (used_utxos * 250) + zc_change))
            else:
                # only first 4 nodes have mined/staked
                assert_equal(w_info[i]["balance"], DecimalAmt(0))

        # immature balance is immature pow blocks rewards plus
        # immature stakes (outputs=inputs+rewards)
        assert_equal(w_info[0]["immature_balance"], DecimalAmt((250.0 * 2) + (500.0 * 20)))
        for i in range(1, num_nodes):
            if i < 4:
                assert_equal(w_info[i]["immature_balance"], DecimalAmt((250.0 * 6) + (500.0 * 20)))
            else:
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
                else:
                    # last mints added on accumulators - not spendable
                    assert_equal(0, len(zclist_spendable))
                assert_equal(set([x['denomination'] for x in zclist]), set(vZC_DENOMS))
                assert_equal([x['confirmations'] for x in zclist], [30-peer] * len(vZC_DENOMS))

        self.log.info("Balances of first %d nodes check out" % num_nodes)



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
