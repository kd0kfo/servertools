#!/bin/sh

epydoc -o python_api --name "BOINC Server Tools" $@ ../python/boinctools
doxygen
