bin_PROGRAMS = validator assimilator 

validator_SOURCES = validate_util.cpp validate_util2.cpp validator.cpp pyvalidator.cpp pyboinc.cpp
validator_CPPFLAGS = $(BOINC_CPPFLAGS) $(MYSQL_CFLAGS) $(PYTHON_CFLAGS)
validator_LDFLAGS = $(BOINC_LDFLAGS) 
validator_LDADD = $(BOINC_LIBS) $(MYSQL_LIBS) $(PYTHON_LDFLAGS)

assimilator_SOURCES = validate_util.cpp assimilator.cpp pyassimilator.cpp pyboinc.cpp
assimilator_CPPFLAGS = $(BOINC_CPPFLAGS) $(MYSQL_CFLAGS) $(PYTHON_CFLAGS)
assimilator_LDFLAGS = $(BOINC_LDFLAGS) 
assimilator_LDADD = $(BOINC_LIBS) $(MYSQL_LIBS) $(PYTHON_LDFLAGS)

