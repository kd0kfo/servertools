#include "globals.h"
#include "GridException.h"
#include "submit_job.h"

#include "libmmpbsa/XMLParser.h"
#include "libmmpbsa/XMLNode.h"
#include "libmmpbsa/Zipper.h"
#include "libmmpbsa/mmpbsa_utils.h"

//boinc stuff
#ifdef USE_BOINC
#include "boinc/boinc_db.h"
#include "boinc/backend_lib.h"
#include "boinc/util.h"
#include "boinc/str_util.h"
#include "boinc/filesys.h"
#endif//USE_BOINC

#include "grid_master.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fstream>
#include <sstream>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#define PROCESS_THAT_CAN_BEGIN_SQL "select distinct `processes`.`ID` AS `ID` from (`processes` left join `process_input_files` on((`processes`.`ID` = `process_input_files`.`process_ID`))) where ((`processes`.`state` = 2) and (isnull(`process_input_files`.`parent`) or `process_input_files`.`parent` in (select `processes`.`ID` AS `ID` from `processes` where (`processes`.`state` = 1))) and (isnull(`processes`.`parentid`) or `processes`.`parentid` in (select `processes`.`ID` AS `ID` from `processes` where (`processes`.`state` = 1)))) order by `processes`.`ID`;"


std::string getFinalOutputName(const std::string& original)
{
  std::string returnMe = original;
  size_t delim;
  for(int i = 0;i<2;i++)
    {
      delim = returnMe.find(FIELD_DELIMITER);//cf db_config.h
      if(delim == std::string::npos)
	return returnMe;
      returnMe.erase(0,delim+1);
    }
  return returnMe;

}


int grid_master_worker(struct grid_master_arguments& args)
{
  using std::cout;
  using std::endl;
	
  int retval;
  if(!connect_db())
    {
      std::ostringstream error;
      error << "grid_master_worker: Could not connect to grid database." << std::endl;
      if(queue_conn == 0)
	error << "No database object. ";
      else
	error << "Reason: MySQL Error #" << mysql_errno(queue_conn) << ": " << mysql_error(queue_conn);
      throw GridException(error,DATABASE_ERROR);
    }

  args.processID = parseID(args.parameter);
  if(args.processID == -1 && (args.command != "list") && (args.command != "next") && (args.command != "finished") &&(args.command != "running") )
    {
      std::string time_stamp = ::get_iso8601_time();
      const char *strTime = time_stamp.c_str();
      std::cerr << "ERROR - " << strTime << " - The parameter " << args.parameter << " requires an integer" << std::endl;
      return INVALID_CLI_ARGUMENT;
    }

  try{
    retval = do_stuff_with_process(args);

    disconnect_db();

    if(retval != NO_ERROR)
      std::cerr << error_msg(retval) << std::endl;
  }
  catch(GridException ge)
    {
      std::cerr << "Error - " << get_iso8601_time() << std::endl;
      std::cerr << ge << std::endl;
      std::cerr << error_msg(ge.getErrType()) << std::endl;
      return ge.getErrType();
    }
	
  return retval;

}

int list_running()
{
  std::ostringstream condition;
  BoincDBConn boincDB = getBoincDBConn();
  condition << " state = " <<  queue_states::running;
  if(getuid() != boincDB.BOINC_DAEMON_UID)
    condition << " and uid = " << getuid();
  condition << " ";
  return list_processes(condition.str().c_str(),false);
}

int do_stuff_with_process(struct grid_master_arguments& args)
{
  MYSQL_RES* process = get_process(args.processID);
  int retval;

  if(args.command == "process" || args.command == "start")
    retval = manage_process(process,args);
  else if(args.command == "status")
    retval = stdout_process_status(args.processID);
  else if(args.command == "abort" || args.command == "close")
    retval = close_wu(process);
  else if(args.command == "running")
    retval = list_running();
  else if(args.command.find("recomb") != std::string::npos)
    retval = recombine_processes(process,args);
  else if(args.command == "list")
    retval = list_processes(args.processID);
  else if(args.command == "next")
    retval = next_process();
  else if(args.command == "finished")
    {
      mysql_free_result(finished_jobs(true));
      retval = NO_ERROR;
    }
  else
    retval = UNKNOWN_COMMAND;

  mysql_free_result(process);
  return retval;
}

int list_all_processes(const char *where_clause)
{
  std::ostringstream sql;
  BoincDBConn boincDB = getBoincDBConn();
  // query list of job id's
  sql << "select distinct runid from processes ";
  if(getuid() != boincDB.BOINC_DAEMON_UID)
    sql << " where processes.uid = " << getuid();
  sql << " order by processes.ID asc;";
	
  // Iterate through list
  MYSQL_RES* jobs = query_db(sql.str());
  if(mysql_errno(queue_conn) != 0)
    {
      mysql_free_result(jobs);
      std::cerr << "ERROR - " << get_iso8601_time() << " - [list_processes] " << mysql_error(queue_conn) << std::endl;
      return mysql_errno(queue_conn);
    }

  if(jobs == 0 || mysql_num_rows(jobs) == 0)
    {
      std::cout << "No processes running." << std::endl;
      return NO_ERROR;
    }
  MYSQL_ROW row = mysql_fetch_row(jobs);
  while(row != NULL)
    {
      if(row[0] != NULL)
	std::cout << row[0] << " ";
      row = mysql_fetch_row(jobs);
    }
  std::cout << std::endl;
  return NO_ERROR;

}

int next_process()
{
  std::ostringstream sql;
  MYSQL_RES* children = query_db(PROCESS_THAT_CAN_BEGIN_SQL);
  
  if(mysql_errno(queue_conn) != 0)
    {
      sql.str("");
      sql << "start_next_generation: Could not get child process information due to a database error" << std::endl
	  << "Reason: MYSQL_ERROR(" << mysql_errno(queue_conn) << ") " << mysql_error(queue_conn);
      if(children != 0)
	mysql_free_result(children);
      throw GridException(sql,MYSQL_ERROR);
    }

  MYSQL_ROW process_id = mysql_fetch_row(children);
  while(process_id != NULL)
    {
      std::cout << process_id[0] << " " << std::endl;
      process_id = mysql_fetch_row(children);
    }
  mysql_free_result(children);
  return NO_ERROR;
}

MYSQL_RES* finished_jobs(bool verbose)
{
  std::ostringstream sql;
  MYSQL_RES* children;

  query_db("create temporary table allbatches select distinct runid,user_directory from processes order by runid; ");
  query_db("create temporary table waitingbatches select distinct runid from processes where state in (2,3); ");
  query_db("create temporary table invalidbatches select distinct runid from processes where state in (4); ");
  children = query_db("select * from allbatches where runid not in (select runid from waitingbatches) or runid in (select runid from invalidbatches);");
  query_db("drop temporary table allbatches;");
  query_db("drop temporary table waitingbatches;");
  query_db("drop temporary table invalidbatches;");
  
  if(mysql_errno(queue_conn) != 0)
    {
      sql.str("");
      sql << "start_next_generation: Could not get child process information" << std::endl
	  << "Reason: MYSQL_ERROR(" << mysql_errno(queue_conn) << ") " << mysql_error(queue_conn);
      if(children != 0)
	mysql_free_result(children);
      throw GridException(sql,MYSQL_ERROR);
    }

  if(verbose)
    {
      MYSQL_ROW process_id = mysql_fetch_row(children);
      while(process_id != NULL)
	{
	  std::cout << process_id[0] << " " << std::endl;
	  process_id = mysql_fetch_row(children);
	}
      mysql_data_seek(children,0);
    }
  return children;
}


int list_processes(const int& jobid)
{
  std::ostringstream str;
  if(jobid < 0)
    return list_all_processes();

  str << " runid = " <<  jobid;
  return list_processes(str.str().c_str());
}

