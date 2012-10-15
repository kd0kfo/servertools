bin_PROGRAMS = validate_mmpbsa grid_master submit_job validate_moledyn merge_mdouts merge_mmpbsa gsub portal portal_client mol2_name_corrector

lib_LIBRARIES = libgrid_tools.a

validate_mmpbsa_SOURCES = validate_mmpbsa.cpp
validate_mmpbsa_CPPFLAGS = -DSTANDALONE $(MMPBSA_CPPFLAGS) $(BOINC_CPPFLAGS)
validate_mmpbsa_LDFLAGS = $(MMPBSA_LDFLAGS) $(BOINC_LDFLAGS)
validate_mmpbsa_LDADD = $(MMPBSA_LDADD) $(BOINC_LIBS) $(MYSQL_LIBS)

libgrid_tools_a_SOURCES = grid_master.cpp combine_mmpbsa_data.cpp combine_sander_data.cpp submit_job.cpp MDParams.cpp db_config.cpp GridException.cpp
libgrid_tools_a_CPPFLAGS = $(MMPBSA_CPPFLAGS) $(BOINC_CPPFLAGS) $(MYSQL_CFLAGS)

submit_job_SOURCES = submit_job_main.cpp 
submit_job_CPPFLAGS = $(MMPBSA_CPPFLAGS) $(BOINC_CPPFLAGS) $(MYSQL_CFLAGS)
submit_job_LDFLAGS = $(MMPBSA_LDFLAGS) $(BOINC_LDFLAGS) 
submit_job_LDADD =  -L. -lgrid_tools $(MMPBSA_LDADD) $(BOINC_LIBS) $(MYSQL_LIBS)
submit_job_DEPENDENCIES = libgrid_tools.a

grid_master_SOURCES = grid_master_main.cpp
grid_master_CPPFLAGS = $(MMPBSA_CPPFLAGS) $(BOINC_CPPFLAGS) $(MYSQL_CFLAGS)
grid_master_LDFLAGS = $(MMPBSA_LDFLAGS) $(BOINC_LDFLAGS) $(MYSQL_LIBS)
grid_master_LDADD =  -L. -lgrid_tools $(MMPBSA_LDADD) $(BOINC_LIBS) $(MYSQL_LIBS)
grid_master_DEPENDENCIES = libgrid_tools.a

gsub_SOURCES = gsub.cpp
gsub_CPPFLAGS = $(MMPBSA_CPPFLAGS) $(BOINC_CPPFLAGS) $(MYSQL_CFLAGS)
gsub_LDFLAGS = $(grid_master_LDFLAGS)
gsub_LDADD =  -L. -lgrid_tools $(MMPBSA_LDADD) $(BOINC_LIBS) $(MYSQL_LIBS)
gsub_DEPENDENCIES = libgrid_tools.a

validate_moledyn_SOURCES = validate_moledyn.cpp GridException.cpp
validate_moledyn_CPPFLAGS = -DSTANDALONE $(MMPBSA_CPPFLAGS) $(BOINC_CPPFLAGS)
validate_moledyn_LDFLAGS = $(MMPBSA_LDFLAGS) $(BOINC_LDFLAGS)
validate_moledyn_LDADD = $(MMPBSA_LDADD) $(BOINC_LIBS)  $(MYSQL_LIBS)

merge_mdouts_SOURCES = combine_sander_data.cpp
merge_mdouts_CPPFLAGS = -DSTANDALONE_COMBINE_SANDER_DATA $(MMPBSA_CPPFLAGS)  $(BOINC_CPPFLAGS)
merge_mdouts_LDFLAGS = $(MMPBSA_LDFLAGS) $(BOINC_LDFLAGS)
merge_mdouts_LDADD = $(MMPBSA_LDADD) $(BOINC_LIBS) -lssl

merge_mmpbsa_SOURCES = combine_mmpbsa_data.cpp GridException.cpp
merge_mmpbsa_CPPFLAGS = -DSTANDALONE_COMBINE_MMPBSA $(MMPBSA_CPPFLAGS)  $(BOINC_CPPFLAGS)
merge_mmpbsa_LDFLAGS = $(MMPBSA_LDFLAGS) $(BOINC_LDFLAGS)
merge_mmpbsa_LDADD = $(MMPBSA_LDADD) $(BOINC_LIBS) -lssl

portal_SOURCES = portal.cpp
portal_CPPFLAGS = $(gsub_CPPFLAGS)
portal_LDFLAGS = $(gsub_LDFLAGS)
portal_LDADD = $(gsub_LDADD)
portal_DEPENDENCIES = libgrid_tools.a

portal_client_SOURCES = portal_client.cpp

mol2_name_corrector_SOURCES = mol2_name_corrector.cpp


if MAKE_BOINC_PROGRAMS
bin_PROGRAMS += validator locutus

validator_SOURCES = validate_util.cpp validate_util2.cpp validator.cpp validate_mdmmpbsa.cpp db_config.cpp GridException.cpp 
validator_CPPFLAGS = $(MMPBSA_CPPFLAGS) $(BOINC_CPPFLAGS) $(MYSQL_CFLAGS)
validator_LDFLAGS = $(MMPBSA_LDFLAGS) $(BOINC_LDFLAGS)
validator_LDADD = $(MMPBSA_LDADD)  $(BOINC_LIBS) $(MYSQL_LIBS)

locutus_SOURCES = assimilator.cpp assimilate_mdmmpbsa.cpp validate_util.cpp
locutus_CPPFLAGS = $(MMPBSA_CPPFLAGS) $(BOINC_CPPFLAGS)  -I$(BOINC_PATH)/boinc/ -I$(BOINC_PATH)/boinc/api/ -I$(BOINC_PATH)/boinc/lib/ -I$(BOINC_PATH)/boinc/sched/ -I$(BOINC_PATH)/boinc/db/ $(MYSQL_CFLAGS)
locutus_LDFLAGS = $(MMPBSA_LDFLAGS) $(BOINC_LDFLAGS)
locutus_LDADD = -L. -lgrid_tools $(MMPBSA_LDADD) $(BOINC_LIBS) $(MYSQL_LIBS)
locutus_DEPENDENCIES = libgrid_tools.a
endif