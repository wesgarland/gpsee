Building GPSEE is relatively straightforward on most platform. GPSEE is known to run on various versions of Mac OS X, Solaris and Linux, and has been tested with x86, x86\_64, Ultrasparc and ARM CPUs.  GPSEE's build system knows how to build SpiderMonkey, LibFFI, GPSEE, the included modules, and how to install it all.

# General Prerequisites #
<ul>
<li>Solaris 10+, Recent Linux, or MacOS/X 10.5+</li>
<ul>
<li>Any UNIX(tm) conforming to SUSv3 should work</li>
<li>BSD UNIX is also expected to work</li>
</ul>
<li>GCC 4.2+ (GPSEE works fine with 3.4, but recent Spidermonkey does not)</li>
<li>If using Spidermonkey (Tracemonkey) 21e90d198613 you cannot use g++ newer than 4.6</li>
<li>GNU Make 3.81</li>
<li>SpiderMonkey prerequisites for multi-threaded build (we've listed the current ones below, but Mozilla could change them in the future)</li>
<ul>
<li>autoconf-2.13  (<i>exactly</i> 2.13)</li>
<li>NSPR 4.7 or better</li>
<li><a href='https://developer.mozilla.org/en/Mozilla_Build_FAQ'>https://developer.mozilla.org/en/Mozilla_Build_FAQ</a> for more info</li>
</ul>
</ul>

# Suggested Distro Packages #
## Solaris 10 (u8 or better) with OpenCSW ##
  * Install OpenCSW - see http://get.opencsw.org/now
  * pkgutil -i mercurial pkgconfig zip perl ncurses5 libnspr4 nspr-dev CSWgcc4 CSWgcc4g++ CSWcoreutils CSWbinutils CSWgmake CSWautoconf2-13
  * pkgrm CSWgcc4 CSWgcc4g++
  * Use http://mirror.opencsw.org/opencsw/allpkgs/ to download gcc and g++ 4.6.3 for your platform, e.g. gcc4core-4.6.3,REV=2012.03.05-SunOS5.10-sparc-CSW.pkg and gcc4g++-4.6.3,REV=2012.03.05-SunOS5.10-sparc-CSW.pkg
  * pkgutil -i CSWcas-texinfo CSWlib-gnu-awt-xlib12 CSWlibffi4 CSWlibgcj12 CSWlibgcj-tools12 CSWlibgij12 libcloog\_isl2 libppl-c4 libmpc2 CSWlibplds4 CSWlibplc4

## Mac OS/X 10.5 ##
  * http://mercurial.berkwood.com/binaries/Mercurial-1.6-py2.5-macosx10.5.zip

## Mac OS/X 10.6 ##
  * http://mercurial.berkwood.com/binaries/Mercurial-1.6-py2.6-macosx10.6.zip

## Mac OS X 10.9 (Mavericks) with HomeBrew ##
  * Install homebrew: ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
  * brew install gcc46 nspr autoconf213 autoconf pkg-config

## Mac Ports ##
  * sudo port install autoconf213
  * sudo port install nspr
  * **Note:** Using Mac Ports GCC is not supported; use Apple's compiler for a pain-free build for Mac OS X <= 10.8

## Fedora Core 10 ##
  * sudo yum install -y autoconf213

## Ubuntu 9.10 ##
  * sudo apt-get install mercurial autoconf2.13 libnspr4-dev libncurses5-dev libdb-dev build-essential

## Debian, Raspbian, etc. ##
  * apt-get install mercurial autoconf2.13 libnspr4-dev libncurses5-dev libdb-dev build-essential
  * (optional, for cURL module) apt-get install libcurl5-{nss, gnutls, or openssl}-dev

# Known Issues #
## Mac OS/X 10.9 (Mavericks) ##
  * Apple dropped their version of GCC and replaced it with clang.  While clang is a fine compiler, the older versions of Mozilla (Tracemonkey-era) will not build with it.  We strongly recommend building using HomeBrew (http://brew.sh/) for building these versions of JSAPI.  Additionally, GPSEE relies on some GCC extensions for things like atomic compare-and-swap and system header parsing which might not work under clang (it has not been tested).

## Sparc / Solaris ##
  * There is a pointer aliasing bug with GCC 4.3 as shipped with Blastwave on Sparc platforms which is triggered by modern SpiderMonkey as soon as the interpreter starts. It is not known if this problem appears in other distributions. There is no easy to disable this in a non-debug build; you need to modify SpiderMonkey's autoconf .in files to suppress.

  * If you compile with GCCFSS (GCC for Sparc Systems), SpiderMonkey's tracing JIT will emit invalid JIT fragment prologues, causing crashes when entering the JIT.

  * Sun ships GCC 3.4 in /usr/sfw which might be on your path before Blastwave. To force the Blastwave package, configure with <tt>--gcc-prefix=/opt/csw/gcc4</tt>.

## OpenSolaris (SunOS 5.11) ##
  * SUNWpr package has invalid versioning, confusing the SpiderMonkey build system into thinking that it is not new enough. Locating an NSPR which is newer than version 4.7 and pointing configure at it should work.

# New-style build (configure script) #
## Get the source code (pick A or B) ##
### A) Release tar ball ###
  * wget http://www.page.ca/~wes/gpsee-latest.tar.gz
  * tar -zxvf gpsee-latest.tar.gz
  * cd gpsee-_version_

### B) Source repository ###
  * mkdir hg
  * cd hg
  * hg clone https://gpsee.googlecode.com/hg/ gpsee
  * hg clone http://hg.mozilla.org/tracemonkey
_* # optional
  * cd tracemonkey
  * hg update `````cat ../gpsee/spidermonkey/suggested-spidermonkey-revision`````
  * cd .._

## Configure GPSEE ##
  * cd gpsee
  * ./configure --help
  * ./configure _options_

Configuring GPSEE will also configure SpiderMonkey and LibFFI.  Since about version 1.8.1, SpiderMonkey ships with LibFFI; GPSEE build will automatically use that version unless you tell it otherwise.

### Mac OS/X Maverics with Homebrew ###
  * Patch tracemonkey with [this patch](ApplePatch21e90d198613.md) if you are using revision 21e90d198613 so it that will build with non-Apple GCC.
  * ./configure --with-cxx=/usr/local/bin/g++-4.6 --with-cc=/usr/local/bin/gcc-4.6

### Solaris with OpenCSW ###
  * LD\_LIBRARY\_PATH= ./configure --with-jsapi-ldflags="-R/opt/csw/lib -L/opt/csw/lib" --with-ldflags="-R/opt/csw/lib -L/opt/csw/lib" --with-autoconf213=/opt/csw/bin/autoconf-2.13 --with-cc=/opt/csw/gcc4/bin/gcc --with-cxx=/opt/csw/gcc4/bin/g++

## Build GPSEE ##
  * make build

## Install GPSEE ##
  * sudo make install

If your platform does not have sudo, and you want to install GPSEE somewhere that requires root permissions, I recommend creating the prefix directory as root, and chowning it so that your user id can write to do. Then run a plain make install as your user -- not root.

**Note:** If your platform does not have sudo, we suggest creating the installation directory (e.g. /opt/gpsee) ahead of time as root, and chowning it so that you can write to it as your regular user. You will also need to link [gsr](GSR.md) to /usr/bin/gsr and /usr/bin/commonjs after installation.

# Debugging Build System #

  * make build\_debug
  * make show\_modules