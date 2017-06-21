Before you can compile Shiboken and PySide, you'll need to install some things on your system first.

# Windows

To get started on Windows you need to install OpenSSL and cmake. OpenSSL can be found [at this site](https://slproweb.com/products/Win32OpenSSL.html). Download the latest cmake from [the downloads page](http://www.cmake.org/download/).

Alternatively, you can get all of the packages from [Chocolatey](https://chocolatey.org/). Just run 
```
choco install openssl.light && choco install cmake
```

For Qt, you will need to first find your Python version you will be using.

For Python 2.7-3.4, you will need to use the `qt-opensource-windows-x86-msvc2010-Y-Y-Y.exe`, where Y-Y-Y is the version (eg 5.5.0). You can find this in [the Qt archives](https://download.qt.io/archive/qt/)

For Python 3.5, you will need to wait until Qt v5.6 is released with a version that will support Visual Studio 2015. 

# Mac OS X

The easiest way to install the dependencies is to get a package manager called [Homebrew](http://brew.sh/)

Once you have that installed, running this command will install Shiboken and PySide's dependencies

```sh
brew install qt5 cmake libxslt libxml2
```
* Note: actually, Qt5 works with Homebrew only, because they repaired some includes which are missing.
  We could add this, too.

# Linux

## Ubuntu

You can get all needed dependencies with `apt-get`:

```sh
sudo apt-get install build-essential git cmake qt5-default libxml2 libxslt
```

Additionally, you'll need to install qt5's libraries:

```sh
sudo apt-get install qttools5-dev-tools libqt5clucene5 libqt5concurrent5 libqt5core5a libqt5dbus5 libqt5designer5 libqt5designercomponents5 libqt5feedback5 libqt5gui5 libqt5help5 libqt5multimedia5 libqt5network5 libqt5opengl5 libqt5opengl5-dev libqt5organizer5 libqt5positioning5 libqt5printsupport5 libqt5qml5 libqt5quick5 libqt5quickwidgets5 libqt5script5 libqt5scripttools5 libqt5sql5 libqt5sql5-sqlite libqt5svg5 libqt5test5 libqt5webkit5 libqt5widgets5 libqt5xml5 libqt5xmlpatterns5 libqt5xmlpatterns5-dev 
```