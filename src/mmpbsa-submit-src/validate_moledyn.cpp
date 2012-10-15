/**
 * Validates molecular dynamics results for agreement of data values.
 *
 * Created by David Coss, 2010
 */

#include "libmmpbsa/EnergyInfo.h"
#include "libmmpbsa/mmpbsa_io.h"
#include "libmmpbsa/mmpbsa_exceptions.h"

#include "GridException.h"
#include "validation_states.h"
#include "validate_moledyn_structs.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <valarray>

#ifdef STANDALONE
#include <argp.h>
static char doc[] = "validate_moledyn -- a program that validates Sander mdout file data.";
static char args_doc[] = "FILE1 FILE2 ... FILEN";

static struct argp_option options[] = {
		{"statistics",'s',"FILE",0,"Output variance statistics to FILE instead of standard output."},
		{"verbose",'v',"LEVEL",OPTION_ARG_OPTIONAL,"Produce verbose output. 0 = not verbose, 1 = verbose."},
		{"tolerance",'t',"FILE",0,"Use an option tolerance level. This file should contain 1 block of mdout data which will used as "
				"the maximum value of each data type when the *variance* is calculated."},
		{0}
};

void set_verbosity(struct validate_moledyn_arguments * args, char* arg)
{
	if(arg == 0 || strlen(arg) == 0)
	{
		args->verbose_level = 1;
		return;
	}
	std::cout << "verbosity arg: \"" << arg << "\""<< std::endl;
	std::istringstream buff(arg);
	buff >> args->verbose_level;
	if(buff.fail())
		args->verbose_level = 1;

	return;
}

