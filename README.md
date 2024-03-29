![Seriesapp Logo](res/icons/asteroid_64.png)
Series-app
==========
For your series
---------------

2011-2022 Gideon van der Kolf, noedigcode@gmail.com

Series-app is a small application that retrieves series and episode info from
epguides.com.

![Screenshot](res/screenshot.png)

Episode dates are shown when hovering the mouse over an episode name and
yet unreleased episodes are marked grey.

The series and episode lists are cached so you don't have to re-download every time.

Double-clicking on an episode name copies the name and number to the clipboard,
useful for old school manual renaming.

Series can be starred for quick access.

Series-app is written in C++ using Qt5 and runs on Linux and Windows (and probably Mac OS too).

More of the same information is available at www.noedig.co.za/seriesapp/


Building:
---------

The app can easily be built by opening the .pro file with QtCreator and building.

To build from the Linux command line, run the following from the source code directory:
```
mkdir build
cd build
qmake ../seriesapp.pro
make
```
A `seriesapp` binary will be created.