int list_processes(const char *where_clause, bool verbose)
{
  std::ostringstream sql;
  if(where_clause == NULL)
    return list_all_processes();

  sql << "select processes.ID,process_states.name,uid,runid from processes " <<
    "left join process_states on processes.state = process_states.ID ";

  sql << " where " << where_clause << " ";

  sql << " order by processes.ID asc;";
  MYSQL_RES* jobs = query_db(sql.str());
  if(mysql_errno(queue_conn) != 0)
    {
      mysql_free_result(jobs);
      std::cerr << "ERROR - " << get_iso8601_time() << " - [list_processes] " << mysql_error(queue_conn) << std::endl;
      return mysql_errno(queue_conn);
    }

  if(jobs == 0 || mysql_num_rows(jobs) == 0)
    {
      std::cout << "No such jobs. " << std::endl;
      return NO_ERROR;
    }


  MYSQL_ROW row;
  std::string batch_id = "";
  unsigned int num_finished,num_not_finished;
  num_finished = num_not_finished = 0;
  while((row = mysql_fetch_row(jobs)))
    {
      if(row != 0 && row[0] != 0 && row[1] != 0 && (row[2] == 0 || has_permission(row[2])))
	{
	  if(verbose)
	    std::cout << "Status of job #" << row[0] << ": " << row[1] << std::endl;
	  else
	    std::cout << row[0] << " ";
	  if(strcmp(row[1],"success") == 0)
	    num_finished++;
	  else
	    num_not_finished++;
	}
      if(row[3])
	batch_id = row[3];
    }

  if(batch_id.size() && verbose)
    {
      std::string time_estimate = estimate_total_time(batch_id);
      std::istringstream time_buff(time_estimate);
      float secs,mins,hrs,days;
      time_buff >> secs;
      if(!time_buff.fail())
	{
	  int isecs = (int)secs;
	  days = (int)ceil(isecs/86400);isecs -= (int)days*86400;
	  std::cout << "(Rough) Estimate of CPU time for whole batch: " << std::endl;
	  if(days)std::cout << days << "d ";
	  hrs = (int)ceil(isecs/3600);isecs -= (int)hrs*3600;
	  if(hrs)std::cout << hrs << "h ";
	  mins = (int)ceil(isecs/60);isecs -= (int)mins*60;
	  if(mins)std::cout << mins << "m ";
	  if(secs)std::cout << isecs << "s ";
	  std::cout << std::endl;
	}
      if(num_finished + num_not_finished != 0)
	{
	  printf("Percent Finished: %.01f\n",(100.0*num_finished/(num_finished+num_not_finished)));
	  printf("Finished %d out of %d\n",num_finished,(num_finished+num_not_finished));
	  fflush(stdout);
	}
    }

  if(!verbose)
    std::cout << std::endl;

  return NO_ERROR;
}

std::string estimate_total_time(const std::string& runid)
{
  std::ostringstream time_stream;
  if(queue_conn == 0)
    return "";

  time_stream << "select sum(gflop_estimate)/3 " 
	      << "+ (select count(ID) from processes where application = 3 and gflop_estimate is null and runid = " << runid << ")*900 "
	      << " + (select count(ID) from processes where application = 2 and gflop_estimate is null and runid = " << runid << ")*3600 "
	      <<"from processes where runid = " << runid << ";";
  MYSQL_RES* time_res = query_db(time_stream);
  if(mysql_errno(queue_conn))std::cout << mysql_error(queue_conn) <<std::endl;
  if(time_res == 0 || mysql_num_rows(time_res) != 1)
    return "";
  MYSQL_ROW time_row = mysql_fetch_row(time_res);
  mysql_free_result(time_res);
  return (time_row[0]) ? time_row[0] : "";
}

int stdout_process_status(const int& process)
{
  using namespace std;
  string batch_id = "";
  ostringstream bufProcess;
  bufProcess << process;
  string sql = "select processes.ID,processes.parentid,"
    "applications.name,runid,process_states.name,user_directory from processes left join applications "
    "on processes.application = applications.ID "
    "left join process_states on process_states.ID = processes.state "
    "where processes.ID = " + bufProcess.str() + ";";

  MYSQL_RES* procResult = query_db(sql);

  int retval = validate_process_result(procResult);
  if(retval)
    return retval;

  MYSQL_ROW procRow = mysql_fetch_row(procResult);

  size_t i = 0;
  cout << "ID: " << procRow[i++] << endl;
  string parentID = (procRow[i]) ? procRow[i] : "None";i++;
  cout << "Parent ID: " << parentID << endl;
  cout << "Application: " << procRow[i++] << endl;
  cout << "Batch ID: " << (batch_id = procRow[i++]) << endl;
  cout << "Status: " << procRow[i++] << endl;
  cout << "Directory: " << procRow[i++] << endl;

  vector<grid_file_info>  infiles = get_process_files(procRow[0]);
  if(infiles.size())
    {
      cout << "Files:" << std::endl;
      for(std::vector<grid_file_info>::const_iterator it = infiles.begin();
	  it != infiles.end();it++)
        {
	  cout << it->filename;
	  if(it->filename_alias != "")
	    cout << " originally named " << it->filename_alias;
	  if(it->filetype == file_types::temporary_input || it->filetype == file_types::temporary_output)
	    cout << " (a temporary file) ";
	  cout  << endl;
        }
    }

  if(batch_id.size())
    {
      string time_estimate = estimate_total_time(batch_id);
      istringstream time_buff(time_estimate);
      float secs,mins,hrs,days;
      time_buff >> secs;
      if(!time_buff.fail())
	{
	  int isecs = (int)secs;
	  days = (int)ceil(isecs/86400);isecs -= (int)days*86400;
	  std::cout << "(Rough) Estimate of CPU time for whole batch: " << std::endl;
	  if(days)std::cout << days << "d ";
	  hrs = (int)ceil(isecs/3600);isecs -= (int)hrs*3600;
	  if(hrs)std::cout << hrs << "h ";
	  mins = (int)ceil(isecs/60);isecs -= (int)mins*60;
	  if(mins)std::cout << mins << "m ";
	  if(secs)std::cout << isecs << "s ";
	  std::cout << std::endl;
	}
    }
    

  mysql_free_result(procResult);
  return 0;
}

int extract_trajectory(MYSQL_ROW process_row, struct grid_master_arguments& args)
{
  std::string extraction_command;
  
  if(!has_permission(process_row[process_fields::uid]))
    throw GridException("extract_trajectory: cannot continue process",INSUFFICIENT_PERMISSION);
  
  if(process_row[process_fields::arguments] == NULL || strlen(process_row[process_fields::arguments]) == 0)
    throw GridException("extract_trajectory: Not provided arguments with which to extract a trajectory.",INVALID_INPUT_PARAMETER);

  extraction_command = MTRAJ_PATH;
  extraction_command +=  " ";
  extraction_command += process_row[process_fields::arguments];
  std::cout << "Running: \"" << extraction_command << "\"" << std::endl;
  return system(extraction_command.c_str());
}

int clean_trajectory_file(MYSQL_ROW process_row, struct grid_master_arguments& args)
{
  if(!has_permission(process_row[process_fields::uid]))
    throw GridException("clean_trajectory_file: cannot continue process",INSUFFICIENT_PERMISSION);
  
  if(process_row[process_fields::arguments] == NULL || strlen(process_row[process_fields::arguments]) == 0)
    throw GridException("clean_trajectory_file: Not provided a temporary trajectory to clean.",INVALID_INPUT_PARAMETER);

  return remove(process_row[process_fields::arguments]);
}

