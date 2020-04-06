(note: this is a temporary file, to be added-to by anybody, and moved to release-notes at release time)

PIVX Core version *version* is now available from:  <https://github.com/pivx-project/pivx/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/pivx-project/pivx/issues>


Mandatory Update
==============


How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/PIVX-Qt (on Mac) or pivxd/pivx-qt (on Linux).


Compatibility
==============

PIVX Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.10+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

Apple released it's last Mountain Lion update August 13, 2015, and officially ended support on [December 14, 2015](http://news.fnal.gov/2015/10/mac-os-x-mountain-lion-10-8-end-of-life-december-14/). PIVX Core software starting with v3.2.0 will no longer run on MacOS versions prior to Yosemite (10.10). Please do not report issues about MacOS versions prior to Yosemite to the issue tracker.

PIVX Core should also work on most other Unix-like systems but is not frequently tested on them.


Notable Changes
==============

(Developers: add your notes here as part of your pull requests whenever possible)

### Hierarchical Deterministic Wallet (HD Wallet)

Wallets under a tree derivation structure in which keypairs are generated deterministically from a single seed, which can be shared partially or entirely with different systems, each with or without the ability to spend coins, [BIP32](https://github.com/bitcoin/bips/blob/master/bip-0032.mediawiki).

Enabling major improvements over the keystore management, the PIVX wallet doesn't require regular backups as before, keys are following a deterministic creation path that can be verified at any time (before HD Wallet, every keypair was randomly created and added to the keypool, forcing the user to backup the wallet every certain amount of time or could end up loosing coins forever if the latest `wallet.dat` was not being used).
As well as new possibilities like the account extended public key that enables deterministic public keys creation without the private keys requisite inside the wallet (A good use case could be online stores generating fresh addresses).

This work includes a customization/extension to the [BIP44](https://github.com/bitcoin/bips/blob/master/bip-0044.mediawiki) standard. We have included an unique staking keys derivation path which introduced the deterministic generation/recovery of staking addresses.

An extended description of this large work can be found in the PR [here](https://github.com/PIVX-Project/PIVX/pull/1327).

#### HD Wallet FAQ

 - How do i upgrade to HD Wallet?

    GUI:
    1) A dialog will appear on every wallet startup notifying you that you are running a pre-HD wallet and letting you upgrade it from there.
    2) If you haven't upgraded your wallet, the topbar (bar with icons that appears at the top of your wallet) will have an "HD" icon. Click it and the upgrade dialog will be launched.

    RPC:
    1) If your wallet is unlocked, use the `-upgradewallet` flag at startup and will automatically upgrade your wallet.
    2) If your wallet is encrypted, use the `upgradewallet` rpc command. It will upgrade your wallet to the latest wallet version.

 - How do i know if i'm already running an HD Wallet?

    1) GUI: Go to settings, press on the Debug option, then Information.
    2) RPC: call `getwalletinfo`, the `walletversion` field must be `169900` (HD Wallet Feature).


### Functional Changes

Automatic zPIV backup has been disabled. Thus, the following configuration options have been removed  (either as entries in the pivx.conf file or as startup flags):
- `autozpivbackup`
- `backupzpiv`
- `zpivbackuppath`

### Stake-Split threshold
The stake split threshold is no longer required to be integer. It can be a fractional amount. A threshold value of 0 disables the stake-split functionality.
The default value for the stake-split threshold has been lowered from 2000 PIV, down  to 500 PIV.


Dependencies
------------

