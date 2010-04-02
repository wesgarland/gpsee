#!/bin/bash
#
# Fresh checker-outter for gpsee
#  Right now, for MacOSX
#
#
# TODO: check for wget (and use curl -o otherwise)
# TODO: laundry list of checks for autocon
# TODO: get nspr 'correctly'
#
#

# Bash trickery.  Hard stop if any error.
#
#
set -e

HG_DIR=gpsee-nickg
LIBFFI="libffi-3.0.8"
LIBFFI_TAR="${LIBFFI}.tar.gz"
LIBFFI_DIR="`pwd`/${LIBFFI}"

MOZJS="libmozjs-1.9.3-1.3"
MOZJS_TAR="${MOZJS}.tar.bz2"
MOZJS_DIR="`pwd`/${MOZJS}"

if [ ! -f "${LIBFFI_TAR}" ]; then
   wget ftp://sources.redhat.com/pub/libffi/${LIBFFI_TAR}
fi

if [ ! -d "${LIBFFI}" ]; then
   tar -xzvf ${LIBFFI_TAR}
fi

if [ ! -f "${MOZJS_TAR}" ]; then
    wget http://packaging-spidermonkey.googlecode.com/files/${MOZJS_TAR}
fi

if [ ! -d "${MOZJS}" ]; then
    tar -xjvf ${MOZJS_TAR}
fi

echo "Asking for root password to remove /opt/local/gpsee"
sudo rm -rf /opt/local/gpsee

#rm -rf gpsee-nickg
#hg clone https://gpsee.googlecode.com/hg/ gpsee
#hg clone https://nickgsuperstar-libcurl.googlecode.com/hg ${HG_DIR}
cd ${HG_DIR}
hg pull && hg update

# Darwin only hack
#
echo "CPPFLAGS += -D_XOPEN_SOURCE=600 -D_DARWIN_C_SOURCE" >> build.mk
#
# MAIN CONFIG
#  -- no changes
#
echo "Making main config..."
cp local_config.mk.sample local_config.mk

# SPIDERMONKEY CONFIG
#
#
echo "Making spidermonkey config in ${MOZJS} ..."
cp spidermonkey/local_config.mk.sample spidermonkey/local_config.mk
echo "SPIDERMONKEY_SRC        = ${MOZJS_DIR}" >> spidermonkey/local_config.mk
echo "BUILD                   = RELEASE" >> spidermonkey/local_config.mk
echo "AUTOCONF                = autoconf213" >> spidermonkey/local_config.mk

# LIBFFI CONFIG
#
#
echo "Making libffi config for ${LIBFFI} ..."
cp libffi/local_config.mk.sample libffi/local_config.mk
echo "LIBFFI_SRC = ${LIBFFI_DIR}" >> libffi/local_config.mk

# HERE WE GO!
#
#
cd libffi
make build
echo "May ask for password for install of libffi"
sudo make install
cd ..

#
#
#
echo "Starting libmozjs build"
cd spidermonkey
make build
echo "May ask for password for install of spidermonkey"
sudo make install
cd ..

echo "Starting GPSEE build in `pwd`"
make build
echo "May ask for password for final install of gpseey
sudo make install
