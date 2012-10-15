/**
 * grid_master
 * Brains and Muscle of the queue system.
 * Provides functions to start processes, abort processes or continue processes after
 * others have finished.
 *
 * Within the BOINC system, these functions are used by the assimilator.
 *
 * Note: "wu" stands for "Work Unit" which refers to a specific calculations process
 * the terms "process" and "work unit" are interchangeable here. Work unit is the
 * term used within the BOINC system and it has bled over here.
 *
 * Created by David Coss, 2010
 */

#ifndef GRID_MASTER_H
#define GRID_MASTER_H

#include <iostream>
#include <string>
#include <vector>


#include "db_config.h"


struct grid_master_arguments {
	bool simulate_spawn;
	int grid_master_verbosity;
	std::string command,parameter;/* command for grid_master to perform. */
	int processID;/* ID corresponding to the process/job being managed. Probably equivalent to parameter. */
  bool initialization;/* If this is true and manage_process is called, sibling processes will be started too (if possible). */
};

/**
 * Main function to preform grid management.
 * This takes command line arguments and performs tasks
 * within the grid system.
*/
int grid_master_worker(struct grid_master_arguments& args);

/**
 * Based on the provided command, the process is handled by a specific function in
 * grid_master, e.g. continue_wu or close_wu.
 */
int do_stuff_with_process(struct grid_master_arguments& args);

/**
 * Looks up the state of the process and decides who should happen to it,
 * including starting the next process if successful or aborting the whole
 * batch upon error.
 */
int manage_process(MYSQL_RES *process,struct grid_master_arguments& args);

/**
 * Starts the next process following the process provided.
 * Throws a GridException when invalided data is provided
 * by the MYSQL_ROW.
 */
int continue_process(MYSQL_ROW process_row, struct grid_master_arguments& args) throw (GridException);

/**
 * Parses input and output file lists and creates a work unit.
 * Within the BOINC system, this is the function that submits
 * the workunit via spawn_wu.
 */
int generate_wu(MYSQL_ROW processRow,struct grid_master_arguments& args);

/**
 * Checks to see if all siblings have finished.
 * If they all are finished, true is returned.
 * Otherwise, false is returned and waiting processes are started.
 */
bool check_and_start_siblings(MYSQL_ROW process_row, struct grid_master_arguments& args) throw (GridException);

/**
 * close_wu removes a (whole) batch from the grid queue system. All raw data files that were
 * produced as output and retained by the server as archived in a tar-gzip file called
 * raw_data_<batchnumber>.tgz
 */
int close_wu(MYSQL_RES * processRow);

/**
 * Prints a list of all of the processes listed under run id (aka jobid)
 * in the queue system. Here, "prints" means information is sent to standard output.
 */
int list_processes(const char *where_clause,bool verbose = true);
int list_processes(const int& jobid);

/**
 * Prints a list of all of the processes listed under the user's id number.
 * in the queue system. Here, "prints" means information is sent to standard output.
 */
int list_all_processes(const char *where_clause = NULL);

/**
 * Creates a list of files used by a specific process in the queue system.
 * The key of the map is the file name that will be used by the application
 * on a grid node, a.k.a. client, and the value of each key is the file name
 * within the user directory.
 *
 * Why there are two different names:
 * A user will submit a job based on data produced, probably, outside of the grid.
 * The queue system and grid nodes cannot assume that filenames will
 * be unique. Therefore, file name collisions are possible. To avoid name collisions
 * file names are given a prefix with the process id and unix time stamp when the
 * proces was submitted. The object of the map retains the name specified by the user
 * so that once the process has finished, only user specified files remain.
 */
std::vector<grid_file_info> get_process_files(const std::string& processID);


/**
 * Provides a list of input files. For a description of the naming scheme and the use
 * of std::map, see get_process_files.
 */
std::vector<grid_file_info> get_input_files(const std::string& processID) throw (GridException);

/**
 * Reads a file prefix and returns the process id. For a description of the naming scheme,
 * see get_process_files.
 */
int parseID(const std::string&);

/**
 * Obtains process information based on the provided ID number. For a list of the fields,
 * see also db_config.h
 */
MYSQL_RES * get_process(const int& process_id);

/**
 * Starts the process.
 *
 * Within the BOINC system, this function sends the process information, including files, to the server.
 *
 * Outside of the BOINC system, server specific submission processes should be placed here.
 *
 */
int spawn_wu(const std::vector<grid_file_info>& infiles,const std::vector<grid_file_info>& outfiles, const std::string& appname, const std::string& appid,const std::string& jobid,const char* gflop_count_str, const struct grid_master_arguments& args);

/**
 * Determins is the MYSQL_RES contains valid data for a process.
 */
int validate_process_result(MYSQL_RES* process_result);

/**
 * Sends process information to standard output.
 */
int stdout_process_status(const int& process);

/**
 * Looks up the users UID and tests whether or not it matches the UID of the person who submitted
 * the job. If it does not, false is returned. Otherwise, true is returned. If no UID is supplied,
 * i.e. null is given to the function, an exception is thrown; it is preferred to have the user complain
 * about this (which is a syntax error in this program) rather than inadvertently granting more
 * permission than should be given to the user.
 */
bool has_permission(const char* uid);

/**
 * Recombines the results of multiple processes. 
 * The grid_master_arguments structure needs to contain the process id of the recombination process.
 * Then this process id will be used to lookup files (in process_input_files) that will be recombined.
 *
 * process contains the process query for the process id stored in args. This can be used to verify
 * permission for the user to perform the recombination.
 */
int recombine_processes(MYSQL_RES* process, struct grid_master_arguments& args);
int recombine_processes(MYSQL_ROW process_row, struct grid_master_arguments& args);

/**
 * Updates job status in the grid queue. Status values may be found in db_config.h within
 * the queue_states namespace.
 */
void updateGridStatus(const int& processID,const int& canonical_resultid);

/**
 * Strips a file of the name mangling system used in the queue system to guarantee uniqueness
 * of file names, cf get_process_files(const std::string& processID)
 */
std::string getFinalOutputName(const std::string& original);

/**
 * Takes a list of process files. Finds the queue file and produces a list of
 * output files.
 */
std::vector<grid_file_info> get_output_files(const std::string& processID) throw (GridException);

/**
 * Queries the queue system and (very roughly) estimates the cpu
 * time of the batch of jobs with the provided id.
 * NOTE: this is CPU time not wall clock time.
 */
std::string estimate_total_time(const std::string& runid);

int start_next_generation(MYSQL_ROW process_row, struct grid_master_arguments& args) throw (GridException);


int next_process();
MYSQL_RES* finished_jobs(bool verbose = false);

std::string str_int_to_hex(std::string int_string);

std::string str_hex_to_int(std::string int_string);

/**
 * Patches together the processes of submitting a job to the queue and then starting the first task.
 *
 * Returns
 * Zero upon Success
 * Error value upon failure.
 */
int submit_and_start(struct submit_job_arguments& sj_args, struct grid_master_arguments& gm_args);

#endif /* GRID_MASTER_H */