The minimum required version of QT has been increased from 5.0 to 5.5.1 (the [depends system](https://github.com/pivx-project/pivx/blob/master/depends/README.md) provides 5.9.7)


RPC Changes
--------------

### Modified input/output for existing commands

- "CoinStake" JSON object in `getblock` output is removed, and replaced with the strings "stakeModifier" and "hashProofOfStake"
- "obfcompat" JSON field in `getmasternodecount` output is removed as it is/was redundant with the `enabled` field.


- "moneysupply" and "zpivSupply" attributes in `getblock` output are removed.


- "isPublicSpend" boolean (optional) input parameter is removed from the following commands:
 - `createrawzerocoinspend`
 - `spendzerocoin`
 - `spendzerocoinmints`
 - `spendrawzerocoin`

 These commands are now able to create only *public* spends (private spends were already enabled only on regtest).


- "mintchange" and "minimizechange" boolean input parameters are removed from the following commands:
 - `spendzerocoin`

 Mints are disabled, therefore it is no longer possible to mint the change of a zerocoin spend. The change is minimized by default.


- `setstakesplitthreshold` now accepts decimal amounts. If the provided value is `0`, split staking gets disabled. `getstakesplitthreshold` returns a double.

- `dumpwallet` no longer allows overwriting files. This is a security measure
   as well as prevents dangerous user mistakes.

- The output of `getstakingstatus` was reworked. It now shows the following information:
  ```
  {
     "staking_status": true|false,       (boolean) whether the wallet is staking or not
     "staking_enabled": true|false,      (boolean) whether staking is enabled/disabled in pivx.conf
     "coldstaking_enabled": true|false,  (boolean) whether cold-staking is enabled/disabled in pivx.conf
     "haveconnections": true|false,      (boolean) whether network connections are present
     "mnsync": true|false,               (boolean) whether masternode data is synced
     "walletunlocked": true|false,       (boolean) whether the wallet is unlocked
     "stakeablecoins": n,                (numeric) number of stakeable UTXOs
     "stakingbalance": d,                (numeric) PIV value of the stakeable coins (minus reserve balance, if any)
     "stakesplitthreshold": d,           (numeric) value of the current threshold for stake split
     "lastattempt_age": n,               (numeric) seconds since last stake attempt
     "lastattempt_depth": n,             (numeric) depth of the block on top of which the last stake attempt was made
     "lastattempt_hash": xxx,            (hex string) hash of the block on top of which the last stake attempt was made
     "lastattempt_coins": n,             (numeric) number of stakeable coins available during last stake attempt
     "lastattempt_tries": n,             (numeric) number of stakeable coins checked during last stake attempt
   }
   ```


### Removed commands

The following commands have been removed from the RPC interface:
- `createrawzerocoinstake`
- `getmintsinblocks`
- `reservebalance`
- `getpoolinfo`

### Newly introduced commands

The following new commands have been added to the RPC interface:
- `logging` <br>Gets and sets the logging configuration.<br>
When called without an argument, returns the list of categories that are currently being debug logged.<br>
When called with arguments, adds or removes categories from debug logging.<br>
E.g. `logging "[\"all\"]" "[\"http\"]""`


Details about each new command can be found below.


Changed command-line options
-----------------------------

- `-debuglogfile=<file>` can be used to specify an alternative debug logging file. This can be an absolute path or a path relative to the data directory


*version* Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Core Features

### Build System

### P2P Protocol and Network Code

The p2p alert system has been removed in [PR #1372](https://github.com/PIVX-Project/PIVX/pull/1372) and the 'alert' message is no longer supported.

### GUI

**Keyboard navigation**: dialogs can now be accepted with the `ENTER` (`RETURN`) key, and dismissed with the `ESC` key ([#1392](https://github.com/PIVX-Project/PIVX/pull/1392)).


**Address sorting**: address sorting in "My Addresses" / "Contacts" / "Cold Staking" can now be customized, setting it either by label (default), by address, or by creation date, ascending (default) or descending order.
Addresses in the dropdown of the "Send Transaction" and "Send Delegation" widgets are now automatically sorted by label with ascending order ([#1393](https://github.com/PIVX-Project/PIVX/pull/1393)).


**Custom Fee**: The custom fee selected when sending a transaction is now saved in the wallet database and persisted across multiple sends and wallet's restarts ([#1406](https://github.com/PIVX-Project/PIVX/pull/1406)).


**Include delegations in send**: The send and cold-staking page present a checkbox ([#1391](https://github.com/PIVX-Project/PIVX/pull/1391)) to make the automatic input selection algorithm include delegated (P2CS) utxos if needed. The option is unchecked by default.


**Staking Charts**: can now be hidden at startup (with a flag `--hidecharts`) or at runtime with a checkbox in settings --> options --> display ([PR #1475](https://github.com/PIVX-Project/PIVX/pull/1475)).



### RPC/REST

### Wallet


__Context Lock/Unlock__ [[PR #1387](https://github.com/PIVX-Project/PIVX/pull/1387)]:<br>
Present the unlock dialog directly (instead of an error message), whenever an action on encrypted/locked wallet requires full unlock.<br>
Restore the previous locking state ("locked" or "locked for staking only") when the action is completed.


__Configuration Options__:

- The `-reservebalance` configuration/startup option has been removed ([PR #1373](https://github.com/PIVX-Project/PIVX/pull/1373)).

### Miscellaneous

## Credits

Thanks to everyone who directly contributed to this release:


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/pivx-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.
