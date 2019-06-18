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

## zPIV Public Spends

Recent exploits of the Zerocoin protocol (Wrapped serials and broken P1 proof) required us to enable the zerocoin spork and deactivate zPIV functionality in order to secure the supply until the pertinent review process was completed.

Moving forward from this undesired situation, we are enabling a secure and chain storage friendly solution for the zerocoin public spend (aka zPIV to PIV conversion).

The explanation of how this works can be found in #891

After block `1,880,000` has past, `SPORK_16` will be deactivated to allow zPIV spends to occur using this new public spend method for version 2 zPIV (version 1 zPIV won't be spendable, see note below). zPIV public spends, as the name suggests, are **NOT** private, they reveal the input mint that is being spent. The minting of **NEW** zPIV, as well as zPIV staking will remain disabled for the time being.

It is advised that users spend/convert their existing zPIV to PIV, which can be done via the GUI or RPC as it was prior to the disabling of zPIV. Note that with the public spend method, the restriction on the number of denominations per transaction (previously 7) has been lifted, and now allows for several hundred denominations per transaction.

*Note on version 1 zPIV*: Version 1 zPIV was only available to me minted between versions v3.0.0 (Oct 6, 2017) and v3.1.0 (May 8, 2018). The announcement that version 1 zPIV was deprecated went out on May 1, 2018 with a recommendation for users to spend/convert their version 1 zPIV.

Version 1 zPIV will be made spendable at a later date due to the extra work required in order to make these version 1 mints spendable.

## GUI Changes

### Options Dialog Cleanup

The options/settings UI dialog has been cleaned up to no longer show settings that are wallet related when running in "disable wallet" (`-disablewallet`) mode.

### Privacy Tab

Notice text has been added to the privacy tab indicating that zPIV minting is disabled, as well as the removal of UI elements that supported such functionality. Notice text has also been added indicating that zPIV spends are currently **NOT** private.

## RPC Changes

### Removal of Deprecated Commands

The `masternode` and `mnbudget` RPC commands, which were marked as deprecated in PIVX Core v2.3.1 (September 19, 2017), have now been completely removed from PIVX Core.

Several new commands were added in v2.3.1 to replace the two aforementioned commands, reference the [v2.3.1 Release Notes](https://github.com/PIVX-Project/PIVX/blob/master/doc/release-notes/release-notes-2.3.1.md#rpc-changes) for further details.

### New `getblockindexstats` Command

A new RPC command (`getblockindexstats`) has been introduced which serves the purpose of obtaining statistical information on a range of blocks. The information returned is as follows:
  * transaction count (not including coinbase/coinstake txes)
  * transaction count (including coinbase/coinstake txes)
  * zPIV per-denom mint count
  * zPIV per-denom spend count
  * total transaction bytes
  * total fees in block range
  * average fee per kB

Command Reference:
```$xslt
getblockindexstats height range ( fFeeOnly )
nReturns aggregated BlockIndex data for blocks
height, height+1, height+2, ..., height+range-1]

nArguments:
1. height             (numeric, required) block height where the search starts.
2. range              (numeric, required) number of blocks to include.
3. fFeeOnly           (boolean, optional, default=False) return only fee info.
```
Result:
```
{
  first_block: x,              (integer) First counted block
  last_block: x,               (integer) Last counted block
  txcount: xxxxx,              (numeric) tx count (excluding coinbase/coinstake)
  txcount_all: xxxxx,          (numeric) tx count (including coinbase/coinstake)
  mintcount: {              [if fFeeOnly=False]
        denom_1: xxxx,         (numeric) number of mints of denom_1 occurred over the block range
        denom_5: xxxx,         (numeric) number of mints of denom_5 occurred over the block range
         ...                    ... number of mints of other denominations: ..., 10, 50, 100, 500, 1000, 5000
  },
  spendcount: {             [if fFeeOnly=False]
        denom_1: xxxx,         (numeric) number of spends of denom_1 occurred over the block range
        denom_5: xxxx,         (numeric) number of spends of denom_5 occurred over the block range
         ...                    ... number of spends of other denominations: ..., 10, 50, 100, 500, 1000, 5000
  },
  pubspendcount: {          [if fFeeOnly=False]
        denom_1: xxxx,         (numeric) number of PUBLIC spends of denom_1 occurred over the block range
        denom_5: xxxx,         (numeric) number of PUBLIC spends of denom_5 occurred over the block range
         ...                   ... number of PUBLIC spends of other denominations: ..., 10, 50, 100, 500, 1000, 5000
  },
  txbytes: xxxxx,              (numeric) Sum of the size of all txes (zPIV excluded) over block range
  ttlfee: xxxxx,               (numeric) Sum of the fee amount of all txes (zPIV mints excluded) over block range
  ttlfee_all: xxxxx,           (numeric) Sum of the fee amount of all txes (zPIV mints included) over block range
  feeperkb: xxxxx,             (numeric) Average fee per kb (excluding zc txes)
}
```

## Build System Changes

### New Architectures for Depends

The depends system has new added support for the `s390x` and `ppc64el` architectures. This is done in order to support the future integration with [Snapcraft](https://www.snapcraft.io), as well as to support any developers who may use systems based on such architectures.

### Basic CMake Support

While the existing Autotools based build system is our standard build system, and will continue to be so, we have added basic support for compiling with CMake on macOS and linux systems.

This is intended to be used in conjunction with IDEs like CLion (which relies heavily on CMake) in order to streamline the development process. Developers can now use, for example, CLion's internal debugger and profiling tools.

Note that it is still required to have relevant dependencies installed on the system for this to function properly.

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
