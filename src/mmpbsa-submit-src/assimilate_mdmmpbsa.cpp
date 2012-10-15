/**
 * Assimilator for molecular dynamics and mmpbsa data sets within the BOINC system.
 *
 * This program has no use outside of the BOINC system. It acts as an interface between the
 * BOINC server and the queue system.
 *
 * This program queries the queue system database (c.f. db_config.h) to obtain a list of
 * process files. If a canonical result was found by the validator, those files are copied
 * to the directory in which the user submitted the job. If no directory is specified, files
 * are copied to the "sample_results" directory of the BOINC project. If no canonical result
 * has been found, all of the files are copied to the "bad_results" directory of the BOINC
 * project.
 *
 * With the BOINC system, file are given a prefix to guarantee that file names are unique,
 * as that cannot be assured based on the filenames submitted by users. Assimilator strips
 * those prefixes. The format of the prefix is a follows:
 *
 * <job number>_<unixtime>
 *
 * The job number is a unique integer that identifies the process in the queue system. Unixtime
 * is a unix time stamp that guarantees uniqueness in addition to provided the time when the
 * process was submitted.
 *
 *  Created by: David Coss, 2010
 */
#include "db_config.h"

#include "boinc/error_numbers.h"
#include "boinc/boinc_db.h"
#include "boinc/filesys.h"
#include "boinc/str_util.h"

#include "grid_master.h"
#include "validate_util.h"
#include "globals.h"

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>


/**
 * Indicators of the state of assimilator process which are passed along to the BOINC server.
 */
enum AssimulationStates{
  SUCCESS = 0,
  NO_SUCH_PROCESS = 1,
  NO_PROCESS_PROVIDED,
  DB_CONNECTION_ERROR
};


extern std::string get_iso8601_time();


/**
 * Continues the next process(es), if there is(are) any.
 */
void continueProcesses(const int& processID)
{
  int retval;
  //let grid_master update child processes if there are any.
  struct grid_master_arguments args;
  args.command = "process";
  args.processID = processID;
  args.grid_master_verbosity = 0;
  args.simulate_spawn = false;
  retval = do_stuff_with_process(args);
  if(retval != 0)
    std::cout << "assimilator: continueProcesses: " << error_msg(retval) << std::endl;
}

/**
 * Abort the process. This command (contained in grid_master) will clear out
 * all processes depending on processID as well.
 */
void abortProcesses(const int& processID)
{
  int retval;
  struct grid_master_arguments args;
  args.command = "abort";
  args.processID = processID;
  args.grid_master_verbosity = 0;
  args.simulate_spawn = false;
  retval = do_stuff_with_process(args);
  if(retval != 0)
    std::cout << "assimilator: abortProcesses: " << error_msg(retval) << std::endl;
}

/**
 * Obtains the queue system ID number from the file/job prefix
 * contain in job_name.
 */
int getProcessID(const std::string& job_name, int& processID, std::string* rtnStringID)
{

  using std::string;
  using std::istringstream;
  string name = job_name;
  size_t fieldDelim = name.find(FIELD_DELIMITER);
  name.erase(0,fieldDelim+1);//removed app name
  fieldDelim = name.find(FIELD_DELIMITER);
  if(name.find(FIELD_DELIMITER) == string::npos)
    return NO_PROCESS_PROVIDED;//no process id found

  string strProcessID = name.substr(0,fieldDelim);
  istringstream is(strProcessID);
  is >> std::hex >> processID;
  if(is.fail())
    return NO_PROCESS_PROVIDED;//no process id found

  if(rtnStringID != 0)
    *rtnStringID = strProcessID;

  return SUCCESS;
}

/**
 * Queries the queue database and builds a map of file names to their
 * subdirectory within the user directory. If there file is located
 * in the user directory itself, there will be no map entry for that
 * file name.
 */
std::map<std::string,std::string> subdirectory_map(const int& process_id)
{
  std::map<std::string,std::string> returnMe;
  std::ostringstream sql;
  sql << "select file_name,file_name_alias,subdirectory from process_input_files where process_ID = " << process_id << ";";
  MYSQL_RES* files = query_db(sql);
  if(files == 0 || mysql_num_rows(files) == 0)
    return returnMe;

  MYSQL_ROW row;
  while((row = mysql_fetch_row(files)) != 0)
    {
      if(row[0] == 0 || row[2] == 0)
	continue;
      returnMe[row[0]] = row[2];
    }

  mysql_free_result(files);

  return returnMe;
}

/**
 * Assimilates the files. Files are copied to the path specified by prefix.
 * If the queue database lists a subdirectory, that is used within the
 * path specified by prefix.
 */
