
Debian
====================
This directory contains files used to package rapidsd/rapids-qt
for Debian-based Linux systems. If you compile rapidsd/rapids-qt yourself, there are some useful files here.

## rapids: URI support ##


rapids-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install rapids-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your rapidsqt binary to `/usr/bin`
and the `../../share/pixmaps/rapids128.png` to `/usr/share/pixmaps`

rapids-qt.protocol (KDE)

