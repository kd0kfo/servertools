/**
 * combine_sander_data
 * Functions that are used to combine sander output files from serial processing.
 *
 * Created by David Coss, 2010
 */
#include "libmmpbsa/EnergyInfo.h"

#include "GridException.h"

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <cerrno>
#include <cstring>


/**
 * Takes a list of filenames and combines the energy data by loading it into an EnergyInfo
 * object and then sending the EnergyInfo object data to an fstream using EnergyInfo::operator<<
 * The last EnergyInfo object is returned.
 */
mmpbsa::EnergyInfo combine_sander_energy_data(const std::vector<std::string>& input_filenames,
        const std::string& output_filename,const size_t& numberOfSnaps) throw (GridException,mmpbsa::MMPBSAException)
{
    using std::vector;
    using std::string;

    mmpbsa::EnergyInfo energy_data;

    std::fstream output(output_filename.c_str(),std::ios::out | std::ios::app);
    if(!output.good())
        throw GridException("Could not open mdout file (for writing): "+output_filename,IO_ERROR);

    std::fstream input;
    bool first = true;

    for(vector<string>::const_iterator filename = input_filenames.begin();
            filename != input_filenames.end();filename++)
    {
        std::cout << "reading " << *filename << std::endl;
        input.clear();
        input.open(filename->c_str(),std::ios::in);
        if(!input.good())
            throw GridException("Could not open mdout file: "+output_filename,IO_ERROR);

        first = true;
        for(size_t i = 0;i<numberOfSnaps;i++)
        {
            input >> energy_data;
            if(first && filename != input_filenames.begin())
            {
                std::cerr << "Continuity check needed in combine_energy_data" << std::endl;
            }
            output << energy_data << std::endl << std::endl;
            if(input.eof())
                i = numberOfSnaps;
        }

        input.close();
    }

    output.close();
    return energy_data;
}

/**
 * Loads files listed in input_files and adds them to an output file specified by output_filename.
 * Input files are simply concatenated in the order in which they appear in the vector.
 *
 * Note: if a file with a name equal to the string output_filename already exists, it is overwritten
 * with the data in the input files.
 */
void cat(const std::vector<std::string>& input_files,
	 const std::string&	output_filename, bool skip_title) throw (GridException)
{
  int retval = 0;
  size_t num_read = 0;
  size_t num_write = 0;
  unsigned char *filebuff;
  const size_t file_buff_size = 4096;
  std::vector<std::string>::const_iterator input_file;
  
  FILE *source, *destination;

  if(input_files.size() == 0 || output_filename.size() == 0)
    throw GridException("combine_sander_data::cat: need at least one input file and an output filename.",INVALID_INPUT_PARAMETER);

  // Allocate a copy buffer
  filebuff = (unsigned char*)calloc(file_buff_size, sizeof(unsigned char));
  if (filebuff == NULL) {
    std::ostringstream error;
    error << "ERROR: Failed to allocate " << file_buff_size << " bytes for file copy buffer!\nReason: " << strerror(errno);
    throw GridException(error,IO_ERROR);
  }

  // Open the destination file
  destination = fopen(output_filename.c_str(),"w+b");
  if (destination == NULL) {
     std::ostringstream error;
     error << "ERROR: Failed to open source file (" << output_filename << ") for file copy!\nReason:" << strerror(errno);
     throw GridException(error,IO_ERROR);
  }

  input_file = input_files.begin();
  for(;input_file != input_files.end();input_file++)
    {
      // Open the source file
      source = fopen(input_file->c_str(),"rb");
      if (source == NULL) {
	std::ostringstream error;
	error << "Could not open source file (" << *input_file << ")\nReason: " << strerror(errno);
	throw GridException(error,IO_ERROR);
      }
      
      if(skip_title && input_file != input_files.begin())
	{
	  char *dummy_array = NULL;
	  size_t dummy_size;
	  getline(&dummy_array,&dummy_size,source);
	  free(dummy_array);
	}
      
      // Copy the file
      while (!feof(source)) {
	num_read  = fread(filebuff,sizeof(unsigned char),file_buff_size,source);
	if (num_read == 0) {
	  // Need to check feof and ferror here for proper error checking
	  std::ostringstream error;
	  error << "ERROR: Read of source file (" << *input_file << ") failed during file copy!";
	  throw GridException(error,IO_ERROR);
	}
	num_write = fwrite(filebuff,sizeof(unsigned char),num_read,destination);
	if (num_read != num_write) {
	  // Need to check feof and ferror here for proper error checking
	  std::ostringstream error;
	  error << "ERROR: Write to destination file (" << output_filename << ") failed during file copy!";
	  throw GridException(error,IO_ERROR);
	}
      }
      fflush(destination);
      fsync(fileno(destination));
      fclose(source);
    }

  fclose(destination);
  // Free the transfer buffer and set it to NULL to prevent memory leaks
  free(filebuff);
  filebuff = NULL;
  // We're done

}