int manage_process(MYSQL_RES *process, struct grid_master_arguments& args)
{
  int retval;
  bool internal_process = false;

  if(process->row_count == 0)
    return NOT_A_PROCESS;
  if(process->row_count != 1)
    return MULTIPLE_ROWS;

  using std::string;
  MYSQL_ROW process_row = mysql_fetch_row(process);

  string state = (process_row[process_fields::state]) ? process_row[process_fields::state] : "NULL";

  if(state == queue_states::waiting)
    {
      std::string strAppname = process_row[process_fields::appname];
      if(strAppname.find("recombine_") != std::string::npos)
	{
	  //rewind back to before process_row was initialized, then pass
	  //process on to recombine_processes, which wants to call mysql_fetch_row
	  //to get the process information.
	  retval = recombine_processes(process_row,args);
	  internal_process = true;
	}
      else if(strAppname == "extract_traj")
	{
	  retval = extract_trajectory(process_row,args);
	  internal_process = true;
	}
      else if(strAppname == "clean_traj")
	{
	  retval = clean_trajectory_file(process_row,args);
	  internal_process = true;
	}

      if(internal_process)
	{
	  if(retval == NO_ERROR)
	    {
	      updateGridStatus(args.processID,42);
	      args.command = "process";
	    }
	  else
	    {
	      updateGridStatus(args.processID,0);
	      args.command = "close";
	    }

	  return do_stuff_with_process(args);
	}

      retval = generate_wu(process_row,args);
      if(retval != NO_ERROR)
	return retval;
      else if(args.initialization)
	return start_next_generation(process_row,args);//return check_and_start_siblings(process_row,args);
    }
  if(state == queue_states::success)
    return continue_process(process_row,args);
  if(state == queue_states::running)
    std::cout << "process " << process_row[process_fields::ID] << " is currently running." << std::endl;
  if(state == queue_states::invalid)
    return close_wu(process);
  return NO_ERROR;
}

int close_finished_batches(struct grid_master_arguments& args)
{
  if(queue_conn == 0)
    throw GridException("close_finished_batches: No database object.", MYSQL_ERROR);

  std::ostringstream sql;
  MYSQL_RES* finished_batches = finished_jobs();
  if(mysql_errno(queue_conn) != 0)
    {
      sql.str("");
      sql << "close_finished_batches: Could not get finished batch list" << std::endl
	  << "Reason: MYSQL_ERROR(" << mysql_errno(queue_conn) << ") " << mysql_error(queue_conn);
      if(finished_batches != 0)
	mysql_free_result(finished_batches);
      throw GridException(sql,MYSQL_ERROR);
    }

  if(finished_batches == NULL)
    return NO_ERROR;

  MYSQL_ROW process_id;
  struct grid_master_arguments new_args = args;
  std::istringstream buff;
  int retval = NO_ERROR;
  new_args.initialization = false;
  new_args.command = "close";
  while ((process_id = mysql_fetch_row(finished_batches)) != NULL)
    {
      if(process_id == NULL)
	continue;
      std::istringstream buff(process_id[1]);
      buff >> new_args.processID;
      new_args.parameter = process_id[1];
      retval = do_stuff_with_process(new_args);
    }
  mysql_free_result(finished_batches);
  return retval;
}

int start_next_generation(MYSQL_ROW process_row, struct grid_master_arguments& args) throw (GridException)
{
  if(queue_conn == 0)
    throw GridException("start_next_generation: No database object.", MYSQL_ERROR);

  if(process_row == 0)
    throw GridException("start_next_generation:Given a null MYSQL_ROW.", MYSQL_ERROR);

  std::ostringstream sql;
  MYSQL_RES* children = query_db(PROCESS_THAT_CAN_BEGIN_SQL);

  if(mysql_errno(queue_conn) != 0)
    {
      sql.str("");
      sql << "start_next_generation: Could not get child process information" << std::endl
	  << "Reason: MYSQL_ERROR(" << mysql_errno(queue_conn) << ") " << mysql_error(queue_conn);
      if(children != 0)
	mysql_free_result(children);
      throw GridException(sql,MYSQL_ERROR);
    }
  
  MYSQL_ROW process_id;
  struct grid_master_arguments new_args = args;
  std::istringstream buff;
  new_args.initialization = false;
  while((process_id = mysql_fetch_row(children)))
    {
      buff.clear();buff.str(process_id[0]);
      buff >> new_args.processID;//will be int due to database definition.
      new_args.command = "start";
      new_args.parameter = process_id[0];
      do_stuff_with_process(new_args);
    }

  mysql_free_result(children);
  return NO_ERROR;
}


extern int mmpbsa_combine_worker(const std::vector<std::string>& input_filenames, const std::string& out_filename);
extern int sander_combine_worker(const std::vector<std::string>& input_filenames,
				 const std::string& output_filename,const std::string& file_type);

int recombiner(const std::map<std::string,std::vector<std::string> >& filename_organizer,const std::string& apptype)
{
  //for each pair in filename_organizer:
  //combine results based on application type.
  std::map<std::string,std::vector<std::string> >::const_iterator filename_pair = filename_organizer.begin();
  int retval = 0;
  for(;filename_pair != filename_organizer.end();filename_pair++)
    {
      if(apptype == "mmpbsa"|| apptype == "recombine_mmpbsa")
	retval = mmpbsa_combine_worker(filename_pair->second,filename_pair->first);
      else if(apptype == "moledyn" || apptype == "recombine_moledyn")
	{
	  std::string file_type = "";
	  if(filename_pair->first.find("mdcrd") != std::string::npos)
	    file_type = "mdcrd";
	  retval = sander_combine_worker(filename_pair->second,filename_pair->first,file_type);
	}
      else
	retval = -1;
      if(retval != 0)
	{
	  std::cerr << "ERROR - " << get_iso8601_time() << " - recombiner: Could not combine " << filename_pair->first;
	  if(retval == -1)
	    std::cerr << "Application: " << apptype;
	  return retval;
	}
    }
  return retval;
}

std::map<std::string,std::vector<std::string> > get_file_organizer(MYSQL_RES* files)
{
  std::map<std::string,std::vector<std::string> > filename_organizer;
  if(files == 0 || mysql_num_rows(files) == 0)
    return filename_organizer;

  BoincDBConn boincDB = getBoincDBConn();

  //map files to the name they will have when combined.
  MYSQL_ROW filename;
  std::string main_filename;//common name among serial segments.
  std::string result_filename,path,combined_file_subdir;
  std::cout << "have output files: " << std::endl;
  while((filename = mysql_fetch_row(files)))
    {
      combined_file_subdir = "";
      if(filename == 0 || filename[0] == 0)
	continue;

      //get user directory
      if(filename[2] != 0 && strlen(filename[2]) != 0)
	path = filename[2];
      else
	path = boincDB.BOINC_CONFIG_FILE_PATH;
      if(path.at(path.size()-1) != '/')
	path += "/";
	  
      //get subdirectory for combined file, if specified.
      if(filename[3] != 0 && strlen(filename[3]))
	combined_file_subdir = filename[3];
      if(filename[3] && combined_file_subdir.at(combined_file_subdir.size()-1) != '/')
	combined_file_subdir += "/";

      main_filename = path;
      if(filename[5] != 0 && file_types::temporary_output == filename[5])//only perminant files go in the user directory. temporary ones go in the subdirectory.
	main_filename += combined_file_subdir;
      main_filename += getFinalOutputName(filename[0]);
      std::cout << main_filename << std::endl;
      if(filename_organizer.find(main_filename) != filename_organizer.end())
	filename_organizer.at(main_filename).push_back(path + combined_file_subdir + filename[0]);
      else
	{
	  std::vector<std::string> newVector;newVector.push_back(path + combined_file_subdir + filename[0]);
	  filename_organizer[main_filename] = newVector;
	}
    }

  return filename_organizer;
}

