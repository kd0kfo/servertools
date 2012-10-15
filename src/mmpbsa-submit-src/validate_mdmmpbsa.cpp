/**
 * BOINC validation routines.
 *
 * The boinc server calls init_result, compare_results, cleanup_result and compute_granted_credit.
 * Those functions determine which specific process should be used based on the process type,
 * e.g. mmpbsa or molecular dynamics.
 *
 * Created by David Coss, 2010
 */

#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "validation_states.h"
#include "validate_mmpbsa.cpp"
#include "validate_moledyn.cpp"
#include "GridException.h"
#include "db_config.h"

#include "libmmpbsa/XMLParser.h"
#include "libmmpbsa/XMLNode.h"

//BOINC Stuff
#include "boinc/str_util.h"
#include "boinc/sched_util.h"
#include "boinc/validate_util.h"
#include "boinc/filesys.h"


std::string get_iso8601_time(){return mmpbsa_utils::get_iso8601_time();}


/**
 * Determines the application name based on the work unit name.
 *
 * A GridException is thrown if the name delimiter is not found.
 */
std::string getAppType(const std::string& wu_name) throw (GridException)
{
	using std::string;
	if(wu_name.find(FIELD_DELIMITER) == string::npos)
		throw GridException("Invalid work unit name. Names must contain a prefix followed by \"_\".\nName: " + wu_name,INVALID_WORKUNIT);

	return wu_name.substr(0,wu_name.find(FIELD_DELIMITER));
}




/**
 * Returns a pointer to a string object contain the path of the data
 * for the result. This is used by the boinc function compare_results.
 */
std::string* init_result_mmpbsa(const std::string& filepath)
{
	return new std::string(filepath);
}

/**
 * Returns a pointer to a string object contain the path of the data
 * for the result. This is used by the boinc function compare_results.
 */

std::string* init_result_moledyn(const std::string& filepath)
{
	return new std::string(filepath);
}


/**
 * Takes a result and loads the result data into memory. void pointer
 * is used because the type of data can vary depending on the process.
 * The data is loaded into memory and a pointer is passed to compare_results.
 */
int init_result(RESULT& result, void*& data)
{
	using std::string;
	using std::cerr;
	using std::endl;
	int appid = result.appid;
	OUTPUT_FILE_INFO fi;
	string filepath,result_name;
	int i, n, retval;
	double x;

	retval = get_output_file_path(result, filepath);
	result_name = result.name;
	if (retval != 0)
	{
	  cerr << "ERROR - " << get_iso8601_time() << " - Could not open file result (" << result_name << ") output file." << endl;
		cerr << "Reason (" << retval << "): " << boincerror(retval) << endl;
		return retval;
	}

	string apptype;
	try
	{
		apptype = getAppType(result_name);
	}
	catch(GridException ge)
	{
	  std::cerr << "ERROR - " << get_iso8601_time() << std::endl;
	  cerr << ge << endl;
	  return ge.getErrType();
	}

	if(apptype == "mmpbsa")
	  data = (void*) init_result_mmpbsa(filepath);
	else if(apptype == "moledyn")
	  data = (void*) init_result_moledyn(filepath);
	else
	  return INVALID_WORKUNIT;
	return 0;
}

/**
 * Compares mmpbsa data using validateMMPBSAWorker in validate_mmpbsa.cpp
 */

