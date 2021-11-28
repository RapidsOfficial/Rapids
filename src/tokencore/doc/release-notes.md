Token Core v0.5.0
================

v0.5.0 is a major release and consensus critical in terms of the Token Layer protocol rules. An upgrade is mandatory, and highly recommended. Prior releases may not be compatible with new behaviour in this release.

**Note: the first time you run this version, all Token Layer transactions are reprocessed due to an database update, which may take 30 minutes up to several hours.**

Please report bugs using the issue tracker on GitHub:

  https://github.com/TokenLayer/tokencore/issues


Table of contents
=================

- [Token Core v0.5.0](#token-core-v050)
- [Upgrading and downgrading](#upgrading-and-downgrading)
  - [How to upgrade](#how-to-upgrade)
  - [Downgrading](#downgrading)
  - [Compatibility with Bitcoin Core](#compatibility-with-bitcoin-core)
- [Notable changes](#notable-changes)
  - [Fix startup issue of Token Core](#fix-startup-issue-of-token-core)
  - [Speed up RPC token_listpendingtransactions](#speed-up-rpc-token_listpendingtransactions)
  - [Rename TOKEN and TTOKEN to OMN and TOMN](#rename-token-and-ttoken-to-omn-and-tomn)
- [Change log](#change-log)
- [Credits](#credits)


Upgrading and downgrading
=========================

How to upgrade
--------------

If you are running Bitcoin Core or an older version of Token Core, shut it down. Wait until it has completely shut down, then copy the new version of `tokencored`, `tokencore-cli` and `tokencore-qt`. On Microsoft Windows the setup routine can be used to automate these steps.

During the first startup historical Token transactions are reprocessed and Token Core will not be usable for approximately 15 minutes up to two hours. The progress of the initial scan is reported on the console, the GUI and written to the `debug.log`. The scan may be interrupted, but can not be resumed, and then needs to start from the beginning.

Downgrading
-----------

Downgrading to an Token Core version prior to 0.4.0 is generally not advised as older versions may not provide accurate information due to the changes in consensus rules.

Compatibility with Bitcoin Core
-------------------------------

Token Core is based on Bitcoin Core 0.13.2 and can be used as replacement for Bitcoin Core. Switching between Token Core and Bitcoin Core may be supported.

Upgrading to a higher Bitcoin Core version is generally supported, but when downgrading from Bitcoin Core 0.15, Token Core needs to be started with `-reindex-chainstate` flag, to rebuild the chainstate data structures in a compatible format.

Downgrading to a Bitcoin Core version prior to 0.12 may not be supported due to the obfuscation of the blockchain database. In this case the database also needs to be rebuilt by starting Token Core with `-reindex-chainstate` flag.

Downgrading to a Bitcoin Core version prior to 0.10 is not supported due to the new headers-first synchronization.


Notable changes
===============

Fix startup issue of Token Core
------------------------------

During startup, when reloading the effect of freeze transactions, it is checked, whether the sender of a freeze transaction is the issuer of that token and thus allowed to freeze tokens. However, if the issuer of the token has been changed in the meantime, that check fails. Such a fail is interpreted as state inconsistency, which is critical and causes a shutdown of the client.

With this change, historical issuers are persisted and can be accessed for any given block. When there is an issuer check, it now checks against the issuer at that point, resolving the startup issue.

Please note: the internal database of Token Core is upgraded, which triggers a reparse of Token Layer transactions the first time this version is started. This can take between 30 minutes and a few hours of processing time, during which Token Core is unusable!

Speed up RPC "token_listpendingtransactions"
-------------------------------------------

When adding a transaction to the mempool, a quick and unsafe check for any Token Layer markers is done without checking transaction validity or whether it's malformed, to identify potential Token Layer transactions.

If the transaction has a potential marker, then it's added to a new cache. If the transaction is removed from the mempool, it's also removed from the cache.

This speeds up the RPC "token_listpendingtransactions" significantly, which can be used to list pending Token Layer transactions.

Rename TOKEN and TTOKEN to OMN and TOMN
-------------------------------------

To be more algined with other symbols and tickers the following changes in wording are made:

- "Token", referring to the native tokens of the Token Layer protocol, becomes "Token tokens"
- "Test Token", referring to the native test tokens of the Token Layer protocol, becomes "Test Token tokens"
- "TOKEN", referring to the symbol of Token tokens, becomes "OMN"
- "TTOKEN", referring to the symbol of Test Token tokens, becomes "TOMN"

While this is change is mostly cosmetic - in particular it changes the code documentation, RPC help messages and RPC documentation - it also has an effect of the RPCS "token_getproperty 1" and "token_getproperty 2", which now return a text with the updated token and symbol names.


Change log
==========

The following list includes relevant pull requests merged into this release:

```
- #907 Update version to 0.4.0.99 to indicate development
- #910 Add marker cache to speed up token_listpendingtransactions
- #925 Store historical issuers and use that data
- #908 Rename TOKEN and TTOKEN to OMN and TOMN
- #931 Bump version to Token Core 0.5.0
- #932 Add release notes for Token Core 0.5.0
```


Credits
=======

Thanks to everyone who contributed to this release!