int combine_process_results(const std::string& runid, const std::string& parentid, const std::string& apptype)
{
  using std::map;
  using std::string;
  using std::vector;

  int retval;
  //this will map combined-file names with serial-segment names.
  map<string,vector<string> > filename_organizer;

  BoincDBConn boincDB = getBoincDBConn();
	
  std::cout << "combining results" << std::endl;
	
  if(runid.size() == 0)
    throw GridException("combine_process_results was given an invalid runid.",INVALID_INPUT_PARAMETER);

  //query all files in this batch.
  std::ostringstream sql;
  sql << "select file_name,processes.ID,user_directory,subdirectory,file_name_alias,file_type from process_input_files "
    "left join processes on "
    "processes.ID = process_input_files.process_ID where (file_type = " 
      << file_types::perminant_output << " or file_type = " << file_types::temporary_output << ")" 
      << " and processes.ID is not null " <<
    "and runid = " << runid << " and parentid";
  if(parentid.size() == 0)
    sql << " is null";
  else
    sql << " = " << parentid;
  sql << ";";

  //If no files were found, do nothing.
  MYSQL_RES* siblings = query_db(sql.str().c_str());
  if(siblings == 0)
    return 0;
  if(mysql_num_rows(siblings) == 0)
    {
      mysql_free_result(siblings);
      return 0;
    }

  filename_organizer = get_file_organizer(siblings);

  retval = recombiner(filename_organizer,apptype);

  //do not leak memory
  mysql_free_result(siblings);
  return retval;
}

bool all_processes_finished(MYSQL_ROW process_row) throw (GridException)
{
  if(process_row == 0 || process_row[process_fields::runid] == 0)
    throw GridException("all_processes_finished: given an empy runid variable.",INVALID_INPUT_PARAMETER);

  if(queue_conn == 0)
    throw GridException("all_processes_finished: could not connect to queue system. No database object.",MYSQL_ERROR);

  std::ostringstream sql;
  sql << "select count(*) from processes where (state = " <<
    queue_states::running << " or state = " << queue_states::waiting << ") and runid = " << process_row[process_fields::runid] << ";";
  MYSQL_RES* processes = query_db(sql);

  if(processes == 0 && mysql_errno(queue_conn) != 0)
    {
      sql.str("all_processes_finished: ");
      sql << " could not get the process id's for job #" << process_row[process_fields::runid] << std::endl;
      sql << "Reason: MySQL Error #" << mysql_errno(queue_conn) << " " << mysql_error(queue_conn);
      throw GridException(sql,MYSQL_ERROR);
    }

  if(processes == 0)
    return true;

  MYSQL_ROW count = mysql_fetch_row(processes);
  if(count == 0 || count[0] == 0)
    return true;

  std::istringstream count_buff(count[0]);
  int count_int;
  count_buff >> count_int;//will be int thanks to mysql.
  mysql_free_result(processes);
  return count_int == 0;
}

int continue_process(MYSQL_ROW process_row, struct grid_master_arguments& args) throw (GridException)
{
  if(process_row == 0 || process_row[process_fields::ID] == 0
     || process_row[process_fields::runid] == 0 ||  process_row[process_fields::application] == 0 || process_row[process_fields::appname] == 0)
    {
      std::cerr << "ERROR - " << get_iso8601_time() << " - continue_process was given an invalid MYSQL_ROW." << std::endl;
      return MYSQL_ERROR;
    }

  std::string id = process_row[process_fields::ID];
  std::string runid = process_row[process_fields::runid];
  std::string appname = process_row[process_fields::appname];
  std::string appid = process_row[process_fields::application];
  if(!has_permission(process_row[process_fields::uid]))
    throw GridException("continue_process: cannot continue process",INSUFFICIENT_PERMISSION);
  std::string parentid = "";
  if(process_row[process_fields::parentid] != 0)
    parentid = process_row[process_fields::parentid];

  start_next_generation(process_row,args);

  close_finished_batches(args);

  return NO_ERROR;
}

int clean_temporary_files(const std::string& runid, const std::string& tarball_filename, const std::string& fileprefix)
{
  using std::set;
  using std::string;
  using mmpbsa_utils::Zipper;

  std::ostringstream sql;

  // KEEP FILES FOR NOW
  return 0;

  sql << "select * from temp_files where runid = " << runid << ";";//file_name, subdirectory, file_type, runid

  MYSQL_RES* filenames = query_db(sql);
  if(mysql_errno(queue_conn) != 0)
    {
      sql.str("clean_temporary_files: ");
      sql << "could not obtain a list of process_input_files." << std::endl;
      sql << "Reason: MySQL error #" << mysql_errno(queue_conn) << " " << mysql_error(queue_conn);
      throw GridException(sql,MYSQL_ERROR);
    }
  FILE *tarball,*output_tarball;
  tarball = tmpfile();
  set<string> rawfiles;
  set<string> temp_dirs;//temporary directories (to be deleted if empty)
  if(filenames != 0)
    {
      MYSQL_ROW filename;
      std::string full_file_path;
      struct stat info;
      while((filename = mysql_fetch_row(filenames)) != 0)
	{
	  //put the filename in filename and full (absolute) path in filename_alias. 
	  //Then use those to stat. If the file exists, put just the filename
	  //in the vector to pass to the Zipper.
	  grid_file_info temp_file;
	  if(filename == 0 || filename[0] == 0)
	    continue;
	  temp_file.filename_alias = fileprefix;//temporarily store the path here for zipper's use.
	  if(filename[2] != 0)//subdirectory in database
	    temp_file.filename = filename[2];
	  if(filename[2] && temp_file.filename.at(temp_file.filename.size() - 1) != '/')
	    temp_file.filename += "/";
	  if(temp_file.filename_alias.at(temp_file.filename_alias.size() - 1) != '/')
	    temp_file.filename_alias += "/";

	  if(filename[1] == 0)//filename alias in database
	    temp_file.filename += filename[0];
	  else
	    temp_file.filename += filename[1];
	  temp_file.filename_alias += temp_file.filename;

	  if(stat(temp_file.filename_alias.c_str(),&info) == 0)
	    {
	      rawfiles.insert(temp_file.filename);
	      if(filename[2])
		temp_dirs.insert(filename[2]);
	    }
	  /*else
	    std::cerr << "clean_temporary_files: Warning: cannot stat " << filename[0] << std::endl;*/
	}
      mysql_free_result(filenames);
    }


  //tarball and remove files
  if(rawfiles.size() > 0)
    {
      output_tarball = fopen(tarball_filename.c_str(),"w");
      if(output_tarball == NULL)
	{
	  std::cerr << "Warning - clean_temporary_file could not open " << tarball_filename << " to create a raw data archive." << std::endl;
	}
      else
	{
	  set<string>::const_iterator rawfile = rawfiles.begin();
	  std::cerr << "INFO - tar-ing: ";// This goes to stderr so that it ends up in the BOINC daemon log
	  for(;rawfile != rawfiles.end();rawfile++)
	    std::cerr << fileprefix << *rawfile << std::endl;
	  std::cerr << std::endl;
	  try
	    {
	      Zipper::tar(rawfiles,tarball,fileprefix,"grid_data/");
	      rewind(tarball);
	      Zipper::fzip(tarball,output_tarball,Z_BEST_SPEED);
	    }
	  catch(mmpbsa::ZipperException ze)
	    {
	      std::cerr << "Warning - " << get_iso8601_time() << " - Could not archive raw data files."
			<< std::endl
			<< "Reason: " << ze << std::endl;
	    }
	  fclose(output_tarball);
	}
      //try to delete files
      for(set<string>::const_iterator rawfile = rawfiles.begin();
	  rawfile != rawfiles.end();rawfile++)
	{
	  string full_rmpath = fileprefix + *rawfile;
	  if(remove(full_rmpath.c_str()) != 0)
	    {
	      std::cout << "clean_temporary_files: Could not remove " << full_rmpath << std::endl
			<< "Reason: " << strerror(errno) << std::endl;
	    }
	}
      //try to remove directory (if they are empty)
      for(std::set<string>::const_iterator subdir = temp_dirs.begin();
	  subdir != temp_dirs.end();subdir++)
	{
	  string full_rmpath = fileprefix + *subdir;
	  if(rmdir(full_rmpath.c_str()) != 0)
	    {
	      std::cout << "clean_temporary_files: Could not remove directory " << full_rmpath << std::endl
			<< "Reason: " << strerror(errno) << std::endl;
	    }
	}
	  
    }
  if(tarball != NULL)
    fclose(tarball);
  return 0;
}

