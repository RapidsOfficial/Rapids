(note: this is a temporary file, to be added-to by anybody, and moved to release-notes at release time)

PIVX Core version *version* is now available from:

  <https://github.com/pivx-project/pivx/releases>

This is a new major version release, including various bug fixes and
performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/pivx-project/pivx/issues>

Mandatory Update
==============


How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/PIVX-Qt (on Mac) or pivxd/pivx-qt (on Linux).

Compatibility
==============

PIVX Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support),
No attempt is made to prevent installing or running the software on Windows XP, you
can still do so at your own risk but be aware that there are known instabilities and issues.
Please do not report issues about Windows XP to the issue tracker.

PIVX Core should also work on most other Unix-like systems but is not
frequently tested on them.

Notable Changes
===============

Random-cookie RPC authentication
---------------------------------

When no `-rpcpassword` is specified, the daemon now uses a special 'cookie'
file for authentication. This file is generated with random content when the
daemon starts, and deleted when it exits. Its contents are used as
authentication token. Read access to this file controls who can access through
RPC. By default it is stored in the data directory but its location can be
overridden with the option `-rpccookiefile`.

This is similar to Tor's CookieAuthentication: see
https://www.torproject.org/docs/tor-manual.html.en

This allows running pivxd without having to do any manual configuration.


Autocombine changes
---------------------------------

The autocombine feature was carrying a bug leading to a significant CPU overhead
when being used. The function is now called only once initial blockchain
download is finished. It's also now avoiding to combine several times while
under the threshold in order to avoid additional transaction fees. Last but not
least, the fee computation as been changed and the dust from fee provisioning
is returned in the main output.


SOCKS5 Proxy bug
---------------------------------

When inputting wrong data into the GUI for a SOCKS5 proxy, the wallet would
crash and be unable to restart without accessing hidden configuration.
This crash has been fixed.


*version* Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in locating
the code changes and accompanying discussion, both the pull request and
git merge commit are mentioned.

### Broad Features
### P2P Protocol and Network Code
### GUI
### Miscellaneous

Credits
=======

Thanks to everyone who directly contributed to this release:


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/pivx-project-translations/).
