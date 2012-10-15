bin_PROGRAMS = gsub gmod validator assimilator unittest

gsub_SOURCES = gsub.cpp
gsub_CPPFLAGS = $(PYTHON_CFLAGS)
gsub_LDADD = $(PYTHON_LDFLAGS)

gmod_SOURCES = gmod.cpp
gmod_CPPFLAGS = $(PYTHON_CFLAGS)
gmod_LDADD = $(PYTHON_LDFLAGS)

validator_SOURCES = validate_util.cpp validate_util2.cpp validator.cpp pyvalidator.cpp pyboinc.cpp
validator_CPPFLAGS = $(BOINC_CPPFLAGS) $(MYSQL_CFLAGS) $(PYTHON_CFLAGS)
validator_LDFLAGS = $(BOINC_LDFLAGS) 
validator_LDADD = $(BOINC_LIBS) $(MYSQL_LIBS) $(PYTHON_LDFLAGS)

assimilator_SOURCES = validate_util.cpp assimilator.cpp pyassimilator.cpp pyboinc.cpp
assimilator_CPPFLAGS = $(BOINC_CPPFLAGS) $(MYSQL_CFLAGS) $(PYTHON_CFLAGS)
assimilator_LDFLAGS = $(BOINC_LDFLAGS) 
assimilator_LDADD = $(BOINC_LIBS) $(MYSQL_LIBS) $(PYTHON_LDFLAGS)


unittest_SOURCES = pyboinc.cpp pyassimilator.cpp pyvalidator.cpp validate_util.cpp unittest.cpp
unittest_CPPFLAGS = $(BOINC_CPPFLAGS) $(MYSQL_CFLAGS) $(PYTHON_CFLAGS)
unittest_LDFLAGS = $(BOINC_LDFLAGS) 
unittest_LDADD = $(BOINC_LIBS) $(MYSQL_LIBS) $(PYTHON_LDFLAGS)