int dealWithFiles(RESULT& result,const std::string& prefix, const std::string& suffix)
{
  std::vector<FILE_INFO>::iterator file_it;
  std::vector<FILE_INFO> files;
  //std::map<std::string,std::string> filename_to_subdir;
  std::vector<grid_file_info> file_infos;
  std::vector<grid_file_info>::const_iterator file_info;
  int retval = 0;
  int fileCounter,numFiles;
  std::string newFileName;

  get_output_file_infos(result,files);
  fileCounter = 0;
  numFiles = files.size();
  if(numFiles == 0)
    return retval;

  //get a map of filenames to subdirectories within the directory
  //represented by prefix
  int process_id;
  std::string process_id_str;
  if(getProcessID(result.name,process_id,&process_id_str) == SUCCESS)
    {
      file_infos = get_process_files(str_hex_to_int(process_id_str));
    }

	

  std::string cleanFileName;
  for(file_it = files.begin();file_it != files.end();file_it++)
    {
      bool use_subdir;
      newFileName = file_it->path;
      for(size_t i = 0;i<4;i++)
	if(newFileName.find('_') != std::string::npos)
	  newFileName.erase(0,newFileName.find('_') + 1);

      for(file_info = file_infos.begin();file_info != file_infos.end();file_info++)
	{
	  if(file_info->filename == newFileName)
	    {
	      if(file_info->filename_alias.size() > 0)
		newFileName = file_info->filename_alias;
	      if(file_info->subdirectory.size() > 0)
		{
		  if(file_info->subdirectory.at(file_info->subdirectory.size() - 1) != '/')
		    newFileName = "/" + newFileName;
		  newFileName = file_info->subdirectory + newFileName;
		}
		    
	      newFileName = prefix + newFileName + suffix;
	      std::cout << "copying " << file_it->path << " to " << newFileName << std::endl;
	      retval = boinc_copy(file_it->path.c_str(),newFileName.c_str());
	      chown(newFileName.c_str(),-1,GRID_USERS_GID);
	      std::string scrub_command = "dos2unix ";
	      scrub_command += newFileName;
	      if(system(scrub_command.c_str()))
		std::cerr << "WARNING - " << get_iso8601_time() << " - Could not dos2unix convert file: " << newFileName << std::endl;

	      if(retval)
		{
		  //If we could not copy the file, possibly due to write permissions,
		  //try to move it to a bad results directory.
		  int bad_dir_copy_retval;
		  std::cerr << "ERROR - " << get_iso8601_time() << std::endl;
		  std::cerr << "dealWithFiles: Could not copy file " << file_it->path << " to " <<newFileName << std::endl << "Reason: " << boincerror(retval) << std::endl;
		  BoincDBConn boinc_info = getBoincDBConn();
		  newFileName = boinc_info.BOINC_CONFIG_FILE_PATH;
		  newFileName += ERROR_STAT_DIR + file_it->name;
		  bad_dir_copy_retval = boinc_copy(file_it->path.c_str(),newFileName.c_str());
		  chown(newFileName.c_str(),-1,GRID_USERS_GID);
		  if(bad_dir_copy_retval == 0)
		    std::cerr << "dealWithFiles: Copied file to " << ERROR_STAT_DIR << ". Contact grid admin to retrieve file."<< std::endl;
		  else
		    std::cerr << "File could not be saved." << std::endl;
		}
	      else
		chmod(newFileName.c_str(),0664);
	    }
	}
    }
  return 0;
}

/**
 * BOINC assimilation hook.
 *
 * The BOINC assimilation program links to this function, which contains the
 * necessary calls to the functions within this file to assimilate the files.
 * An AssimilationState is returned to BOINC.
 */
int assimilate_handler(WORKUNIT& wu, std::vector<RESULT>& results, RESULT& canonical_result)
{

  int processID,retval;

  std::string job_name = wu.name;
  if(queue_conn == 0 && !connect_db())
    {
      std::cerr << "ERROR - " << get_iso8601_time() << std::endl;

      std::cerr << "Could not connect to process DB." << std::endl;
      std::cerr << "Reason: ";
      if(queue_conn == 0)
	std::cerr << "No database object." << std::endl;
      else
	std::cerr << "MySQL Error(" << mysql_errno(queue_conn) << "): " << mysql_error(queue_conn) << std::endl;

      return DB_CONNECTION_ERROR;
    }
  retval = getProcessID(job_name,processID,0);

  switch(retval == 0)
    {
    case DB_CONNECTION_ERROR:
      {
	//not being able to connect to the queue system data base should cause it to end
	//and be investigated. Error message is sent to cerr above.
	return retval;
      }
    default:
      break;
    }

  std::string outputFilePrefix = DEFAULT_RESULT_DIR;
  std::ostringstream sql;
  sql << "select user_directory from processes where ID = " << processID;
  MYSQL_RES* prefix = query_db(sql);

  if(prefix != 0)
    {
      MYSQL_ROW row = mysql_fetch_row(prefix);
      if(row != 0 && row[0] != 0)
	outputFilePrefix = row[0];
      if(outputFilePrefix.at(outputFilePrefix.size()-1) != '/')
	outputFilePrefix += "/";
      mysql_free_result(prefix);
    }

  if(outputFilePrefix.at(outputFilePrefix.size()-1) != '/')
    outputFilePrefix += "/";
  updateGridStatus(processID,wu.canonical_resultid);
  if(wu.canonical_resultid != 0)
    {
      retval = dealWithFiles(canonical_result,outputFilePrefix,"");
      continueProcesses(processID);
    }
  else
    {
      retval = dealWithFiles(results.at(0),outputFilePrefix,".ERROR");
      abortProcesses(processID);
    }
  return SUCCESS;
}



