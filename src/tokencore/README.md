Token Core (beta) integration/staging tree
=========================================

[![Build Status](https://travis-ci.org/TokenLayer/tokencore.svg?branch=tokencore-0.0.10)](https://travis-ci.org/TokenLayer/tokencore)

What is the Token Layer
----------------------
The Token Layer is a communications protocol that uses the Bitcoin block chain to enable features such as smart contracts, user currencies and decentralized peer-to-peer exchanges. A common analogy that is used to describe the relation of the Token Layer to Bitcoin is that of HTTP to TCP/IP: HTTP, like the Token Layer, is the application layer to the more fundamental transport and internet layer of TCP/IP, like Bitcoin.

http://www.tokenlayer.org

What is Token Core
-----------------

Token Core is a fast, portable Token Layer implementation that is based off the Bitcoin Core codebase (currently 0.13.2). This implementation requires no external dependencies extraneous to Bitcoin Core, and is native to the Bitcoin network just like other Bitcoin nodes. It currently supports a wallet mode and is seamlessly available on three platforms: Windows, Linux and Mac OS. Token Layer extensions are exposed via the UI and the JSON-RPC interface. Development has been consolidated on the Token Core product, and it is the reference client for the Token Layer.

Disclaimer, warning
-------------------
This software is EXPERIMENTAL software. USE ON MAINNET AT YOUR OWN RISK.

By default this software will use your existing Bitcoin wallet, including spending bitcoins contained therein (for example for transaction fees or trading).
The protocol and transaction processing rules for the Token Layer are still under active development and are subject to change in future.
Token Core should be considered an alpha-level product, and you use it at your own risk. Neither the Token Foundation nor the Token Core developers assumes any responsibility for funds misplaced, mishandled, lost, or misallocated.

Further, please note that this installation of Token Core should be viewed as EXPERIMENTAL. Your wallet data, bitcoins and Token Layer tokens may be lost, deleted, or corrupted, with or without warning due to bugs or glitches. Please take caution.

This software is provided open-source at no cost. You are responsible for knowing the law in your country and determining if your use of this software contravenes any local laws.

PLEASE DO NOT use wallet(s) with significant amounts of bitcoins or Token Layer tokens while testing!

Related projects
----------------

* https://github.com/TokenLayer/TokenJ

* https://github.com/TokenLayer/tokenwallet

* https://github.com/TokenLayer/spec

Support
-------

* Please open a [GitHub issue](https://github.com/TokenLayer/tokencore/issues) to file a bug submission.
