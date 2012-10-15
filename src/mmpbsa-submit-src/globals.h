#ifndef GLOBALS_H
#define GLOBALS_H

//File system defines
#define GRID_USERS_GID 76988
#define INTERMEDIATE_DIRECTORY "grid_temp_files"
#define ERROR_STAT_DIR "bad_results/"///<Boinc project subdirectory where bad result files are sent.
#define DEFAULT_RESULT_DIR "sample_results/"///<When you don't know where to put the result.
#define DATA_FILE_PREFIX "raw_data_"///<File name prefix for the tarball containing the temporary files.
#define MAX_GRID_FILE_SIZE 20///< in MB, largest file to be transfered TO the grid
#define MTRAJ_PATH "/opt/mmpbsa/bin/mtraj"

//Name mangling defines
#define FIELD_DELIMITER '_'///<delimiter for fields in work unit names

// Grid Parameters
#define GRID_MAX_TOTAL_RESULTS 8
#define GRID_MAX_SUCCESS_RESULTS 8
#define GRID_MAX_ERROR_RESULTS 5
#define GRID_RESULT_QUORUM 2

//MD Run defines
#define MD_MAX_GFLOP 14000///<Maximum number of float operations (*10^9) for the molecular dynamics
#define DEFAULT_GFLOP_COUNT 1.1e4///<default Flop count provided to boinc for a particular work unit. If possible, this is replaced by a per workunit calculated value. 1 hour on a 3.25GFlops machine

// MMPBSA Run defines
#define MMPBSA_GFLOPS_PER_SNAPSHOT 50///<Number of GFlops per snapshot of mmpbsa based on statistics from grid.
#define MMPBSA_MAX_SNAPSHOTS 10///< Maximum number of snapshots allowed to run on the host

//Verbosity levels
#define MEDIUM_VERBOSITY 3
#define FREQUENT_CALLS 6
#define WICKED_VERBOSE 10

#endif //GLOBALS_H
