#ifndef SUBMIT_JOB_STRUCTS_H
#define SUBMIT_JOB_STRUCTS_H

#include <map>
#include <string>
#include <mysql.h>

#include "MDParams.h"

struct submit_job_arguments
{
  std::string job_file;
  int verbose_level;
  bool simulate_submission;
  int previous_process;

  // Whether nor not a graphviz graph of the command
  // graph should be created (.dot file).
  // Default: false
  bool generate_graph;
  // graph file name. Ignored if generate_graph is false
  std::string graph_filename;
  
  
  int submitter_uid;// to ignore this parameter, set equal to -1
  
  std::map<size_t,my_ulonglong> previous_processes;
  std::map<std::string,MDParams> mdin_mdcrd_map;
  int runid;
  int natoms,ifbox;
  bool extract_trajectory;
};

#endif //SUBMIT_JOB_STRUCTS_H


