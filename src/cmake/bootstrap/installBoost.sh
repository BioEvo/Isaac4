#!/bin/bash
################################################################################
##
## Isaac Genome Alignment Software
## Copyright (c) 2010-2017 Illumina, Inc.
## All rights reserved.
##
## This software is provided under the terms and conditions of the
## GNU GENERAL PUBLIC LICENSE Version 3
##
## You should have received a copy of the GNU GENERAL PUBLIC LICENSE Version 3
## along with this program. If not, see
## <https://github.com/illumina/licenses/>.
##
################################################################################
##
## file installBoost.sh
##
## Script to install boost
##
## author Come Raczy
##
################################################################################

REDIST_DIR=$1
INSTALL_DIR=$2
PARALLEL=$3
BOOST_CXX=$4
BOOST_CXX_FLAGS=$(echo $5 |sed 's/\([^ ]\+\)/<cxxflags>\1/g')

# common_options below wants to deal with all our unparsed arguments. Make sure parsed ones don't get in the way
shift 5

. `dirname "$0"`/common.sh

BUILD_DIR=${INSTALL_DIR}/build
BIN_DIR=${INSTALL_DIR}/bin
LIB_DIR=${INSTALL_DIR}/lib
INCLUDE_DIR=${INSTALL_DIR}/include

SCRIPT=`basename "$0"`
VERSION=`echo ${iSAAC_BOOST_VERSION} | sed "s/\./_/g"`
SOURCE_TARBALL=${REDIST_DIR}/boost_${VERSION}.tar.bz2
TARBALL_COMPRESSION=j
SOURCE_DIR=${BUILD_DIR}/boost_${VERSION}
USER_CONFIG_JAM=${SOURCE_DIR}/tools/build/user-config.jam

#for now these defines do nothing to linux but ensure that boost gets properly built on cygwin.
CYGWIN_BJAM_OPTIONS="define=_GNU_SOURCE define=BOOST_SYSTEM_API_CONFIG_HPP define=BOOST_POSIX_API"

BJAM_OPTIONS="${CYGWIN_BJAM_OPTIONS} --layout=system link=static threading=multi variant=release"
USER_CONFIG_JAM=${SOURCE_DIR}/tools/build/user-config.jam

common_options $@

if [[ $CLEAN ]] ; then
    echo removing $SOURCE_DIR
    rm -rf $SOURCE_DIR ${INCLUDE_DIR}/boost ${LIB_DIR}/libboost_*.{a,so}
    exit 0
fi

if [[ $DEBUG ]] ; then
    echo "... creating a debug-glibc version of BOOST"
    BJAM_OPTIONS="${BJAM_OPTIONS} define=_GLIBCXX_DEBUG=1"
fi


common_create_source
#NO_BZIP2 disables dependency on libbz.h which is handy in cygwin
cd ${SOURCE_DIR} \
    && ./bootstrap.sh ${BOOTSTRAP_OPTIONS} --prefix=${INSTALL_DIR} --with-libraries=`echo ${iSAAC_BOOST_COMPONENTS} | sed "s/;/,/g"` \
    && echo "using gcc : : ${BOOST_CXX} : ${BOOST_CXX_FLAGS} ;" >${USER_CONFIG_JAM} \
    && echo "modules.poke : NO_BZIP2 : 1 ;" >>${USER_CONFIG_JAM} \
    && BOOST_BUILD_USER_CONFIG=${USER_CONFIG_JAM} ./bjam -q -j$PARALLEL ${BJAM_OPTIONS} --libdir=${INSTALL_DIR}/lib install

if [ $? != 0 ] ; then echo "$SCRIPT: build failed: Terminating..." >&2 ; exit 1 ; fi


echo "boost-$VERSION installed successfully"  >&2