int close_wu(MYSQL_RES* process)
{
  using std::endl;
  int retval;

  if(process == 0 || mysql_num_rows(process) == 0)
    return NOT_A_PROCESS;

  MYSQL_ROW process_info = mysql_fetch_row(process);

  if(!has_permission(process_info[process_fields::uid]))
    throw GridException("close_wu: cannot continue process",INSUFFICIENT_PERMISSION);

  if(process_info == 0 || process_info[process_fields::runid] == 0 || process_info[process_fields::user_directory] == 0)
    return INVALID_INPUT_PARAMETER;

  std::string runid = process_info[process_fields::runid];

  std::string fileprefix = "./";
  if(process_info[process_fields::user_directory] != 0)
    fileprefix = process_info[process_fields::user_directory];
  if(fileprefix.at(fileprefix.size()-1) != '/')
    fileprefix += "/";

  //prepare tarball filename
  std::ostringstream sql;
  sql << fileprefix << DATA_FILE_PREFIX << runid << ".tgz";
  std::string tarball_filename = sql.str();
  sql.str("");

  //clean temporary files (for this process and siblings).
  retval = clean_temporary_files(runid,tarball_filename,fileprefix);
  if(retval != 0)
    return retval;

  //clean DB
  sql.str("");
  MYSQL_RES* result;
  sql << "delete processes,process_input_files from process_input_files right join processes on processes.ID = process_input_files.process_ID where processes.runid = " << runid << ";";
  result = query_db(sql);
  if(mysql_errno(queue_conn) != 0)
    {
      std::cerr << "ERROR - " << get_iso8601_time() << " - close_wu: " << mysql_error(queue_conn) << std::endl;
      mysql_free_result(result);
      return mysql_errno(queue_conn);
    }
  mysql_free_result(result);

  std::cout << "Closed job #" << runid << " and all processes depending on it." << std::endl;
  return NO_ERROR;
}

int validate_process_result(MYSQL_RES* process_result)
{
  if(process_result == 0 || mysql_errno(queue_conn) != 0)
    {
      std::cerr << "ERROR - " << get_iso8601_time() << " - validate_process_result: MySQL Error #" << mysql_errno(queue_conn) << ": " << mysql_error(queue_conn) << std::endl;
      return MYSQL_ERROR;
    }

  if(process_result->row_count == 0)
    return NOT_A_PROCESS;
  if(process_result->row_count != 1)
    return MULTIPLE_ROWS;

  return 0;

}

int generate_wu(struct grid_master_arguments& args)
{
  MYSQL_RES* processInfo = get_process(args.processID);
  int retval = validate_process_result(processInfo);

  if(retval)
    {
      std::cout << "validate_process_result failed in generate_wu." << std::endl;
      return retval;
    }

  MYSQL_ROW processRow = mysql_fetch_row(processInfo);

  retval = generate_wu(processRow, args);//checks for permission.

  mysql_free_result(processInfo);
  return retval;
}

std::vector<grid_file_info> get_output_files(const std::string& processID) throw (GridException)
{
  std::vector<grid_file_info> returnMe;
  

  if("" == processID)
    return returnMe;

  std::ostringstream sql;
  sql << "select file_name,file_name_alias,file_type from process_input_files "
    "where process_ID = " << processID;
    
  sql << " and (file_type = " << file_types::perminant_output;
  sql << " or file_type = " << file_types::temporary_output;
  sql << ");";

  MYSQL_RES* file_rows = query_db(sql);
  if(mysql_errno(queue_conn) != 0)
    {
      sql << "get_output_files: could not get output file list." << std::endl;
      sql << "MySQL Error #" << mysql_errno(queue_conn) << " " << mysql_error(queue_conn);
      throw GridException(sql,MYSQL_ERROR);
    }
    
  if(file_rows == 0 || mysql_num_rows(file_rows) == 0)
    return returnMe;

  MYSQL_ROW file;
  while((file = mysql_fetch_row(file_rows)) != 0)
    {
      std::string file_name = file[0];
      returnMe.push_back(get_file_info(0,&file_name));
    }

  mysql_free_result(file_rows);
  return returnMe;
}

std::string str_int_to_hex(std::string int_string)
{
  std::istringstream input(int_string);
  int val;
  std::ostringstream output;
  
  input >> val;
  if(input.fail())
    {
      std::cerr << "ERROR - " << get_iso8601_time() << " - " << int_string << " is not a valid integer." << std::endl;
      return "ERROR";
    }  
  output << std::hex << val;
  return output.str();
}

std::string str_hex_to_int(std::string int_string)
{
  std::istringstream input(int_string);
  int val;
  std::ostringstream output;
  
  input >> std::hex >> val;
  if(input.fail())
    {
      std::cerr << "ERROR - " << get_iso8601_time() << " - " << int_string << " is not a valid hexadecimal integer." << std::endl;
      return "ERROR";
    }  
  output << std::dec << val;
  return output.str();
}


int generate_wu(MYSQL_ROW processRow, struct grid_master_arguments& args)
{
  if(!has_permission(processRow[process_fields::uid]))
    throw GridException("continue_process: cannot continue process",INSUFFICIENT_PERMISSION);

  std::vector<grid_file_info> infiles = get_input_files(processRow[process_fields::ID]);

  std::vector<grid_file_info> outfiles;
  try
    {
      outfiles = get_output_files(processRow[process_fields::ID]);
    }
  catch(GridException ge)
    {
      std::cerr << "ERROR - " << get_iso8601_time() << std::endl;
      std::cerr << ge << std::endl;
      return ge.getErrType();
    }
  std::ostringstream sql;
  sql << "select name,gridid from applications where ID = "
      << processRow[process_fields::application] << ";";
  MYSQL_RES* appname = query_db(sql);

  int retval = validate_process_result(appname);
  if(retval)
    {
      std::cerr << "ERROR - " << get_iso8601_time() << " - validate_process_result failed in generate_wu when retrieving appname." << std::endl;
      std::cerr << "SQL statement: " << std::endl <<  sql.str() << std::endl;
      return retval;
    }

  MYSQL_ROW appname_row = mysql_fetch_row(appname);

  std::string strAppname,appid;
  strAppname = (appname_row[0]) ? appname_row[0] : "";
  appid = (appname_row[1]) ? appname_row[1] : "";

  retval = spawn_wu(infiles, outfiles, strAppname,appid,str_int_to_hex(processRow[process_fields::ID]),processRow[process_fields::gflop_estimate],args);

  if(retval == NO_ERROR)
    {
      std::ostringstream sql;
      sql << "update processes set state = " << queue_states::running
	  << " where ID = " << processRow[process_fields::ID] << " limit 1;";
      mysql_query(queue_conn,sql.str().c_str());
      if(mysql_errno(queue_conn))
	std::cerr << "ERROR - " << get_iso8601_time() << " - generate_wu: " << mysql_error(queue_conn) << std::endl;
    }

  mysql_free_result(appname);

  return retval;
}

