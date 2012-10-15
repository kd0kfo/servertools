/**
 * submit_job
 *
 * These functions handle the process of parsing a list of commands from the user and
 * producing a series of parallel grid processes. Those processes which cannot be run
 * in parallel are segmented into smaller serial processes.
 *
 * Within the BOINC system, these functions prepare the processes to be sent to the BOINC
 * server, but starting the processes is handled by grid_master.
 *
 * Created by David Coss, 2010
 */
#ifndef SUBMIT_JOB_H
#define SUBMIT_JOB_H

#include <string>
#include <vector>

#include "db_config.h"
#include "submit_job_structs.h"

const size_t numberOfApplications = 2;

/**
 * Determines if the specified directory exists. If it does, that directory
 * name is returned. If it does it, the function attempts to create it. If it
 * can create it, that directory is returned. Else, "." is returned.
 */
std::string get_or_create_directory(const std::string& desired_directory,const struct submit_job_arguments& args);


/**
 * Sets the argument variables to default values.
 */
void default_submit_job_args(struct submit_job_arguments *args);

int submit_job_main(struct submit_job_arguments& args, std::map<std::string,grid_file_info>& file_list);

/**
 * Creates a string with current date and time using the ISO-8601 standard.
 *
 */
std::string get_iso8601_time();

#endif //SUBMIT_JOB_H
