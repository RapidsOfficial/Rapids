PIVX Core
=============

Setup
---------------------
[PIVX Core](http://pivx.org/wallet) is the original PIVX client and it builds the backbone of the network. However, it downloads and stores the entire history of PIVX transactions; depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more. Thankfully you only have to do this once.

Running
---------------------
The following are some helpful notes on how to run PIVX Core on your native platform.

### Unix

Unpack the files into a directory and run:

- `bin/pivx-qt` (GUI) or
- `bin/pivxd` (headless)

### Windows

Unpack the files into a directory, and then run pivx-qt.exe.

### macOS

Drag PIVX-Qt to your applications folder, and then run PIVX-Qt.

### Need Help?

* See the documentation at the [PIVX Wiki](https://github.com/PIVX-Project/PIVX/wiki)
for help and more information.
* Ask for help on [BitcoinTalk](https://bitcointalk.org/index.php?topic=1262920.0) or on the [PIVX Forum](http://forum.pivx.org/).
* Join our Discord server [Discord Server](https://discord.pivx.org)

Building
---------------------
The following are developer notes on how to build PIVX Core on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](dependencies.md)
- [macOS Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows.md)
- [Gitian Building Guide](gitian-building.md)

Development
---------------------
The PIVX repo's [root README](/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Multiwallet Qt Development](multiwallet-qt.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Source Code Documentation (External Link)](https://www.fuzzbawls.pw/pivx/doxygen/)
- [Translation Process](translation_process.md)
- [Unit Tests](unit-tests.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Dnsseed Policy](dnsseed-policy.md)

### Resources
* Discuss on the [BitcoinTalk](https://bitcointalk.org/index.php?topic=1262920.0) or the [PIVX](http://forum.pivx.org/) forum.
* Join the [PIVX Discord](https://discord.pivx.org).

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)

License
---------------------
Distributed under the [MIT software license](/COPYING).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