std::vector<grid_file_info> get_process_files(const std::string& processID)
{
  std::vector<grid_file_info> returnMe;

  if("" == processID)
    return returnMe;

  std::string sql = "select file_name,file_name_alias,file_type,subdirectory from process_input_files "
    "left join file_types on file_types.ID = process_input_files.file_type "
    "where process_ID = " + processID + ";";
  MYSQL_RES* file_rows = query_db(sql);
  if(mysql_errno(queue_conn) != 0)
    std::cerr << "ERROR - " << get_iso8601_time() << " - get_process_files: " << mysql_error(queue_conn) << std::endl;
    
  if(file_rows == 0 || mysql_num_rows(file_rows) == 0)
    return returnMe;

  MYSQL_ROW file;
  while((file = mysql_fetch_row(file_rows)))
    {
      returnMe.push_back(get_file_info(file));
    }

  return returnMe;
}

std::vector<grid_file_info> get_input_files(const std::string& processID) throw (GridException)
{
  std::vector<grid_file_info> returnMe;


  if("" == processID)
    return returnMe;

  std::ostringstream sql;
  sql << "select file_name,file_name_alias,file_type from process_input_files "
    "where process_ID = " << processID;

  sql << " and (file_type = " << file_types::perminant_input;
  sql << " or file_type = " << file_types::temporary_input;
  sql << ");";

  MYSQL_RES* file_rows = query_db(sql);
  if(mysql_errno(queue_conn) != 0)
    {
      sql << "get_input_files: could not get input file list." << std::endl;
      sql << "MySQL Error #" << mysql_errno(queue_conn) << " " << mysql_error(queue_conn);
      throw GridException(sql,MYSQL_ERROR);
    }

  if(file_rows == 0 || mysql_num_rows(file_rows) == 0)
    return returnMe;

  MYSQL_ROW file;
  while((file = mysql_fetch_row(file_rows)) != 0)
    {
      std::string filename = file[0];
      returnMe.push_back(get_file_info(0,&filename));
    }

  mysql_free_result(file_rows);
  return returnMe;
}

int parseID(const std::string& arg)
{
  if(arg.size() == 0)
    return -1;
  int ID;
  std::istringstream cl_arg(arg);
  cl_arg >>  ID;
  if(cl_arg.fail())
    return -1;
  return ID;
}

MYSQL_RES * get_process(const int& process_id)
{
  std::ostringstream idBuff;
  idBuff << "select processes.ID,parentid,application,runid,state,user_directory,name as appname,uid,gflop_estimate,arguments from processes left join applications on processes.application = applications.ID "
	 << " where processes.ID = " << process_id << ";";
  MYSQL_RES* retval = query_db(idBuff);//maintain order with enum FIELD_ID above
  if(mysql_errno(queue_conn) != 0)
    std::cout << "get_process SQL statement: " << idBuff.str() <<std::endl;
  return retval;
}

std::string get_user_path(const std::string& process_id) throw (GridException)
{
  if(queue_conn == 0)
    throw GridException("get_user_path: Could not connect to queue system. No database object.",MYSQL_ERROR);
      
  std::ostringstream sql;
  sql << "select user_directory from processes "
    " where ID = " << process_id << ";";
  
  MYSQL_RES* res = query_db(sql);//maintain order with enum FIELD_ID above
  if(mysql_errno(queue_conn) != 0)
    {
      sql.str("");
      sql << "get_user_path: Could not get user path. MYSQL ERROR(" << mysql_errno(queue_conn)
	  << "): " << mysql_error(queue_conn) << std::endl
	  << "get_user_path SQL statement: " << sql.str();
      throw GridException(sql,MYSQL_ERROR);
    }
  if(res == 0)
    return "./";
  
  std::string returnMe;
  if(mysql_num_rows(res) == 0)
    returnMe = "./";
  else
    {
      MYSQL_ROW row = mysql_fetch_row(res);
      if(row == 0 || row[0] == 0)
	returnMe = "./";
      else
	returnMe = row[0];
    }

  mysql_free_result(res);
  return returnMe;
}

/**
 * Creates a file_info tag for BOINC workunit templates. These are different
 * from result tempalte file_info tags.
 */
std::string makeFileinfoTag(const std::string& filename, const int& number)
{
  using std::endl;
  std::ostringstream tag;
  tag << "<file_info>" << endl << "<number>";
  tag << number;
  tag << "</number>" << endl;
  tag << "<name>" << filename << "</name>" << endl << "</file_info>";
  return tag.str();
}

/**
 * Creates a file_ref tag for BOINC workunit templates.
 */
std::string makeFilerefTag(const std::string& filename, const int& number)
{
  using std::endl;
  std::ostringstream tag;
  tag << "<file_ref>" << endl << "<file_number>";
  tag << number;
  tag << "</file_number>" << endl;
  tag << "<open_name>" << filename << "</open_name>" << endl;
  tag << "</file_ref>";
  return tag.str();
}

/**
 * Creates a work unit template for a given list of input files.
 * This work unit is returned as a string, which can be pass to create_work from
 * the BOINC API.
 */
std::string getWUTemplate(const char* wu_name, const std::vector<grid_file_info>& infiles)
{
  std::string header = "";
  std::string footer = "<workunit>\n";
  std::vector<grid_file_info>::const_iterator file;
  int fileCounter = 0;
  std::string queueFile = "";
  for(file = infiles.begin(); file != infiles.end();file++)
    {
      const std::string& filename = file->filename;
      if(filename.find("queue") != std::string::npos)
	queueFile = filename;
      header += makeFileinfoTag(filename,fileCounter) + "\n";
      footer += makeFilerefTag(filename,fileCounter++)+"\n";
    }
  if(queueFile.size() != 0)
    footer += "<command_line>queue=" + queueFile + "</command_line>\n";
  footer += "</workunit>";
  return header+footer;
}

/**
 * Creates a file_info tag for a result template. This is different from a workunit
 * file_info tag, because it contains information as to whether or not the
 * file is optional and the maximum expected file size.
 */
std::string getResultFileInfo(const std::string& filename, const size_t& file_counter)
{
  std::ostringstream buff;
  using std::endl;
  buff << "<file_info>" << endl
       << "<name>" << "<OUTFILE_" << filename << "/>" <<  "</name>" << endl
       << "<generated_locally/>" << endl
       << "<upload_when_present/>" << endl
       << "<max_nbytes>50000000</max_nbytes>" << endl
       << "<url><UPLOAD_URL/></url>" << endl
       << "</file_info>";
  return buff.str();
}

/**
 * Creates a file_ref tag for BOINC result templates. These are different from
 * work unit templates, because the file_name uses the result name.
 */
std::string getResultFileref(const std::string& filename, const size_t& file_counter)
{
  std::ostringstream buff;
  using std::endl;
  buff << "<file_ref>" << endl
       << "<file_name>" << "<OUTFILE_" << filename << "/>" << "</file_name>" << endl
       << "<open_name>" << filename << "</open_name>" << endl
       << "</file_ref>";
  return buff.str();
}

/**
 * Creates a result template in the "templates" subdirectory in the boinc project path. Then the name
 * of this template file is place in the template_filename string.
 */
