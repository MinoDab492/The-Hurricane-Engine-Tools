#!/bin/bash
#
#  SPDX-License-Identifier: GPL-3.0-or-later
#

# Halt on errors and be verbose about what we are doing
set -e
set -x

# Read in our parameters
export BUILD_PREFIX=$1
export KRITA_SOURCES=$2
export KRITA_BRANDING="${3}"

# qjsonparser, used to add metadata to the plugins needs to work in a en_US.UTF-8 environment.
# That's not always the case, so make sure it is
export LC_ALL=en_US.UTF-8
export LANG=en_us.UTF-8

# We want to use $prefix/deps/usr/ for all our dependencies
export DEPS_INSTALL_PREFIX=$BUILD_PREFIX/deps/usr
export DOWNLOADS_DIR=$BUILD_PREFIX/downloads/

# Setup variables needed to help everything find what we build
export LD_LIBRARY_PATH=$DEPS_INSTALL_PREFIX/lib:$LD_LIBRARY_PATH
export PATH=$DEPS_INSTALL_PREFIX/bin:$PATH
export PKG_CONFIG_PATH=$DEPS_INSTALL_PREFIX/share/pkgconfig:$DEPS_INSTALL_PREFIX/lib/pkgconfig:/usr/lib/pkgconfig:$PKG_CONFIG_PATH
export CMAKE_PREFIX_PATH=$DEPS_INSTALL_PREFIX:$CMAKE_PREFIX_PATH
# https://docs.python.org/3.8/using/cmdline.html#envvar-PYTHONHOME
export PYTHONPATH=$DEPS_INSTALL_PREFIX/sip
export PYTHONHOME=$DEPS_INSTALL_PREFIX

source ${KRITA_SOURCES}/packaging/linux/appimage/override_compiler.sh.inc

cd $KRITA_SOURCES

if [ -z "${KRITA_BRANDING}" ]; then
    # determine the channel for branding
    if [ "${JOB_NAME}" == "Krita_Nightly_Appimage_Build" ]; then
        KRITA_BRANDING="Next"
    elif [ "${JOB_NAME}" == "Krita_Stable_Appimage_Build" ]; then
        KRITA_BRANDING="Plus"
    else
        KRITA_BRANDING=""
    fi
fi

BUILD_TYPE="Release"

# Make sure our build directory exists
if [ ! -d $BUILD_PREFIX/krita-build/ ] ; then
    mkdir -p $BUILD_PREFIX/krita-build/
fi

# Now switch to it
cd $BUILD_PREFIX/krita-build/

# Determine how many CPUs we have
CPU_COUNT=`grep processor /proc/cpuinfo | wc -l`

# Configure Krita
cmake $KRITA_SOURCES \
    -DCMAKE_INSTALL_PREFIX:PATH=$BUILD_PREFIX/krita.appdir/usr \
    -DDEFINE_NO_DEPRECATED=1 \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DFOUNDATION_BUILD=1 \
    -DHIDE_SAFE_ASSERTS=ON \
    -DFETCH_TRANSLATIONS=ON \
    -DBUILD_TESTING=FALSE \
    -DKRITA_ENABLE_PCH=off \
    -DPYQT_SIP_DIR_OVERRIDE=$DEPS_INSTALL_PREFIX/share/sip/ \
    -DHAVE_MEMORY_LEAK_TRACKER=FALSE \
    -DBRANDING="${KRITA_BRANDING}"


# Build and Install Krita (ready for the next phase)
make -j$CPU_COUNT install

# We add Krita's AppImage location for plugins (GMic)
export PLUGINS_INSTALL_PREFIX=$BUILD_PREFIX/krita.appdir/usr

# Setup variables needed to help everything find what we build
ARCH=`dpkg --print-architecture`
export LD_LIBRARY_PATH=$PLUGINS_INSTALL_PREFIX/lib:$LD_LIBRARY_PATH
export CMAKE_PREFIX_PATH=$PLUGINS_INSTALL_PREFIX:$CMAKE_PREFIX_PATH

# Make sure our build directory exists
if [ ! -d $BUILD_PREFIX/plugins-build/ ] ; then
    mkdir -p $BUILD_PREFIX/plugins-build/
fi

# The 3rdparty dependency handling in Krita also requires the install directory to be pre-created
if [ ! -d $DOWNLOADS_DIR ] ; then
    mkdir -p $DOWNLOADS_DIR
fi

# Switch to our build directory as we're basically ready to start building...
cd $BUILD_PREFIX/plugins-build/

# Determine how many CPUs we have
CPU_COUNT=`grep processor /proc/cpuinfo | wc -l`

# Configure the dependencies for building
cmake $KRITA_SOURCES/3rdparty_plugins \
    -DCMAKE_INSTALL_PREFIX=$PLUGINS_INSTALL_PREFIX \
    -DINSTALL_ROOT=$PLUGINS_INSTALL_PREFIX \
    -DEXTERNALS_DOWNLOAD_DIR=$DOWNLOADS_DIR \
    -DSUBMAKE_JOBS=$CPU_COUNT

# Now start building everything we need, in the appropriate order
cmake --build . --target ext_gmic -- -j$CPU_COUNT