int compare_results_mmpbsa(void* lhs_data,void* rhs_data)
{
  std::string curr;
  BoincDBConn boincDB;
  std::string tolerance_path;
  int result;
  size_t argc = 0;
  std::vector<std::string> datafiles;
  mmpbsa::EMap tolerance;
  const char tolerance_filename[] = "tolerance.xml";

  // provided path for tolerance file
  boincDB = getBoincDBConn();
  tolerance_path = boincDB.BOINC_CONFIG_FILE_PATH;
  tolerance_path += tolerance_filename;
  if(access(tolerance_path.c_str(),R_OK) == 0)// can a tolerance file be read from?
    {
      int retval = load_mmpbsa_tolerance(tolerance_path.c_str(),tolerance);
      if(retval != 0)
	{
	  fprintf(stderr,"compare_results_mmpbsa: Could not load tolerance file\n");
	  return VALIDATOR_IO_ERROR;
	}
      else
	fprintf(stderr,"Using tolerance file %s\n",tolerance_path.c_str());
    }
  else if(access(tolerance_path.c_str(),F_OK) == 0)// does it exist if it can't be read from?
    {
      fprintf(stderr,"Could not open %s\n",tolerance_path.c_str());
      return VALIDATOR_IO_ERROR;
    }

  //Insert the file names into the list to give the validation routine
  curr = *((std::string*) lhs_data);
  datafiles.push_back(curr);
  curr = *((std::string*) rhs_data);
  datafiles.push_back(curr);

  try{
    mmpbsa_utils::XMLNode* stats = validateMMPBSAWorker(datafiles,tolerance,result);
    std::cerr << "INFO - " << 	  get_iso8601_time() << " - Validation result for " << "mmpbsa" << ": " << result << std::endl;

    // If validation failed and statistics exist, dump the statistics
    if(result != 0)
      {
	std::fstream errorStats;
	size_t npos = std::string::npos, path_ender;
	
	// For: 
	//    0) copy lhs_data
	//    1) copy rhs_data
	//    2) write stats (using filename of lhs_data
	for(size_t fileidx = 0;fileidx < 3;fileidx++)
	  {
	    curr = datafiles.at(0 % 2);
	    path_ender = curr.find_last_of('/');
	    if(path_ender != npos)
	      curr.erase(0,path_ender+1);
	    if(fileidx == 2)
	      {
		curr ="STATS_" + curr;
	      }
	    curr = ERROR_STAT_DIR + curr;
	    curr = boincDB.BOINC_CONFIG_FILE_PATH + curr;
	    
	    if(fileidx == 2)
	      {
		//write stats
		fprintf(stderr,"Saving stats in %s\n",curr.c_str());
		errorStats.open(curr.c_str(),std::ios::out);
		if(errorStats.is_open())
		  {
		    if(stats == NULL)// If we have no statistics, we cannot write a statistics file...
		      {
			errorStats << "No statistics available" << std::endl;
			break;
		      }
		    errorStats << stats->toString() << std::endl;
		    errorStats.close();
		  }
		else
		  fprintf(stderr,"Could not write to %s\n",curr.c_str());
		errorStats.close();
	      }
	    else
	      boinc_copy(datafiles.at(fileidx).c_str(),curr.c_str());//copy data for analysis.
	  }
      }
    delete stats;
  }
  catch(mmpbsa::XMLParserException xmlpe)
    {
      std::cerr << "ERROR - " << get_iso8601_time() << std::endl;
      std::cerr << xmlpe << std::endl;
      result = XML_FILE_ERROR;
    }

  return result;
}

/**
 * Compares results of molecular dynamics processes.
 */
int compare_results_moledyn(void* lhs_data,void* rhs_data)
{
	int result;
	std::vector<mmpbsa::EnergyInfo> stats;
	struct validate_moledyn_arguments validation_args;
	validation_args.stats_file = "";
	validation_args.tolerance = "";
	validation_args.verbose_level = 0;

	//Do we have a tolerance file?
	struct stat info;
	BoincDBConn boincDB = getBoincDBConn();
	std::string tolerance_filename = boincDB.BOINC_CONFIG_FILE_PATH;
	tolerance_filename += "tolerance.mdout";
	if(stat(tolerance_filename.c_str(),&info) == 0)
	  {
	    std::cout << "Using tolerance file: " << std::endl;
	    validation_args.tolerance = tolerance_filename;
	    std::cout << validation_args.tolerance << std::endl;
	  }

	//Can only validate mdout files. So any other file is passed along.
	std::string * lhs_file = (std::string*) lhs_data;
	if(lhs_file && lhs_file->find(".") != std::string::npos && lhs_file->substr(lhs_file->find(".") + 1).find("out") == std::string::npos)
	  return GOOD_VALIDATION;

	std::vector<std::string>& datafiles = validation_args.mdout_files;
	datafiles.push_back(*((std::string*) lhs_data));
	datafiles.push_back(*((std::string*) rhs_data));
	stats = validateMoledynWorker(validation_args,result);
	std::cerr << "ERROR - " << get_iso8601_time() << " - Validation result for " << "moledyn" << ": " << result << std::endl;
	if(result != 0 && stats.size() != 0)
	{
		std::fstream errorStats;
		std::string curr = datafiles.at(0);
		size_t npos = std::string::npos;
		size_t path_ender;

		curr = datafiles.at(0);
		path_ender = curr.find_last_of('/');
		if(path_ender != npos)
		  curr.erase(0,path_ender+1);
		curr = "STATS_" + curr;
		curr = ERROR_STAT_DIR + curr;
		curr = boincDB.BOINC_CONFIG_FILE_PATH + curr;
		errorStats.open(curr.c_str(),std::ios::out);
		if(errorStats.is_open())
		{
		  print_validation_error(errorStats,stats);
		  errorStats.close();
		}
		else
		  fprintf(stderr,"Could not open %s for writing\n",curr.c_str());		
	}
	if(result != 0)
	  {
	    std::string curr;
	    size_t npos = 0;
	    for(size_t fileidx = 0;fileidx < 2;fileidx++)
	      {
		size_t path_ender;
		curr = datafiles.at(0);
		path_ender = curr.find_last_of('/');
		if(path_ender != npos)
		  curr.erase(0,path_ender+1);
		curr = ERROR_STAT_DIR + curr;
		curr = boincDB.BOINC_CONFIG_FILE_PATH + curr;
		boinc_copy(datafiles.at(fileidx).c_str(),curr.c_str());//copy data for analysis.
	      }
	  }
	return result;
}


