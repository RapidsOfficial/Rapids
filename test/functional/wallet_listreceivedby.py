#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the listreceivedbyaddress RPC."""
from decimal import Decimal

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_array_result,
    assert_equal,
    assert_raises_rpc_error,
    sync_blocks,
)


class ReceivedByTest(PivxTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def run_test(self):
        # Generate block to get out of IBD
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        self.log.info("listreceivedbyaddress Test")

        # Send from node 0 to 1
        addr = self.nodes[1].getnewaddress()
        txid = self.nodes[0].sendtoaddress(addr, 0.1)
        self.sync_all()

        # Check not listed in listreceivedbyaddress because has 0 confirmations
        assert_array_result(self.nodes[1].listreceivedbyaddress(),
                            {"address": addr},
                            {},
                            True)
        # Bury Tx under 10 block so it will be returned by listreceivedbyaddress
        self.nodes[1].generate(10)
        self.sync_all()
        assert_array_result(self.nodes[1].listreceivedbyaddress(),
                            {"address": addr},
                            {"address": addr, "label": "", "amount": Decimal("0.1"), "confirmations": 10, "txids": [txid, ]})
        # With min confidence < 10
        assert_array_result(self.nodes[1].listreceivedbyaddress(5),
                            {"address": addr},
                            {"address": addr, "label": "", "amount": Decimal("0.1"), "confirmations": 10, "txids": [txid, ]})
        # With min confidence > 10, should not find Tx
        assert_array_result(self.nodes[1].listreceivedbyaddress(11), {"address": addr}, {}, True)

        # Empty Tx
        addr = self.nodes[1].getnewaddress()
        assert_array_result(self.nodes[1].listreceivedbyaddress(0, True),
                            {"address": addr},
                            {"address": addr, "label": "", "amount": 0, "confirmations": 0, "txids": []})

        self.log.info("getreceivedbyaddress Test")

        # Send from node 0 to 1
        addr = self.nodes[1].getnewaddress()
        txid = self.nodes[0].sendtoaddress(addr, 0.1)
        self.sync_all()

        # Check balance is 0 because of 0 confirmations
        balance = self.nodes[1].getreceivedbyaddress(addr)
        assert_equal(balance, Decimal("0.0"))

        # Check balance is 0.1
        balance = self.nodes[1].getreceivedbyaddress(addr, 0)
        assert_equal(balance, Decimal("0.1"))

        # Bury Tx under 10 block so it will be returned by the default getreceivedbyaddress
        self.nodes[1].generate(10)
        self.sync_all()
        balance = self.nodes[1].getreceivedbyaddress(addr)
        assert_equal(balance, Decimal("0.1"))

        # Trying to getreceivedby for an address the wallet doesn't own should return an error
        assert_raises_rpc_error(-4, "Address not found in wallet", self.nodes[0].getreceivedbyaddress, addr)

        self.log.info("listreceivedbylabel + getreceivedbylabel Test")

        # set pre-state
        addrArr = self.nodes[1].getnewaddress()
        label = self.nodes[1].getaccount(addrArr)
        received_by_label_json = [r for r in self.nodes[1].listreceivedbyaccount() if r["label"] == label][0]
        balance_by_label = self.nodes[1].getreceivedbyaccount(label)

        txid = self.nodes[0].sendtoaddress(addr, 0.1)
        self.sync_all()

        # listreceivedbylabel should return received_by_label_json because of 0 confirmations
        assert_array_result(self.nodes[1].listreceivedbylabel(),
                            {"label": label},
                            received_by_label_json)

        # getreceivedbyaddress should return same balance because of 0 confirmations
        balance = self.nodes[1].getreceivedbylabel(label)
        assert_equal(balance, balance_by_label)

        self.nodes[1].generate(10)
        self.sync_all()
        # listreceivedbylabel should return updated received list
        assert_array_result(self.nodes[1].listreceivedbylabel(),
                            {"label": label},
                            {"label": received_by_label_json["account"], "amount": (received_by_label_json["amount"] + Decimal("0.1"))})

        # getreceivedbylabel should return updated receive total
        balance = self.nodes[1].getreceivedbylabel(label)
        assert_equal(balance, balance_by_label + Decimal("0.1"))

        # Create a new label named "mynewlabel" that has a 0 balance
        self.nodes[1].getlabeladdress("mynewlabel", True)
        received_by_label_json = [r for r in self.nodes[1].listreceivedbylabel(0, True) if r["label"] == "mynewlabel"][0]

        # Test includeempty of listreceivedbylabel
        assert_equal(received_by_label_json["amount"], Decimal("0.0"))

        # Test getreceivedbylabel for 0 amount labels
        balance = self.nodes[1].getreceivedbylabel("mynewlabel")
        assert_equal(balance, Decimal("0.0"))

if __name__ == '__main__':
    ReceivedByTest().main()