void getResultTemplateNames(const std::vector<grid_file_info>& filenames, const char* wu_name, std::string& template_filename, std::string& full_template_path) throw (GridException)
{
  using std::endl;

  template_filename = "templates/";
  template_filename += wu_name;
  template_filename += "_result_template.xml";

  BoincDBConn boincDB = getBoincDBConn();
  full_template_path += boincDB.BOINC_CONFIG_FILE_PATH;
  full_template_path += template_filename;

  std::fstream templateFile;
  templateFile.open(full_template_path.c_str(),std::ios::out);
  if(!templateFile.is_open())
    {
      throw GridException("Could not open " + full_template_path,IO_ERROR);
    }
  std::vector<grid_file_info>::const_iterator filename;
  std::string header = "";
  std::string footer = "<result>\n";
  size_t file_counter = 0;
  for(filename = filenames.begin();filename != filenames.end();filename++)
    {
      header += getResultFileInfo(filename->filename,file_counter) + "\n";
      footer += getResultFileref(filename->filename,file_counter) + "\n";
    }

  header += getResultFileInfo("sander-stdout.txt",0) + "\n";
  footer += "<file_ref>\n<file_name><OUTFILE_sander-stdout.txt/></file_name>\n<open_name>sander-stdout.txt</open_name>\n<optional>1</optional>\n</file_ref>\n";

  footer += "</result>";
  templateFile << header << footer;
  templateFile.close();

}

 
int grid_stage_file(const char *src_file_path,const char * dest_file_path)
{
  struct stat fs;
  int retval = stat(src_file_path,&fs);
  const off_t max_file_size = MAX_GRID_FILE_SIZE *1048576;// convert to bytes
  if(retval)
    {
      std::ostringstream error;
      error << "grid_stage_file: Error in copying " << src_file_path << " to " << dest_file_path << std::endl;
      error << "Acording to stat: " << strerror(errno);
      throw GridException(error,IO_ERROR);
    }
  
  if(fs.st_size > max_file_size)
    {
      std::ostringstream error;
      error << "grid_stage_file: " << src_file_path << " exceeds the specified maximum file size for data sent to the grid..\nSize: " << fs.st_size << " bytes\nMax: " << max_file_size << " bytes";
      throw GridException(error,IO_ERROR);
    }
#ifdef USE_BOINC
  boinc_copy(src_file_path,dest_file_path);
#endif
  return 0;
}

int spawn_wu(const std::vector<grid_file_info>& infiles,const std::vector<grid_file_info>& outfiles,
	     const std::string& appname,const std::string& appid,
	     const std::string& jobid,const char* gflop_count_str,
	     const struct grid_master_arguments& args)
{
#ifdef USE_BOINC
  DB_APP app;
  DB_WORKUNIT wu;
  using std::string;
  string wu_template;
  string wu_template_name,template_filename,full_template_path;
  string lookup_clause;
  char path[1024];
  int retval;
  const char* infiles_array[infiles.size()];
  std::string infile_path;
  size_t fileCounter;
  float gflop_count = 0;
  string iso8601;// Time string formatted using ISO8601
  //get path info
  SCHED_CONFIG config;
  BoincDBConn boincDB = getBoincDBConn();
  config.parse_file(boincDB.BOINC_CONFIG_FILE_PATH);
  retval = boinc_db.open(boincDB.BOINC_DB_NAME,boincDB.DB_HOST_IP,boincDB.BOINC_DB_USER,boincDB.BOINC_DB_PASSWORD);
  if(retval != 0)
    {
      std::cerr << "ERROR - " << get_iso8601_time() << " - Could not connect to " << boincDB.BOINC_DB_NAME
		<< " on " << boincDB.DB_HOST_IP << "Reason: "
		<< boincerror(retval) << std::endl;
      return retval;
    }

  lookup_clause = "where id = " + appid;
  app.lookup(lookup_clause.c_str());

  //Create workunit template (char*)
  wu.clear();     // zeroes all fields
  iso8601 = ::get_iso8601_time();
  sprintf(wu.name, "%s_%s_%d",appname.c_str(),jobid.c_str(),iso8601.c_str());
  wu_template = getWUTemplate(wu.name,infiles);

  //create result template. local file.
  try{
    getResultTemplateNames(outfiles, wu.name, template_filename,full_template_path);
  }
  catch(GridException ge)
    {
      std::cerr << "ERROR - " << get_iso8601_time() << " - Could not create template file. " << std::endl;
      std::cerr << "Reason: ";
      std::cerr << ge << std::endl;
      return ge.getErrType();
    }

  //check to see if we (think we) know the gflop count for this job.
  if(gflop_count_str != 0)
    {
      std::istringstream gflop_buff(gflop_count_str);
      gflop_buff >> gflop_count;
      if(gflop_buff.fail())
	{
	  gflop_count = 0;
	  std::cerr << "Warning - Invalid gflop estimate of " << gflop_count_str << std::endl;
	}
    }

  //create work unit object.
  wu.appid = app.id;
  wu.min_quorum = GRID_RESULT_QUORUM;
  wu.target_nresults = 2;
  wu.max_error_results = GRID_MAX_ERROR_RESULTS;
  wu.max_total_results = GRID_MAX_TOTAL_RESULTS;
  wu.max_success_results = GRID_MAX_SUCCESS_RESULTS;
  wu.rsc_fpops_est = (gflop_count == 0) ? DEFAULT_GFLOP_COUNT * 1e9: gflop_count * 1e9;
  //wu.rsc_fpops_est *= 10;//factor of 10: for some reason, boinc is in its time estimation. Figure out why!!! Probably: boinc now keeps track of how well the fpops_est predicted the time it took to run and tries to adjust accordingly.
  wu.rsc_fpops_bound = 9.0e15;//;3.0*wu.rsc_fpops_est;
  wu.rsc_memory_bound = 1e8;
  wu.rsc_disk_bound = 1e8;
  wu.delay_bound = 24*3600;// 1 day deadline

  //get input file names and copy them to download directory
  char newFileName[4096];
  std::string user_path;
  try{
    user_path = get_user_path(str_hex_to_int(jobid));
  }
  catch(GridException ge)
    {
      std::cerr << "ERROR - " << get_iso8601_time() << std::endl;
      std::cerr << ge << std::endl;
      return ge.getErrType();
    }
  fileCounter = 0;
  for(std::vector<grid_file_info>::const_iterator infile = infiles.begin();
      infile != infiles.end();infile++)
    {
      infiles_array[fileCounter++] = infile->filename.c_str();
      config.download_path(infile->filename.c_str(),newFileName);
      infile_path = user_path;

      //construct path
      if(infile_path.at(infile_path.size() - 1) != '/')
	infile_path += "/";
      if(infile->subdirectory.size() != 0)
	infile_path += infile->subdirectory;
      if(infile_path.at(infile_path.size() - 1) != '/')
	infile_path += "/";

      //Append filename to path. use alias for filename if it exists.
      if(infile->filename_alias != "")
	infile_path += infile->filename_alias;
      else
	infile_path += infile->filename;

      //copy file
      if(args.grid_master_verbosity > 0 || args.simulate_spawn)
	std::cout << "copying: " << infile_path << " to " << newFileName << std::endl;

      retval = grid_stage_file(infile_path.c_str(),newFileName);
      std::string scrub_command = "dos2unix ";
      scrub_command += newFileName;
      if(system(scrub_command.c_str()))
	std::cerr << "WARNING - Could not dos2unix convert file: " << newFileName << std::endl;
	   
      if(retval != 0)
    	{
	  std::ostringstream error;
	  error << " ERROR - spawn_wu: could not copy " << infile_path << " to " << newFileName << std::endl;
	  error << "Reason (" << retval << "): " << boincerror(retval) << std::endl;
	  if(retval == ERR_FOPEN)
	    error << "fopen message (" << errno << "): " << strerror(errno) << std::endl;
	  throw GridException(error,BOINC_ERROR);
    	}
      chmod(newFileName,S_IRWXU|S_IRGRP|S_IWGRP|S_IROTH);//this way, if someone else copies, the grid manager can read/write(as member of the group)
    }

  //Submit work unit
  if(!args.simulate_spawn)
    {
      return create_work(
			 wu,
			 wu_template.c_str(),
			 template_filename.c_str(),/*relative to project path*/
			 full_template_path.c_str(),
			 infiles_array,
			 infiles.size(),
			 config
			 );
    }
  else
    {

      std::cout << "wu: "<< wu.name << " appid: " << wu.appid << std::endl <<
	"wu_template: " << wu_template << std::endl;
      for(int i = 0;i<infiles.size();i++)
	std::cerr << infiles_array[i] << std::endl;
      return 1;
    }

#else
  using namespace std;
  string queueFile;
  for(vector<grid_file_info>::const_iterator it = infiles.begin();
      it != infiles.end();it++)
    if(it->filename.find ("queue") != string::npos)
      queueFile = it->filename;
  cout << "mmpbsa queue=" << queueFile << endl;
  cout << appname << " ";
  if(infiles.size())
    {
      cout << "The following files are to be used:" << endl;
      for(vector<grid_file_info>::const_iterator it = infiles.begin();
	  it != infiles.end();it++)
	cout << it->filename << endl;
    }
  cout << endl;

  return NO_ERROR;
#endif
}


