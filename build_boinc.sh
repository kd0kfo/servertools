#!/bin/sh
set -e
if [[ -z $1 ]];then
	echo "Missing boinc source directory." >&2
	exit 1
fi

cd $1

BOINC_TARGET_DIR=$PWD
./_autosetup
./configure --enable-libraries --disable-client --enable-server --enable-install-headers --prefix=${BOINC_TARGET_DIR}/INSTALLDIR --enable-shared

make

make install
