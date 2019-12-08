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

New Wallet UI
-------------------

v4.0.0 introduces a completely new GUI for the wallet, designed and coded from the ground up by the [Krubit](https://krubit.com/) team.

This new UI, aside from the overall design large implementation, includes user-focused improvements and features such as a brief introduction on first load, a FAQ section, one-click QRCode compatible receiving addresses, masternode creation wizard, dark and light themes, filterable staking charts, and much more.

You can read more details about this extensive work in ([PR #954](https://github.com/PIVX-Project/PIVX/pull/954))

There are some legacy features that have not been included, however, notably the in-wallet block explorer and the governance page. The in-wallet block explorer was sorely outdated, and the governance page was a newer addition that will be seeing a return in a future version.

Cold Staking
-------------------

A brand new feature is being introduced with the release of v4.0.0: Cold Staking ([PR #955](https://github.com/PIVX-Project/PIVX/pull/955))! This feature allows a coin owner to keep coins in a "cold" (or locked) wallet whilst a "hot" wallet carries out the burden of staking those coins.

This brings added security to coin owners as they are no longer required to use an unlocked or partially unlocked wallet (with the ability to spend coins anywhere) in order to gain staking rewards. Users who have chosen to store their coins on hardware devices such as a Ledger or Trezor<sup>1</sup> can also gain staking rewards with those coins.

A full technical writup is available on the [PIVX Wiki](https://github.com/PIVX-Project/PIVX/wiki/ColdStaking), and an initial video showcase is available on [YouTube](https://www.youtube.com/watch?v=utxB5TzAeXc).
A brief guide to setup cold staking with GUI and RPC is available [here](https://github.com/random-zebra/PIVX-Wiki/blob/master/User-Documentation/Cold-Staking-HowTo.md).

<sup>1</sup> Spending cold stakes from HW wallets currently available only for Ledger devices via [PET4L](https://github.com/PIVX-Project/PET4L) tool.

Multi-Split Stake Splitting
-------------------

Stake splitting has received a makeover and now supports splitting to more than two (2) outputs. [PR #968](https://github.com/PIVX-Project/PIVX/pull/968) introduced the change, which is controlled by the wallet's `stakesplitthreshold` setting.

The default split threshold remains at 2000 PIV, and can be adjusted in the GUI's Settings page, or via the RPC `setstakesplitthreshold` command.

For a real example, with a stake split threshold of 1500, and a UTXO of 4708.1557; the current stake split algorithm would break that into two outputs of approximately 2355.07785. With this new logic; it will be broken into 3 outputs instead of two; each sized 1570.0519 (4708.1557 input + 2 stake = 4710.1557 / 3 outputs = 1570.0519.

The maximum number of outputs is currently capped at 48. Also, in relation to the new Cold Staking feature described above; the stake split threshold is set by the staker wallet and **NOT** the owner wallet.

New Consensus Rules
-------------------

The following consensus rule changes will be enforced on or shortly after block `2153200`. Note that **Upgrade Enforcement** (mentioned above) will occur prior to this block height.

### V1 zPIV Spending (Public Spends Version 4)

Since the discovery of a critical exploit within the libzerocoin library in early 2019, remaining legacy v1 zPIV have been un-spendable. We're happy to say that, once the new consensus rules are in effect, users will once again be able to spend their v1 zPIV with public spends version 4 ([PR #936](https://github.com/PIVX-Project/PIVX/pull/936)).

As with the previous version 3 public spends introduced in core wallet version 3.3.0 (enabling the spending of v2 zPIV), version 4 spends will also be public. A full technical writeup is available on the [PIVX Wiki](https://github.com/PIVX-Project/PIVX/wiki/CoinRandomnessSchnorrSignature).

### OP_CHECKCOLDSTAKEVERIFY and P2CS

Cold staking introduces a new opcode, `OP_CHECKCOLDSTAKEVERIFY`, in the scripting language, and a new standard transaction type using it, named `P2CS` (Pay-To-Cold-Staking). A P2CS script is defined as follows:
```
OP_DUP OP_HASH160 OP_ROT OP_IF OP_CHECKCOLDSTAKEVERIFY [HASH160(stakerPubKey]
OP_ELSE [HASH160(ownerPubKey)] OP_ENDIF OP_EQUALVERIFY OP_CHECKSIG
```
`OP_CHECKCOLDSTAKEVERIFY` is used to ensure that the staker can only spend the output in a valid coinstake transaction (using the same P2CS script in the new output).

### Time Protocol v2

[#PR1002](https://github.com/PIVX-Project/PIVX/pull/1002) introduces a new time protocol for the Proof-Of-Stake consensus mechanism, to ensure better efficiency, fairness and security. The time is now divided in 15-seconds slots and valid blocktimes are at the beginning of each slot (i.e. the block timestamp's seconds can only be `00`, or `15`, or `30` or `45`).<br>
The maximum future time limit is lowered from 3 minutes to 14 seconds and the past limit is set to the previous blocktime (i.e. a block can no longer have a timestamp earlier than its previous block).<br>
This means that, when looking for a valid kernel, each stakeable input can be hashed only once every 15 seconds (once per timeslot), and it is not possible to submit blocks with timestamp higher than the current time slot. This ultimately enables the removal of the "hashdrift" concept.<br>

**NOTE:** Given the much stricter time constraints, a node's clock synchronization is required for P2P connections: the maximum time offset is 15 seconds and peers with a time drift higher than 30 seconds (in absolute value) will be outright disconnected.

 For advanced users, we recommend the setup of NTP clients and own servers. This will provide to your node a higher level of time accuracy and the best, time wise, synchronization with the peers in the network.

### Block Version 7

[#PR1022](https://github.com/PIVX-Project/PIVX/pull/1022) defines Version 7 blocks, which remove the (now-unused) accumulator checkpoint from the block header. This results in an overall data reduction of ~256 bits from each block as well as the in-memory indexes.

### New Network Message Signatures

Layer 2 network messages (MN, Budget, Spork, etc) are now signed based on the hash of their **binary** content instead of their **string** representation ([#PR1024](https://github.com/PIVX-Project/PIVX/pull/1024)).

### New SPORKS

Two new SPORKS are introduced, `SPORK_17` ([#PR975](https://github.com/PIVX-Project/PIVX/pull/975)) and `SPORK_18` ([#PR995](https://github.com/PIVX-Project/PIVX/pull/995)).<br>
`SPORK_17` (off by default) is used to activate the [Cold Staking](#cold-staking) protocol. When this spork is off, no cold-staked block is accepted by the network and new delegations are rejected, but coin-owners are still able to spend previously created pay-to-cold-stake delegations.

`SPORK_18` (off by default) is used to switch between Version 3 and [Version 4 Public Spends](#v1-zpiv-spending-public-spends-version-4). When this spork is active, only version 4 spends are accepted by the network. When it's not, only version 3 spends are accepted.

RPC Changes
--------------

### New options for existing wallet commands

A new (optional) argument, `includeDelegated`, has been added to the following commands that allows these commands to include delegated coins/information in their operation:
- `getbalance` - Boolean (Default: True)
- `sendfrom` - Boolean (Default: False)
- `sendmany` - Boolean (Default: False)
- `listtransactions` - Boolean (Default: True)

Additionally, a new (optional) argument, `includeCold`, has been added to the `listtransactions` command (Boolean - Default: True), which allows for filtering of cold-staker delegated transactions.

### New return fields for existing commands

The `validateaddress` command now includes an additional response field, `isstaking`, to indicate wither or not the specified address is a cold staking address.

The `getwalletinfo` command now includes two additional response fields:
- `delegated_balance` - PIV balance held in P2CS contracts (delegated amount total).
- `cold_staking_balance` - PIV balance held in cold staking addresses.

### Newly introduced commands

The following new commands have been added to the RPC interface:
- `getnewstakingaddress`
- `delegatestake`
- `rawdelegatestake`
- `getcoldstakingbalance`
- `delegatoradd`
- `delegatorremove`
- `listcoldutxos`
- `liststakingaddresses`
- `listdelegators`

Details about each new command can be found below.

`getnewstakingaddress` generates a new cold staking address:
```
getnewstakingaddress ( "account" )

Returns a new PIVX cold staking address for receiving delegated cold stakes.

Arguments:
1. "account"        (string, optional) The account name for the address to be linked to. if not provided, the default account "" is used. It can also be set to the empty string "" to represent the default account. The account does not need to exist, it will be created if there is no account by the given name.

Result:
"pivxaddress"    (string) The new pivx address
```

`delegatestake` sends a cold staking delegation transaction:
```
delegatestake "stakingaddress" amount ( "owneraddress" fExternalOwner fUseDelegated fForceNotEnabled )

Delegate an amount to a given address for cold staking. The amount is a real and is rounded to the nearest 0.00000001

Requires wallet passphrase to be set with walletpassphrase call.

Arguments:
1. "stakingaddress"      (string, required) The pivx staking address to delegate.
2. "amount"              (numeric, required) The amount in PIV to delegate for staking. eg 100
3. "owneraddress"        (string, optional) The pivx address corresponding to the key that will be able to spend the stake.
                               If not provided, or empty string, a new wallet address is generated.
4. "fExternalOwner"      (boolean, optional, default = false) use the provided 'owneraddress' anyway, even if not present in this wallet.
                               WARNING: The owner of the keys to 'owneraddress' will be the only one allowed to spend these coins.
5. "fUseDelegated"       (boolean, optional, default = false) include already delegated inputs if needed.6. "fForceNotEnabled"    (boolean, optional, default = false) force the creation even if SPORK 17 is disabled (for tests).

Result:
{
   "owner_address": "xxx"   (string) The owner (delegator) owneraddress.
   "staker_address": "xxx"  (string) The cold staker (delegate) stakingaddress.
   "txid": "xxx"            (string) The stake delegation transaction id.
}
```

`rawdelegatestake` creates a raw cold staking delegation transaction without broadcasting it to the network:
```
rawdelegatestake "stakingaddress" amount ( "owneraddress" fExternalOwner fUseDelegated )

Delegate an amount to a given address for cold staking. The amount is a real and is rounded to the nearest 0.00000001

Delegate transaction is returned as json object.
Requires wallet passphrase to be set with walletpassphrase call.

Arguments:
1. "stakingaddress"      (string, required) The pivx staking address to delegate.
2. "amount"              (numeric, required) The amount in PIV to delegate for staking. eg 100
3. "owneraddress"        (string, optional) The pivx address corresponding to the key that will be able to spend the stake.
                               If not provided, or empty string, a new wallet address is generated.
4. "fExternalOwner"      (boolean, optional, default = false) use the provided 'owneraddress' anyway, even if not present in this wallet.
                               WARNING: The owner of the keys to 'owneraddress' will be the only one allowed to spend these coins.
5. "fUseDelegated         (boolean, optional, default = false) include already delegated inputs if needed.

Result:
{
  "txid" : "id",        (string) The transaction id (same as provided)
  "version" : n,          (numeric) The version
  "size" : n,             (numeric) The serialized transaction size
  "locktime" : ttt,       (numeric) The lock time
  "vin" : [               (array of json objects)
     {
       "txid": "id",    (string) The transaction id
       "vout": n,         (numeric)
       "scriptSig": {     (json object) The script
         "asm": "asm",  (string) asm
         "hex": "hex"   (string) hex
       },
       "sequence": n      (numeric) The script sequence number
     }
     ,...
  ],
  "vout" : [              (array of json objects)
     {
       "value" : x.xxx,            (numeric) The value in btc
       "n" : n,                    (numeric) index
       "scriptPubKey" : {          (json object)
         "asm" : "asm",          (string) the asm
         "hex" : "hex",          (string) the hex
         "reqSigs" : n,            (numeric) The required sigs
         "type" : "pubkeyhash",  (string) The type, eg 'pubkeyhash'
         "addresses" : [           (json array of string)
           "pivxaddress"        (string) pivx address
           ,...
         ]
       }
     }
     ,...
  ],
  "hex" : "data",       (string) The serialized, hex-encoded data for 'txid'
}
```

`getcoldstakingbalance` returns the cold balance of the wallet:
```
getcoldstakingbalance ( "account" )

If account is not specified, returns the server's total available cold balance.
If account is specified, returns the cold balance in the account.
Note that the account "" is not the same as leaving the parameter out.
The server total may be different to the balance in the default "" account.

Arguments:
1. "account"      (string, optional) The selected account, or "*" for entire wallet. It may be the default account using "".

Result:
amount              (numeric) The total amount in PIV received for this account in P2CS contracts.
```

`delegatoradd` whitelists a delegated owner address for cold staking:
```
delegatoradd "addr"

Add the provided address <addr> into the allowed delegators AddressBook.
This enables the staking of coins delegated to this wallet, owned by <addr>

Arguments:
1. "addr"        (string, required) The address to whitelist

Result:
true|false           (boolean) true if successful.
```

`delegatorremove` to remove previously whitelisted owner address:
```
delegatoradd "addr"

Add the provided address <addr> into the allowed delegators AddressBook.
This enables the staking of coins delegated to this wallet, owned by <addr>

Arguments:
1. "addr"        (string, required) The address to whitelist

Result:
true|false           (boolean) true if successful.
```

`listcoldutxos` lists all P2CS UTXOs belonging to the wallet (both delegator and cold staker):
```
listcoldutxos ( nonWhitelistedOnly )

List P2CS unspent outputs received by this wallet as cold-staker-

Arguments:
1. nonWhitelistedOnly   (boolean, optional, default=false) Whether to exclude P2CS from whitelisted delegators.

Result:
[
  {
    "txid" : "true",            (string) The transaction id of the P2CS utxo
    "txidn" : "accountname",    (string) The output number of the P2CS utxo
    "amount" : x.xxx,             (numeric) The amount of the P2CS utxo
    "confirmations" : n           (numeric) The number of confirmations of the P2CS utxo
    "cold-staker" : n             (string) The cold-staker address of the P2CS utxo
    "coin-owner" : n              (string) The coin-owner address of the P2CS utxo
    "whitelisted" : n             (string) "true"/"false" coin-owner in delegator whitelist
  }
  ,...
]
```

`liststakingaddresses` lists all cold staking addresses generated by the wallet:
```
liststakingaddresses "addr"

Shows the list of staking addresses for this wallet.

Result:
[
   {
   "label": "yyy",  (string) account label
   "address": "xxx",  (string) PIVX address string
   }
  ...
]
```

`listdelegators` lists the whitelisted owner addresses
```
listdelegators "addr"

Shows the list of allowed delegator addresses for cold staking.

Result:
[
   {
   "label": "yyy",  (string) account label
   "address": "xxx",  (string) PIVX address string
   }
  ...
]
```

Snapcraft Packages
------------------

For our linux users, in addition to the [Ubuntu PPA](https://launchpad.net/~pivx) repository, we are now offering a [Snap package](https://snapcraft.io/pivx-core) as quick way to install and update a PIVX wallet.

Release versions are available via the `Stable` branch, and (for testing-only purposes) nightly builds are available in the `Beta` branch.

Internal Miner/Staker Change
--------------

The wallet's internal miner/staker is no longer prevented from running prior to having synced all the additional layer 2 (MN/Budget) data. Instead, mining/staking uses better logic to allow block creation without fully synced layer 2 data when the full data set wouldn't be required.

In other words, try to stake a new block only if:

- full layer 2 sync is complete
OR
- The spork list is synced and all three sporks (8,9 and 13) are **not** active.

Faster Shutdown During Initial Loading
--------------

Previously, if a user wanted to close/quit the wallet before it had finished its initial loading process, they would need to wait until that loading process actually completed before the wallet would fully close.

Now, the new behavior is to gracefully close the wallet once the current step is complete.

*version* Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Core Features

### Build System

### P2P Protocol and Network Code

### GUI

### RPC/REST

### Wallet

### Miscellaneous


## Credits

Thanks to everyone who directly contributed to this release:


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/pivx-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.