bool has_permission(const char* uid)
{
  if(uid == 0)
    throw GridException("Could not authenticate user.",INVALID_INPUT_PARAMETER);

  //boinc daemon can do anything to any process. period.
  BoincDBConn boinc_daemon = getBoincDBConn();
  if(boinc_daemon.BOINC_DAEMON_UID == getuid())
    return true;

  std::istringstream uid_buff(uid);
  uid_t provided_uid;
  uid_buff >> provided_uid;
  if(uid_buff.fail())
    {
      throw GridException("Could not authenticate user.",INVALID_INPUT_PARAMETER);
    }

  return (provided_uid == getuid());
}


int recombine_processes(MYSQL_RES* process, struct grid_master_arguments& args)
{
  MYSQL_ROW process_row;
 
  if(process == 0 || mysql_num_rows(process) == 0)
    throw GridException("recombine_processes: Invalid process query result.",INVALID_INPUT_PARAMETER);

  if(process->current_row == 0)
    mysql_row_seek(process,mysql_row_tell(process)-1);

  process_row = mysql_fetch_row(process);
  return recombine_processes(process_row,args);

}

int recombine_processes(MYSQL_ROW process_row, struct grid_master_arguments& args)
{
  std::map<std::string,std::vector<std::string> > file_organizer;
  MYSQL_RES* files;
  MYSQL_ROW file;
  std::ostringstream sql;
  std::string appname;
  const char* uid;

  uid = process_row[process_fields::uid];
  appname = process_row[process_fields::appname];
  
  if(uid == 0 || !has_permission(uid))
    throw GridException("recombine_processes: Insufficient permission to recombine these files.",INSUFFICIENT_PERMISSION);

  sql << "select file_name,processes.ID,user_directory,subdirectory,file_name_alias,file_type from process_input_files left join processes on process_ID = processes.ID where process_ID = " << args.processID << " order by process_input_files.ID asc;";
  files = query_db(sql);

  if(mysql_errno(queue_conn))
    {
      sql.str("");
      sql << "recombine_processes: Could not query queue database. " << std::endl << "Reason: ";
      sql << "MySQL Error #" << mysql_errno(queue_conn) << ": " << mysql_error(queue_conn);
      throw GridException(sql,MYSQL_ERROR);
    }

  if(files == 0 || mysql_num_rows(files) == 0)
    return 0;

  //fill file list
  while((file = mysql_fetch_row(files)) != NULL)
    {
      std::string input_filename,output_filename;
      if(file[4] == 0)
	throw GridException("recombine_processes: given a file without a file name alias.",INVALID_INPUT_PARAMETER);
      if(file[2] == 0)
	throw GridException("recombine_processes: given a file without directory.",INVALID_INPUT_PARAMETER);
      input_filename = output_filename = file[2];
      if(input_filename.at(input_filename.size() - 1) != '/')
	input_filename += "/";
      if(file[3] != 0)
	input_filename += file[3];
      if(input_filename.at(input_filename.size() - 1) != '/')
	input_filename += "/";
      input_filename += file[0];
      
      if(output_filename.at(output_filename.size() - 1) != '/')
	output_filename += "/";
      if(file[5] != 0 && strcmp(file[5],file_types::temporary_output.c_str()) == 0)
	output_filename += file[3];
      if(output_filename.at(output_filename.size() - 1) != '/')
	output_filename += "/";
      output_filename += file[4];

      if(file_organizer.find(output_filename) == file_organizer.end())
	{
	  std::vector<std::string> merge_map;merge_map.push_back(input_filename);
	  file_organizer[output_filename] = merge_map;
	}
      else
	file_organizer[output_filename].push_back(input_filename);
      
    }
  mysql_free_result(files);
  return recombiner(file_organizer,appname);
}

void updateGridStatus(const int& processID,const int& canonical_resultid)
{
  std::ostringstream sql;
  if(queue_conn == 0)
    {
      throw GridException("updateGridStatus: Could not connect to queue system database. No database object exists.",MYSQL_ERROR);
    }
  sql << "update processes set state = ";
  if(canonical_resultid != 0)
    sql << queue_states::success;
  else
    sql << queue_states::invalid;
  sql << " where ID = " << processID;
  MYSQL_RES* result = query_db(sql);
  if(mysql_errno(queue_conn) != 0)
    {
      std::ostringstream error;
      error << "Could not connect to queue system database. " << std::endl;
      error << "Reason: MySQL error #" << mysql_errno(queue_conn)
	    << ": " << mysql_error(queue_conn);
      throw GridException(error,MYSQL_ERROR);
    }
  mysql_free_result(result);//not needed. update if you can.
}

int submit_and_start(struct submit_job_arguments& sj_args, struct grid_master_arguments& gm_args)
{
	std::map<std::string,grid_file_info> file_list;
  	int retval = submit_job_main(sj_args,file_list);

	if(retval != 0)
	  return retval;
	if(gm_args.simulate_spawn)
	  {
	    if(gm_args.grid_master_verbosity >= MEDIUM_VERBOSITY)
	      std::cout << "Without the simulated staging flag, the first process would begin now." << std::endl;
	    return retval;
	  }

	if(!connect_db())
	{
	  std::cerr << "ERROR - " << get_iso8601_time() << " - gsub: could not connect to queue system database." << std::endl;
		std::cerr << "Reason: ";
		if(queue_conn == NULL)
		  std::cerr << "queue connection is a NULL pointer.";
		else
		  std::cerr << mysql_error(queue_conn);
		std::cerr << std::endl;
		return 1;
	}

	std::ostringstream sql;
	sql << "select ID from processes where runid = " << sj_args.runid << " and parentid is null order by ID asc limit 1;";

	std::cerr << "SQL query: " << std::endl << sql.str() << std::endl;

	MYSQL_RES * process = query_db(sql);

	if(mysql_errno(queue_conn) != 0)
	  {
	    std::cerr << "ERROR - " << get_iso8601_time() << " - gsub: Could not start process due to a database error: " << std::endl;
	    std::cerr << "MySQL Error # " << mysql_errno(queue_conn) << ": " << mysql_error(queue_conn) << std::endl;
	    return mysql_errno(queue_conn);
	  }

	if(process == 0 || mysql_num_rows(process) == 0)
	{
	  std::cerr << "ERROR - " << get_iso8601_time() << " - gsub: Could not start the process." << std::endl;
		return 1;
	}

	MYSQL_ROW processid = mysql_fetch_row(process);
	if(processid == 0)
	{
	  std::cerr << "ERROR - " << get_iso8601_time() << " - gsub: Could not start the process." << std::endl;
			return 1;
	}
	mysql_free_result(process);

	gm_args.parameter = processid[0];
	retval = grid_master_worker(gm_args);
	
	return retval;
}

