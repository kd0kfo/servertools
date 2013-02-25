bin_PROGRAMS =  unittest

unittest_SOURCES = ../src/pyboinc.cpp ../src/pyassimilator.cpp ../src/pyvalidator.cpp ../src/validate_util.cpp unittest.cpp
unittest_CPPFLAGS = -I ../src $(BOINC_CPPFLAGS) $(MYSQL_CFLAGS) $(PYTHON_CFLAGS)
unittest_LDFLAGS = $(BOINC_LDFLAGS) 
unittest_LDADD = $(BOINC_LIBS) $(MYSQL_LIBS) $(PYTHON_LDFLAGS)

test: unittest
	echo "project_path = '"${PWD}"'" > local_boinc_settings.py
	PYTHONPATH=${PWD}/../python:${PWD} ./unittest