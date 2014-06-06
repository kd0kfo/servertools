#!/bin/bash

set -e

yum install -y python python-devel python-setuptools gcc gcc-c++ make autoconf git libtool mysql mysql-devel libcurl-devel openssl-devel

echo Cloning Code
if [[ ! -d repo ]];then
	git clone /repo
	cd repo
else
	cd repo
	git pull origin master
fi

echo Building Boinc
if [[ ! -f ltmain.sh ]];then
	cp /usr/share/libtool/config/ltmain.sh .
fi
REVISION=master
if [[ -f /vagrant/revision ]];then
	read REVISION < /vagrant/revision
fi
if [[ ! -d boinc ]];then
	if [[ -d /vagrant/boinc ]];then
		git clone /vagrant/boinc
	else
		git clone https://github.com/kd0kfo/boinc.git
	fi
	cd boinc
	git checkout $REVISION
	cd ..
fi
sh ./build_boinc.sh $PWD/boinc
cd ..

echo Build DAG
if [[ ! -d dag ]];then
	git clone https://github.com/kd0kfo/dag.git
	cd dag
else
	cd dag
	git pull origin master
fi
python setup.py install
cd ..

echo Building Code
cd repo
./setup
./configure --with-boinc=$PWD/boinc/INSTALLDIR
make
make install
cd test
make test
cd ..
cd python
python setup.py build
python setup.py install