/**
 * Function that decides which function should be used to combine energy data based on the
 * value of file_type.
 */
int sander_combine_worker(const std::vector<std::string>& input_filenames,
		const std::string& output_filename,const std::string& file_type)
{
  using std::cerr;
  bool skip_title = false;
	
	
	if(input_filenames.size() < 1)
	{
		std::cerr << "sander_combine_worker requires at least one file to combine, but " << input_filenames.size()
				<< " were given." << std::endl;
		return 1;
	}

	// Do we need to carry over the title in the file?
	skip_title = (file_type == "mdcrd");
	
	// Fork combination procedure
	if(file_type == "mdout_clean")
	{
		std::vector<std::string> first_file;
		first_file.push_back(input_filenames.at(0));
		std::vector<std::string> the_rest;
		if(input_filenames.begin()+1 != input_filenames.end())
			the_rest = std::vector<std::string>(input_filenames.begin()+1,input_filenames.end());

		try
		{
			combine_sander_energy_data(first_file,output_filename,2);
			if(the_rest.size() != 0)
				combine_sander_energy_data(the_rest,output_filename,1);
		}
		catch(GridException ge)
		{
			cerr << "sander_combine_worker: " << ge.identifier() << ": " << ge.what() << std::endl;
			return ge.getErrType();
		}
		catch(mmpbsa::MMPBSAException me)
		{
			cerr << "sander_combine_worker: " << me.identifier() << ": " << me.what() << std::endl;
			return me.getErrType();
		}
	}
	else
	{
		try{
		  cat(input_filenames,output_filename,skip_title);
		}
		catch(GridException ge)
		{
			cerr << "sander_combine_worker: " << ge.identifier() << ": " << ge.what() << std::endl;
			return ge.getErrType();
		}
	}

	return 0;
}

#ifdef STANDALONE_COMBINE_SANDER_DATA

#include <unistd.h>

static const char shortopts[] = "t:";

/**
 * fills a vector with argv[1..argc-2] and calls sander_combine_worker
 */
int main(int argc, char** argv) {

    using namespace std;
    using mmpbsa::EnergyInfo;
    
    int optflag;
    string output_filename = argv[argc-1];
    vector<string> input_filenames;
    string file_type = "";
    
    if(argc < 3)
    {
        cout << "usage: combine_sander_data <data1> ... <data N> <output file>" << endl;
        return 0;
    }

    while((optflag = getopt(argc,argv,shortopts)) != -1)
      {
	switch(optflag)
	  {
	  case 't':
	    file_type = optarg;
	    break;
	  default:
	    fprintf(stderr,"Unknown flag: %c",optflag);
	    exit(-1);
	    break;
	  }
      }

    for(int i = optind;i<argc-1;i++)
    	input_filenames.push_back(argv[i]);
    output_filename = argv[argc-1];
    
    return sander_combine_worker(input_filenames,output_filename,file_type);

}
#endif//standalone




