#!/bin/sh
BOINC_TARGET_DIR=$1

./configure --enable-libraries --disable-client --enable-server --enable-install-headers --prefix=${BOINC_TARGET_DIR} --enable-shared