static error_t parse_opt(int key, char* arg, struct argp_state* state)
{
	struct validate_moledyn_arguments * args = (struct validate_moledyn_arguments*) state->input;

	switch(key)
	{
	case 's':
		args->stats_file = arg;
		break;
	case 'v':
		set_verbosity(args,arg);
		break;
	case 't':
		args->tolerance = arg;
		break;
	case ARGP_KEY_ARG:
		args->mdout_files.push_back(arg);
		break;
	case ARGP_KEY_NO_ARGS:
		argp_usage(state);
	case ARGP_KEY_END:
		if (state->arg_num < 2)
			argp_usage (state);//too few args
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = {options,parse_opt, args_doc, doc};

#endif//standalone

static void print_validation_error(std::ostream& output_stream,std::vector<mmpbsa::EnergyInfo> stats)
{
  using std::endl;
  size_t  size;
  
  if(stats.size() < 2)
    throw GridException("print_validation_error was provided insufficient data",INVALID_INPUT_PARAMETER);
  
  if(stats.at(0).is_minimization)
    {
      output_stream << "The difference between the two results was too large compared to the slope of the energy curve." << endl;
      output_stream << "Difference in final energies:" << endl;
      output_stream << stats.at(0) << endl << endl;
      
      output_stream << "Change in last two energy values for the larger data set" << endl;
      output_stream << stats.at(1) << endl;

      if(stats.at(0).has_converged == mmpbsa::EnergyInfo::UNKNOWN)
	output_stream << "DATA SETS DISAGREE ON CONVERGENCE" << endl;

    }
  else
    {
      // MD will have 4 sets of validation information
      if(stats.size() < 4)
	throw GridException("print_validation_error requires 4 statistics objects: tolerance, average, rms and variance.",INVALID_INPUT_PARAMETER);
      
      size = stats[0].size();
      output_stream << "TOLERANCE:" << endl << stats[0] << endl << endl;
      output_stream << "AVERAGES:" << endl << stats[1] << endl << endl;
      output_stream << "RMS:" << endl << stats[2] << endl;
      output_stream << "VARIANCE:" << endl << stats[3] << endl;
      output_stream << "Outlying data:" << endl;
      for(size_t outlier = 0;outlier < size;outlier++)
	if(stats[3][outlier] > stats[0][outlier])
	  output_stream << "Data #" << outlier << ": " << stats[3][outlier] << endl;
    }
}

/**
 * This function handles the validation of the average data values of the snapshots and the RMS fluctuation of that data as reported by sander.
 */
static std::vector<mmpbsa::EnergyInfo> validate_avg_rms(const std::valarray<mmpbsa::AveRmsEnerInfo>& stated_avgrms, const mmpbsa::AveRmsEnerInfo& tolerance, bool isVerbose)
{
  using mmpbsa::EnergyInfo;
  size_t size = stated_avgrms.size();
  std::vector<mmpbsa::EnergyInfo> returnMe;
  EnergyInfo ratio;

  // When reading this function, be careful of the unfortunate fact that this function
  // finds the average and root mean square of average and root mean square data.
  // The following variable names refer to the data in the mdout file. The array indices
  // refer to the the averages and rms' being calculated here.
  EnergyInfo avg[3],rms[3];// 0 --> average, 1 --> rms, 2 --> variance of avg(average) and rms (rms fluctuation) values in sander mdout files
  
  if(size == 0)
    return returnMe;

  for(size_t idx = 0;idx < size;idx++)
    {
      const EnergyInfo& curravg = stated_avgrms[idx].get_average();
      const EnergyInfo& currrms = stated_avgrms[idx].get_rms();
      avg[0] += curravg;
      avg[1] += curravg*curravg;

      rms[0] += currrms;
      rms[1] += currrms*currrms;
    }
  
  for(size_t idx = 0;idx < 2;idx++)
    {
      avg[idx] /= size;
      rms[idx] /= size;// techinically rms^2

    }
  avg[2] = avg[1];avg[2] -= avg[0]*avg[0];avg[2] = sqrt(avg[2]);
  rms[2] = rms[1];rms[2] -= rms[0]*rms[0];rms[2] = sqrt(rms[2]);
  avg[2] /= abs(avg[0]);
  rms[2] /= abs(rms[0]);
   // Adjust values which are not included (and are therefore zero in the average).
  for(size_t avg_idx = 0;avg_idx < EnergyInfo::total_parameters;avg_idx++)
    {
      if(avg[0][avg_idx] == 0)
	avg[2][avg_idx] = 0;
      if(rms[0][avg_idx] == 0)
	rms[2][avg_idx] = 0;
    }
  
  if(isVerbose)
    {
      std::cout << "Average according to sander" << std::endl;
      std::cout << "AVERAGE " << std::endl;
      std::cout << avg[0] << std::endl<< std::endl;
      std::cout << "RMS" << std::endl;
      std::cout << sqrt(avg[1]) << std::endl << std::endl;
      std::cout << "VARIANCE" << std::endl;
      std::cout << avg[2] << std::endl;
      
      std::cout << "RMS according to sander" << std::endl;
      std::cout << "AVERAGE " << std::endl;
      std::cout << rms[0] << std::endl<< std::endl;
      std::cout << "RMS" << std::endl;
      std::cout << sqrt(rms[1]) << std::endl << std::endl;
      std::cout << "VARIANCE" << std::endl;
      std::cout << rms[2] << std::endl;
    }

#if 0 //decide to base validity on the spread of the averages w.r.t. the RMS fluctuation.
  if(avg[2] > tolerance.get_average())
    {
      returnMe.push_back(tolerance.get_average());
      returnMe.push_back(avg[0]);
      returnMe.push_back(sqrt(avg[1]));
      returnMe.push_back(avg[2]);
      return returnMe;
    }

  if(rms[2] > tolerance.get_rms())
    {
      returnMe.push_back(tolerance.get_rms());
      returnMe.push_back(rms[0]);
      returnMe.push_back(sqrt(rms[1]));
      returnMe.push_back(rms[2]);
      return returnMe;
    }
  return returnMe;
#else
  ratio = avg[1];
  ratio -= avg[0]*avg[0];
  ratio = sqrt(ratio);
  ratio /= rms[0];
  if(ratio > tolerance.get_rms())
    {
      returnMe.push_back(tolerance.get_rms());
      returnMe.push_back(avg[0]);
      returnMe.push_back(sqrt(avg[1]));
      returnMe.push_back(ratio);
      return returnMe;
    }
#endif
}

std::vector<mmpbsa::EnergyInfo> validate_minimization(const std::vector<mmpbsa::EnergyInfo>& last_energy,const std::vector<mmpbsa::EnergyInfo>& second_to_last_energy)
{
  // d_last represents the difference between the last energy values of *the first two files*. 
  // slope represents the largest change in energies *for each file*
  //
  // Then validity is based on 3*d_last < slope OR if there is a disagreement of whether or not convergence occured.
  using mmpbsa::EnergyInfo;
  EnergyInfo d_last, slope, tmp;
  std::vector<EnergyInfo> retval;
  size_t idx, end = EnergyInfo::total_parameters;
  if(last_energy.size() < 2 || second_to_last_energy.size() < 2)
    throw GridException("validate_minimization: Two files are require for validation of minimization results.",INVALID_INPUT_PARAMETER);
  
  tmp = last_energy.at(0);
  tmp -= last_energy.at(1);
  d_last = abs(tmp);
  
  idx = (last_energy.at(0)[EnergyInfo::etot] > last_energy.at(1)[EnergyInfo::etot]) ? 0 : 1;
  tmp = second_to_last_energy.at(idx);
  tmp -= last_energy.at(idx);
  slope = abs(tmp);

  if(last_energy.at(0).has_converged != last_energy.at(1).has_converged)
    {
      d_last.has_converged = mmpbsa::EnergyInfo::UNKNOWN;
      slope.has_converged = mmpbsa::EnergyInfo::UNKNOWN;
      retval.push_back(d_last);
      retval.push_back(slope);
      return retval;// Disagreement on convergence; error data is returned.
    }

  idx = 0;
  if(3*d_last[EnergyInfo::etot] > slope[EnergyInfo::etot])
    {
      d_last.has_converged = slope.has_converged = last_energy.at(0).has_converged;
      retval.push_back(d_last);
      retval.push_back(slope);
      return retval;// Invalid, error data is returned.
    }
  
  return retval;
}

/**
 * Checks to see if the variance is within the supplied tolerance. If it is
 * an empty vector is returned. Otherwise, a vector containing the average,
 * rms and variance is returned.
 * 
 * @param input_filenames
 * @param numberOfSnaps
 * @param tolerance Tolerance of snapshot data
 * @param avg_tolerance Tolerance of reported average and rms data
 * @param isVerbose bool determining if the function will report its data
 * @return If valid, empty vector. Else, vector of size 3, corresponding to average, rms and variacne in that order.
 */
std::vector<mmpbsa::EnergyInfo> validateMoledyn(const std::vector<std::string>& input_filenames,
						const size_t& numberOfSnaps,
						const mmpbsa::EnergyInfo& tolerance, const  mmpbsa::AveRmsEnerInfo& avg_tolerance,
						bool isVerbose) throw (GridException,mmpbsa::MMPBSAException)
{
    using mmpbsa::EnergyInfo;
    using mmpbsa::AveRmsEnerInfo;
    using namespace std;

    vector<EnergyInfo> returnMe;
    EnergyInfo zero;
    valarray<EnergyInfo> avgs,rms_sqrdes;
    valarray<AveRmsEnerInfo> stated_avgrms;// values amber gives for average and rms fluctuation over snap shots. These should be validated differently than regular data
    size_t numberOfFiles = input_filenames.size();
    vector<EnergyInfo> last_energy, second_to_last_energy;// In case of minimization
    bool is_minimization = false, first_converged, second_converged;
    
    // Iterate through files and snapshots, collecting energy data.
    for(vector<string>::const_iterator filename = input_filenames.begin();
            filename != input_filenames.end();filename++)
    {
    	fstream data(filename->c_str(),ios::in);
    	if(!data.good())
            throw GridException("Could not open mdout file: " + *filename,IO_ERROR);
        
        for(size_t i = 0;i<numberOfSnaps;i++)
	  {
	    EnergyInfo curr;

	    if(data.eof())
	      {
            	throw GridException("Data ended abruptly in mdout file: " + *filename,IO_ERROR);
	      }
	    try{
	      int load_retval;
	      curr.clear();
	      load_retval = curr.get_next_energyinfo(data);
	      if(load_retval == mmpbsa::UNEXPECTED_EOF && numberOfSnaps - 1 == i)
		break;


	      if(load_retval != 0)
		{
		  std::ostringstream error;
		  error << "validateMoledyn: Error loading the " << i+1 << "-th (1-indexed) snapshot of " << *filename << ". Error Code: " << load_retval;
		  throw mmpbsa::SanderIOException(error,(mmpbsa::MMPBSAErrorTypes)load_retval);
		}
	    }
	    catch(mmpbsa::SanderIOException sioe)
	      {
		std::ostringstream error;
		error << "validate_moledyn: Could not read " << *filename << std::endl;
		error << "Reason : " << sioe;
		throw GridException(error);
	      }
	    
	    // Minimization and MD are validated differently.
	    // Therefore, storage of energy data is handled 
	    // differently as well.
	    if(curr.is_minimization)
	      {
		size_t file_index = std::distance(input_filenames.begin(),filename);
		is_minimization = true;
		if(filename == input_filenames.begin())
		  first_converged = curr.has_converged;
		else
		  second_converged = curr.has_converged;

		// With minimization, there is no average or RMS. The last NSTEP
		// entry is a repeat of the FINAL energy value. Thus it should
		// be ignored. HOWEVER, reading it does set 
		// the convergence flag.
		if(i + 1 == numberOfSnaps)
		  break;	      

		if(last_energy.size() == 0)
		  {
		    last_energy.resize(numberOfFiles);
		    second_to_last_energy.resize(numberOfFiles);
		  }
		second_to_last_energy.at(file_index) = last_energy.at(file_index);
		last_energy.at(file_index) = curr;
	      }
	    else
	      {
		if(avgs.size() == 0)
		  {
		    avgs.resize(numberOfSnaps,zero);//default EnergyInfo: all values are zero.
		    rms_sqrdes.resize(numberOfSnaps);
		    stated_avgrms.resize(numberOfFiles);
		  }
		avgs[i] += curr/((mmpbsa_t)numberOfFiles);
		rms_sqrdes[i] += curr*curr/((mmpbsa_t) numberOfFiles);
	      }
	  }
	
	// Get average and RMS data. If this is a minimization, however,
	// there will be no average and RMS data.
	if(!is_minimization && !data.eof())
	  {
	    int load_retval;
	    vector<string>::difference_type curr_idx = std::distance(input_filenames.begin(),filename);
	    if(curr_idx < 0 || curr_idx > stated_avgrms.size())
	      {
		std::ostringstream error;
		error << "validateMoledyn: improper size of Sander Average data containter" << std::endl;
		error << "Number of Files: " << input_filenames.size() << "\tContainer size: " << stated_avgrms.size();
		throw GridException(error,INVALID_INPUT_PARAMETER);
	      }
	    load_retval = stated_avgrms[curr_idx].get_avg_rms_info(data);
	    if(load_retval != 0)
	      {
		std::ostringstream error;
		error << "validateMoledyn: Error loading the fluctuation data from " << *filename << ". Error Code: " << load_retval;
		throw mmpbsa::SanderIOException(error,(mmpbsa::MMPBSAErrorTypes)load_retval);	      
	      }
	  }
        data.close();
    }

    // Validation is difference for minimization and MD
    // Validate accordingly
    if(last_energy.size() != 0)
      {
	// Minimization
	last_energy.at(0).has_converged = second_to_last_energy.at(0).has_converged = first_converged;
	last_energy.at(1).has_converged = second_to_last_energy.at(1).has_converged = second_converged;

	returnMe = validate_minimization(last_energy,second_to_last_energy);
      }
    else
      {
	// MD
	returnMe = validate_avg_rms(stated_avgrms,avg_tolerance,isVerbose);
      }
    return returnMe;
}

/**
 * When provided files names for data files, validateMoledynWorker will
 * parse the data files and validate them using validateMoledyn.
 * If the result is invalid but data was compared, the allowed tolerance, average, RMS, and variance are
 * returned, respectively.
 *
 * validationState is set equal to the resulting state, cf validation_states.h
 *
 * The stream to which validation statistics (if any) are sent is indicated
 * by output_filename. If this is a null string, statistics are sent to stdout.
 * Otherwise, they are sent to a file with the name stored in output_filename.
 *
 */
std::vector<mmpbsa::EnergyInfo> validateMoledynWorker(validate_moledyn_arguments& arguments, int& validationState)
{
	using namespace std;
	using namespace mmpbsa_io;
	using mmpbsa::EnergyInfo;
	using mmpbsa::AveRmsEnerInfo;
	vector<mmpbsa::EnergyInfo> returnMe;
	std::ofstream out_file;
	std::ostringstream output_stream;
	vector<string>& input_filenames = arguments.mdout_files;
	vector<string>::const_iterator input_file;
	validationState = EMPTY_SET;//invalid until proven valid.

	try{

		size_t numberOfSnaps = 0;
		string currLine;	
		
		// Verify that all input files have the same number of snapshots
		fstream input;
		input_file = input_filenames.begin();
		for(;input_file != input_filenames.end();input_file++)
		  {
		    size_t current_numberOfSnaps = 0;
		    input.open((*input_file).c_str(),std::ios::in);
		    if(!input.is_open())
		      {
			fprintf(stderr,"validateMoledynWorker: Could not open %s.\n",(*input_file).c_str());
			validationState = VALIDATOR_IO_ERROR;
			return returnMe;
		      }		      
		    while(!input.eof())
		      {
			currLine = getNextLine(input);
			if(currLine.find(MDOUT_AVERAGE_HEADER) != string::npos)
			  break;//end of snapshot list
			if(currLine.find("NSTEP") != string::npos)
			    current_numberOfSnaps++;
		      }
		    if(input_file == input_filenames.begin())
		      numberOfSnaps = current_numberOfSnaps;
		    else if(numberOfSnaps != current_numberOfSnaps)
		      {
			// current file does not have the same number of snapshots as the first 
			// file. That is invalid.
			fprintf(stderr,"MD output files have a different number of snapshots.\n");
			validationState = UNEQUAL_NUMBER_OF_SNAPSHOTS;
			return returnMe;
		      }
		    input.close();
		  }

		if(numberOfSnaps == 0)
		{
			std::cerr << "validate_moledyn: Empty file: " << input_filenames[0] << std::endl;
			validationState = EMPTY_FILE;
			return returnMe;
		}

		EnergyInfo tolerance;
		AveRmsEnerInfo avg_tolerance;
		if(arguments.tolerance.size() > 0)
		  {
		    std::fstream tolerance_file(arguments.tolerance.c_str(),std::ios::in);
		    if(tolerance.get_next_energyinfo(tolerance_file) != 0)
		      throw GridException("Invalid tolerance file: " + arguments.tolerance,IO_ERROR);
		    if(!tolerance_file.eof())
		      {
			try{
			  if(avg_tolerance.get_avg_rms_info(tolerance_file) != 0)
			    throw GridException("Invalid tolerance file: " + arguments.tolerance,IO_ERROR);
			}
			catch(mmpbsa::MMPBSAException mmpbsae)
			  {
			    std::ostringstream error;
			    error << "Error loading Average/RMS tolerance data from " << arguments.tolerance << "\nError Message:\n";
			    error << mmpbsae;
			    throw mmpbsa::MMPBSAException(error,mmpbsae.getErrType());
			  }
		      }
		  }

		vector<EnergyInfo> avg_rms = validateMoledyn(input_filenames,numberOfSnaps,tolerance,avg_tolerance,arguments.verbose_level);
		if(avg_rms.size() == 0)
		{
			output_stream << "Files are valid." << endl;
			validationState = GOOD_VALIDATION;
		}
		else
		{
		  returnMe = avg_rms;
		  print_validation_error(output_stream,returnMe);
		  validationState = BEYOND_TOLERANCE;
		}
	}
	catch(mmpbsa::MMPBSAException me)
	{
		cerr << me << endl;
		validationState = me.getErrType();
	}

	if(arguments.stats_file.size() > 0)
	{
		out_file.open(arguments.stats_file.c_str(),std::ios::out);
		if(out_file.good())
		{
			out_file << output_stream.str();
			out_file.close();
		}
		else
			cout << output_stream.str() << endl;
	}
	else
		cout << output_stream.str() << endl;


	return returnMe;
}

#ifdef STANDALONE

int main(int argc, char** argv)
{
    using namespace std;
    using mmpbsa::EnergyInfo;
    using mmpbsa_io::getNextLine;
    
    struct validate_moledyn_arguments args;
    args.stats_file = "";
    args.tolerance = "";
    args.verbose_level = 0;

    argp_parse(&argp,argc,argv,0,0,&args);

    int validationState;
    try{
    	validateMoledynWorker(args,validationState);
    }
    catch(GridException ge)
    {
    	std::cerr << ge << std::endl;
    	return ge.getErrType();
    }
    catch(mmpbsa::MMPBSAException mmpbsae)
    {
    	std::cerr << mmpbsae << std::endl;
    	return mmpbsae.getErrType();
    }
    return validationState;
}
#endif