/**
 * Determines the type of calcation of the result and compares the data accordingly.
 */
int compare_results(RESULT& lhs, void* lhs_data,const RESULT& rhs, void* rhs_data, bool& isValid)
{
	using std::string;
	isValid = false;//prove otherwise. guilty until proven innocent.

	//at least make *some* attempt to make sure we didn't give different data types
	//which to compare before casting. Then fork to correct process.
	string lhs_apptype,rhs_apptype,result_name;
	try
	{
	  result_name = lhs.name;
		lhs_apptype = getAppType(result_name);
		result_name = rhs.name;
		rhs_apptype = getAppType(result_name);
		if(lhs_apptype != rhs_apptype || lhs_apptype.size() == 0)
			return INVALID_WORKUNIT;
	}
	catch(GridException ge)
	{
	  std::cerr << "ERROR - " << get_iso8601_time() << std::endl;
	  std::cerr << ge << std::endl;
	  return ge.getErrType();
	}

	//fork
	int result = MISSING_SNAPSHOT;
	if(lhs_apptype == "mmpbsa")
		result = compare_results_mmpbsa(lhs_data,rhs_data);
	if(lhs_apptype == "moledyn")
		result = compare_results_moledyn(lhs_data,rhs_data);

	isValid = (result == GOOD_VALIDATION);
	return result;
}

/**
 * Removes the result data from memory.
 */
int cleanup_result(const RESULT& result, void* data)
{
	if(data == 0)
		return 0;
	std::string apptype,result_name;
	try
	{
	  result_name = result.name;
	  apptype = getAppType(result_name);
	}
	catch(GridException ge)
	{
	  std::cerr << "ERROR - " << get_iso8601_time() << std::endl;
	  std::cerr << ge << std::endl;
	  return ge.getErrType();
	}
	if(apptype == "mmpbsa")
	  delete (std::string*) data;
	else if(apptype == "moledyn")
	  delete (std::string*) data;
	else
	{
	  std::cerr << "ERROR - " << get_iso8601_time() << std::endl;
	  std::cerr << "Could not delete the data of " << result.name << " because its data type is unknown."
		    << std::endl << "Data type: " << ((apptype.size() == 0)? "Unknown" : apptype) << std::endl;
	  return INVALID_WORKUNIT;
	}

	return 0;
}

/**
 * Computes the amount of credit that should be given to the result.
 *
 * Calculation process (per BOINC website):
 * BOINC server suggests a credit amount for each result based on CPU usage.
 * If there is only one result, grant the result's credit.
 * If there are two results, grant the minimum credit.
 * If there are three or more results, average the results' credit, excluding
 * 	the maximum and minimum credit.
 */

double compute_granted_credit(WORKUNIT& wu, std::vector<RESULT>& results)
{
	std::vector<RESULT>::const_iterator it;
	double maxval,minval,curr,avg;
	maxval = minval = 0;
	if(results.size() == 2)
		return std::min(results[0].claimed_credit,results[1].claimed_credit);
	if(results.size() == 1)
		return results[0].claimed_credit;
	if(results.size() == 0)
		return 0;
	for(it = results.begin();it != results.end();it++)
	{
		curr = it->claimed_credit;
		if(curr > maxval)
			maxval = curr;
		if(curr < minval)
			minval = curr;
		avg += curr;
	}
	avg -= maxval + minval;
	avg /= results.size() - 2;
	return avg;
}

#ifdef STANDALONE
int main(int argc,char** argv)
{
	std::cout << "create standalone mode" << std::endl;
	return 1;
}
#endif
