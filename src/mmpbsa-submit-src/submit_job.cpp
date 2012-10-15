#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "globals.h"

#include "GridException.h"
#include "MDParams.h"

#include "libmmpbsa/XMLNode.h"
#include "libmmpbsa/XMLParser.h"
#include "libmmpbsa/mmpbsa_utils.h"
#include "libmmpbsa/mmpbsa_io.h"
#include "libmmpbsa/SanderParm.h"
#include "libmmpbsa/StringTokenizer.h"

#ifdef USE_BOINC
//boinc in sched directory
#include "boinc/filesys.h"
#include "boinc/sched_config.h"
#include "boinc/sched_util.h"
#endif


#include "submit_job.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include <ctime>
#include <unistd.h>
#include <sys/types.h>//at least for getting the UID of the submitting user.
#include <sys/stat.h>//for chmod, at least
#include <errno.h>


std::string get_iso8601_time(){return mmpbsa_utils::get_iso8601_time();}

/**
 * Tries to convert the provided string into an index of the file list. If it can do that,
 * the grid_file_info structure is returned. If not, a structure (of a perminant input file)
 * is returned, which has a filename equal to index.
 */
grid_file_info grid_resolve_file_info(const std::map<std::string,grid_file_info>& file_list, const std::string& index)
{
	if(file_list.find(index) == file_list.end())
	{
		grid_file_info returnMe;
		returnMe.filename = index;
		returnMe.filename_alias = "";
		returnMe.filetype = file_types::perminant_input;
		return returnMe;
	}
	return file_list.find(index)->second;
}


/**
 * Tries to convert the provided string into an index of the file list. If it can do that,
 * the filename of the grid_file_info structure is returned. If it cannot, index itself
 * is returned. This allows submit_job to try to place the file in the file list. If it can't
 * the file name is carried along instead of an index. Then this function is called to resolve
 * the index->filename relationship, if that has been used.
 */
std::string grid_resolve_filename(const std::map<std::string,grid_file_info>& file_list, const std::string& index)
{
	return grid_resolve_file_info(file_list, index).filename;
}


/**
 * Maps known program names to the name of the executable on the grid. If a program is to
 * be added to the grid system, it must be added to the map in this routine.
 *
 * Map:
 * What the user thinks the program is  --->  What the grid thinks the program is
 */
std::map<std::string, std::string> knownApplications()
{
    std::map<std::string, std::string> returnMe;
    returnMe["moledyn"] = "moledyn";
    returnMe["sander"] = returnMe["moledyn"];
    returnMe["mmpbsa"] = "mmpbsa";
    returnMe["recombine_moledyn"] = "recombine_moledyn";
    returnMe["recombine_mmpbsa"] = "recombine_mmpbsa";
    returnMe["extract_traj"] = "extract_traj";
    returnMe["clean_traj"] = "clean_traj";
    return returnMe;
}

/**
 * Maps command line argument flags (i.e. "-i") to the
 * file type and tag name in mmpbsa queue XML files (i.e. "mdin").
 * Tag names (generally) follow those listed in the command line
 * document of Sander in the Amber 11 manual.
 *
 * Map:
 * Command line flags (often one letter) ----> Queue XML tags
 *
 */
std::map<std::string, std::string> knownParameters()
{
    std::map<std::string, std::string> returnMe;
    returnMe["i"] = "mdin";
    returnMe["o"] = "mdout";
    returnMe["p"] = "prmtop";
    returnMe["c"] = "inpcrd";
    returnMe["r"] = "restrt";
    returnMe["ref"] = "refc";
    returnMe["x"] = "mdcrd";
    returnMe["v"] = "mdvel";
    returnMe["e"] = "mden";
    returnMe["inf"] = "mdinfo";
    returnMe["traj"] = "traj";
    returnMe["top"] = "top";
    returnMe["radii"] = "radii";
    returnMe["cpin"] = "cpin";
    returnMe["cpout"] = "cpout";
    returnMe["cprestrt"] = "cprestrt";
    returnMe["mmtsb"] = "mmtsb";
    
    returnMe["snap_list"] = "snap_list";
    return returnMe;
}

/**
 * For the known parameters list, determines 
 * whether or not the file type is an output
 * type. If the filetype is unknow it is assumed
 * to be an output file.
 */
bool isOutput(const std::string& filetype)
{
  if(filetype == "mdin")
    return false;
  if(filetype == "mdout")
    return true;
  if(filetype == "mdinfo")
    return true;
  if(filetype == "prmtop" || filetype == "top")
    return false;
  if(filetype == "inpcrd" || filetype == "traj")
    return false;
  if(filetype == "refc")
    return false;
  if(filetype == "mdcrd")
    return true;
  if(filetype == "mdvel")
    return true;
  if(filetype == "mden")
    return true;
  if(filetype == "restrt")
    return true;
  if(filetype == "inpdip")
    return false;
  if(filetype == "rstdip")
    return true;
  if(filetype == "radii")
    return false;

  //do not support these types of simulation on grid
  //should be blocked by barred_sander_parameters()
  if(filetype == "cpin" )
    return false;
  if(filetype == "cprestrt")
    return false;
  if(filetype == "cpout")
    return true;

  //Parameters that are not files at all, but could be found in 
  //the queue xml tree.
  if(filetype == "snap_list")
    return false;
  if(filetype == "gflop")
    return false;

  return true;
}

/**
 * Some parameters (generally listed in mdin files) are not supported
 * on the grid system. A vector of those parameters are produced here.
 */
std::vector<std::string> barred_sander_parameters()
{
    std::vector<std::string> returnMe;
    returnMe.push_back("cprestrt");//cannot do constant pH
    returnMe.push_back("cpout");
    returnMe.push_back("cpin");
    returnMe.push_back("icnstph");
    returnMe.push_back("inpdip");
    returnMe.push_back("rstdip");
    returnMe.push_back("evbin");
    return returnMe;
}

/**
 * Determines if a name is listed in the given map. If it is not, false is returned.
 * If it is a key value, name is set to the key's object value and true is returned.
 */
bool resolveProcessName(const std::map<std::string,std::string>& known_names, std::string& name)
{
    if(known_names.find(name) == known_names.end())
        return false;
    name = known_names.find(name)->second;
    return true;
}

/**
 * Determines if a process is contained in the given map. If it is not, false is returned.
 * If it is, name is set equal to its object value and true is returned.
 */
bool resolveParameterName(const std::map<std::string, std::string>& parameters, std::string& name)
{
    return resolveProcessName(parameters, name);
}

/**
 * Parses a list of commands to be run within the grid system. Based on the commands
 * given in the istream, a XML queue list is produced, which can later be parallelized.
 */
mmpbsa_utils::XMLNode* parse_job_submission(std::istream& input,std::map<std::string,grid_file_info>& file_list)
{
    using mmpbsa_utils::XMLNode;
    using std::string;
    string arg,lastarg;
    lastarg = "david was here";
    XMLNode* currNode = 0;
    XMLNode* currParameter = 0;
    std::map<std::string, std::string> params = knownParameters();
    std::map<std::string, std::string> apps = knownApplications();

    while(input.good())
    {
    	input >> arg;

    	//ignore lines that begin with #
    	if(arg.size() == 0 || mmpbsa_utils::trimString(arg).at(0) == '#')
    	{
    		getline(input,arg);
		arg = "";
    		continue;
    	}
    	if(arg.size() >= 2 && arg.substr(0,2) == "--")
    		continue;

    	if(arg == lastarg)
    		continue;//in case there is a weird duplicate
    	lastarg = arg;


    	if(resolveProcessName(apps,arg))
    	{
    		if(currNode == 0)
    			currNode = new XMLNode(MMPBSA_QUEUE_TITLE);

    		while(currNode->parent != 0)
    			currNode = currNode->parent;
    		XMLNode* newProcess = new XMLNode(arg);
    		currNode->insertChild(newProcess);
    		currNode = newProcess;
    		continue;
    	}

    	if(currNode == 0)
    	{
	  std::cerr << "Application name was not given. Instead have " << arg << std::endl;
	  delete currNode;
	  delete currParameter;
	  return 0;
    	}

    	if(arg[0] == '-')
    	{
    		if(arg.find("-help") != std::string::npos || arg == "-O")
    		{
    			std::cerr << "Help and override have been disabled." << std::endl;
    			continue;
    		}
    		arg.erase(0,1);

    		if(!resolveParameterName(params,arg))
    			std::cerr << "Warning: unknown parameter " << arg << std::endl;

    		if(currParameter != 0)
    			currNode->insertChild(currParameter);

    		currParameter = new XMLNode(arg);
    		continue;
    	}

        if(currParameter == 0)
            currParameter = new XMLNode();

        if(currParameter->getName() == "")
        {
            resolveParameterName(params,arg);
            currParameter->setName(arg);
        }
        else if(currParameter->getName() == "snap_list")
        {
        	currParameter->setText(arg);
        }
        else
        {
        	grid_file_info curr_file;
        	std::ostringstream index_buff;
        	index_buff << file_list.size();
        	curr_file.filename = arg;
        	curr_file.filetype = (isOutput(currParameter->getName())) ? file_types::perminant_output : file_types::perminant_input;
        	curr_file.filename_alias = arg;
            currParameter->setText(index_buff.str());
            file_list[index_buff.str()] = curr_file;
        }

        currNode->insertChild(currParameter);
        currParameter = 0;
    }

    while(currNode != 0 && currNode->parent != 0)
        currNode = currNode->parent;

    return currNode;
}


/**
 * Replaces a serial XML queue with a parallel queue.
 */
void replace_serial_queue(mmpbsa_utils::XMLNode* parallelizedNode, mmpbsa_utils::XMLNode* serial_queue)
{
    using mmpbsa_utils::XMLNode;
    //replace serial_queue XMLNode with the new parallelizedNode.
    XMLNode* parent = serial_queue->parent;
    //attach new parallel stuff
    XMLNode* it = parent->children;
    if(it == serial_queue)//in case serial_queue is first
    {
        parent->children = parallelizedNode->children;
        it = parent->children;
    }
    else
    {
        while(it->siblings != serial_queue)
        {
            it = it->siblings;
        }
        it->siblings = parallelizedNode->children;
        it = it->siblings;
    }

    //detach old serial_queue (carefully)
    while(it->siblings != 0)
    {
        it->parent = parent;
        it = it->siblings;
    }
    it->parent = parent;
    it->siblings = serial_queue->siblings;
    serial_queue->parent = 0;
    serial_queue->siblings = it->siblings;//now deleting. won't remove siblings we want to keep.
    delete serial_queue->children;
    serial_queue->children = 0;
}


void recombine_mmpbsa_results(mmpbsa_utils::XMLNode* parallelizedNode, 
			      std::vector<grid_file_info>& recombination_list, int& process_index, 
			      std::map<std::string,grid_file_info>& file_list, struct submit_job_arguments& args)
{    
  using mmpbsa_utils::XMLNode;

  if(recombination_list.size() > 0)
    {
      XMLNode* newNode;
      grid_file_info new_file;
      std::ostringstream recombo_id,counter_buff;
      recombo_id << process_index++;
      newNode = new XMLNode("recombine_mmpbsa");
      newNode->insertChild(new XMLNode("prereq",recombo_id.str()));
      recombo_id.str("");recombo_id << process_index;
      newNode->insertChild(new XMLNode("id",recombo_id.str()));
      for(size_t i = 0;i<recombination_list.size();i++)
    	{
	  new_file = recombination_list[i];
	  if(new_file.filename_alias.size() == 0)
	    new_file.filename_alias = new_file.filename;
	  if(new_file.filename_alias.find_last_of(FIELD_DELIMITER) != std::string::npos && new_file.filename_alias.find_last_of(FIELD_DELIMITER) > new_file.filename_alias.find_last_of('.'))
	    new_file.filename_alias.erase(new_file.filename_alias.find_last_of(FIELD_DELIMITER));
	  new_file.filetype = file_types::perminant_output;
	  counter_buff.str("");counter_buff << file_list.size();
	  file_list[counter_buff.str()] = new_file;
	  newNode->insertChild(new XMLNode("recombination_input",counter_buff.str()));
    	}
      parallelizedNode->insertChild(newNode);
      args.previous_process = process_index;
    }

}


void insert_traj_extraction(mmpbsa_utils::XMLNode *parallelizedNode, std::string& snaplist, grid_file_info trajectory_fileinfo, int& parent_process, std::ostringstream& prereq, int& process_index, struct submit_job_arguments& args)
{
  mmpbsa_utils::XMLNode *extraction_node = NULL;
  std::ostringstream arg_buf;
  std::string output_path = "";

  if(parallelizedNode == NULL)
    return;
  
  extraction_node = new mmpbsa_utils::XMLNode("extract_traj");
  if(parent_process >= 0)
    {
      prereq << parent_process;
      extraction_node->insertChild("prereq",prereq.str());
    }
  prereq.clear();prereq.str("");
  ++args.previous_process;
  prereq << args.previous_process;
  process_index = args.previous_process;
  extraction_node->insertChild("id",prereq.str());
		
  arg_buf << " --natoms " << args.natoms;
		
  if(args.ifbox > 0)
    arg_buf << " --periodic";
		
  arg_buf << " --frames " << snaplist;

  if(trajectory_fileinfo.filename_alias.size() == 0)
    throw GridException("insert_traj_extraction: Input filename was given for trajectory extraction.",INVALID_INPUT_PARAMETER);

  if(trajectory_fileinfo.subdirectory.size() != 0)
    {
      output_path = trajectory_fileinfo.subdirectory;
      if(*(output_path.end()-1) != '/')
	output_path += "/";
    }
  output_path += trajectory_fileinfo.filename;

  arg_buf << " --input " << trajectory_fileinfo.filename_alias << " --output " << output_path;
  
  extraction_node->insertChild("arguments",arg_buf.str());
  parallelizedNode->insertChild(extraction_node);
}

void insert_traj_cleanup(mmpbsa_utils::XMLNode *parallelizedNode, grid_file_info trajectory_fileinfo, int& parent_process, std::ostringstream& prereq, int& process_index, struct submit_job_arguments& args)
{
  mmpbsa_utils::XMLNode *cleanup_node = NULL;
  std::string filepath = "";
  if(parallelizedNode == NULL)
    return;

  if(trajectory_fileinfo.filename_alias.size() == 0)
    throw GridException("insert_traj_cleanup: Input filename was given for trajectory extraction.",INVALID_INPUT_PARAMETER);

  cleanup_node = new mmpbsa_utils::XMLNode("clean_traj");

  if(trajectory_fileinfo.subdirectory.size() != 0)
    {
      filepath = trajectory_fileinfo.subdirectory;
      if(*(filepath.end()-1) != '/')
	filepath += "/";
    }
  filepath += trajectory_fileinfo.filename;
  
  cleanup_node->insertChild("arguments",filepath);
  
  if(parent_process >= 0)
    {
      prereq.clear();prereq.str("");
      prereq << parent_process;
      cleanup_node->insertChild("prereq",prereq.str());
    }
  prereq.clear();prereq.str("");
  ++args.previous_process;
  prereq << args.previous_process;
  process_index = args.previous_process;
  cleanup_node->insertChild("id",prereq.str());

  parallelizedNode->insertChild(cleanup_node);
}

/**
 * Takes a serial XML queue for an mmpbsa process and splits it into parallel segments based
 * on the snapshot list provided in snaps, where snaps is a vector containing a ONE-INDEXED
 * listed of snap shots to be run on the grid. This list is divided into segments of no more
 * than three snapshots. These may be run in parallel, as they results are independent of each
 * other.
 *
 * Returns an index of the last process in the parallelized queue. This can be used for dependency
 * in next queue items. If the value is -1, there are no dependencies.
 */
int branch_mmpbsa(mmpbsa_utils::XMLNode* parallelizedNode,
        const mmpbsa_utils::XMLNode* serial_queue, const std::vector<size_t>& snaps,
        std::map<std::string,grid_file_info>& file_list,
        struct submit_job_arguments& args)
{
  using mmpbsa_utils::XMLNode;
  static const size_t max_snaps_per_branch = MMPBSA_MAX_SNAPSHOTS;
  int process_index, parent_process;
  std::string snaplist = "", extractionlist;
  std::string name,text;
  grid_file_info extracted_trajfile;
  std::ostringstream snap_buff,extraction_buff,counter_buff,prereq,idbuff;
  size_t counter = 0,extraction_mask = 0, offset = 0;
  size_t last_snap = 0;
  bool insertedSnaplist = false;
  std::vector<grid_file_info> recombination_list;
  
  parent_process = args.previous_process;
  prereq << args.previous_process;
  process_index = args.previous_process;

  // Iterate through snapshot list
  for(size_t i = 0,logical_counter = 1;i<snaps.size();i++)
    {
      // Add current snap shots. If we are extracting values from 
      // a larger trajectory file, then the snap shot list needs to
      // be locally changes for that mmpbsa run, so that the index
      // corresponds to the correct number in the temporary file.
      // Otherwise, we simply used the snapshot index as it is (else case).
      if(args.extract_trajectory)
	{
	  if(logical_counter == 1)
	    {
	      offset = snaps.at(i) - logical_counter;
	    }
	  else
	    extraction_buff << ",";
	  extraction_buff << snaps.at(i);
	  snap_buff << logical_counter++ << ",";
	}
      else
	snap_buff << snaps.at(i) << ",";

      // Once we've reached the end of a batch of snapshots. OR if we need to segment the snapshot list because 
      // we've made too big of a jump in snapshots for the snap_list_offset to be useful (i.e. needing to calculate 103 and 1435).
      if((i+1) % max_snaps_per_branch == 0 || (args.extract_trajectory && i + 1 != snaps.size() && snaps.at(i) + 1 != snaps.at(i+1)))
        {
	  XMLNode* newNode;

	  counter_buff.str("");
	  counter_buff << FIELD_DELIMITER << counter++;
	  snaplist = snap_buff.str();
	  extractionlist = extraction_buff.str();
	  if(snaplist.at(snaplist.size()-1) == ',')
	    snaplist.erase(snaplist.end()-1);
	  if(args.verbose_level > MEDIUM_VERBOSITY)
	    {
	      std::cout << "Processing MMPBSA Snaplist ";
	      if(extractionlist.size() != 0)
		std::cout << extractionlist;
	      else
		std::cout << snaplist;
	      std::cout << std::endl;
	    }

	  newNode = new XMLNode("mmpbsa");
	  insertedSnaplist = false;
	  for(XMLNode* it = serial_queue->children;it != 0;it = it->siblings)
            {
	      name = it->getName();
	      if("snap_list" == name)//is not a file
                {
		  text = snaplist;
		  insertedSnaplist = true;
                }
	      else//is a file
                {
		  grid_file_info file_info = grid_resolve_file_info(file_list,it->getText());
		  if(isOutput(it->getName()))
		    {
		      if(file_info.filename_alias.size() == 0)
			file_info.filename_alias = file_info.filename;
		      file_info.filename += FIELD_DELIMITER + counter_buff.str();
		      file_info.filename_alias += FIELD_DELIMITER + counter_buff.str();
		      file_info.filetype = file_types::temporary_output;
		      file_info.subdirectory = get_or_create_directory(INTERMEDIATE_DIRECTORY,args);
		      //info for recombination
		      recombination_list.push_back(file_info);
		    }
		  else if(it->getName() == "traj" && args.extract_trajectory)
		    {
		      if(file_info.filename_alias.size() == 0)
			file_info.filename_alias = file_info.filename;
		      file_info.filename += FIELD_DELIMITER + counter_buff.str();
		      file_info.filetype = file_types::temporary_input;
		      file_info.subdirectory = get_or_create_directory(INTERMEDIATE_DIRECTORY,args);
		      extracted_trajfile = file_info;
		      
		      if(file_info.filename_alias.size() != 0)
			file_info.filename_alias += FIELD_DELIMITER + counter_buff.str();
		    }
		  
		  counter_buff.str("");
		  counter_buff << file_list.size();
		  text = counter_buff.str();
		  file_list[text] = file_info;


                }
	      newNode->insertChild(name,text);
            }
	  if(!insertedSnaplist)
	    newNode->insertChild("snap_list",snaplist);//in the previous serial node did not have <snap_list>...</snap_list>
	    
	  // If the input trajectory is too large, the necessary part of it should
	  // be extracted. This block will create a subnode of mmpbsa that will 
	  // be used by grid_master to prepare the input trajectory when the 
	  // workunit is sent to the BOINC server and then the assimilator will
	  // remove that file.
	  if(args.extract_trajectory)
	    {
	      insert_traj_extraction(parallelizedNode,extractionlist,extracted_trajfile,parent_process,prereq,process_index,args);
	    }
	    		
	  // Insert the mmpbsa task.
	  snaplist == "";
	  snap_buff.str("");
	  extraction_buff.str("");
	  
	  if(offset != 0)
	    {
	      snap_buff.clear();
	      snap_buff.str("");
	      snap_buff << offset;
	      newNode->insertChild("snap_list_offset",snap_buff.str());
	      snap_buff.clear();
	      snap_buff.str("");
	    }
	  
	  std::ostringstream gflopbuff;
	  gflopbuff << max_snaps_per_branch*(MMPBSA_GFLOPS_PER_SNAPSHOT);
	  newNode->insertChild("gflop",gflopbuff.str());

	  if(args.previous_process >= 0)
            {
	      newNode->insertChild("prereq",prereq.str());
            }
	  idbuff.str("");idbuff << ++process_index;
	  newNode->insertChild("id",idbuff.str());
	  parallelizedNode->insertChild(newNode);// insert mmpbsa segment
	    
	  recombine_mmpbsa_results(parallelizedNode,recombination_list,process_index,file_list,args);// if needed, insert instance to recombined mmpbsa results
	  recombination_list.clear();

	  if(args.extract_trajectory)
	    insert_traj_cleanup(parallelizedNode, extracted_trajfile, process_index,prereq,process_index,args);

	  logical_counter = 1;
        }
    }

  if(snap_buff.str() != "")//in case there are N snapshots, where N % max_snaps_per_branch != 0
    {
      counter_buff.str("");
      counter_buff << FIELD_DELIMITER << counter++;
      snaplist = snap_buff.str();
      extractionlist = extraction_buff.str();
      if(snaplist.at(snaplist.size()-1) == ',')
	snaplist.erase(snaplist.end()-1);
      XMLNode* newNode = new XMLNode("mmpbsa");
      insertedSnaplist = false;
      for(XMLNode* it = serial_queue->children;it != 0;it = it->siblings)
        {
	  name = it->getName();
	  text = it->getText();
	  if("snap_list" == name)
	    {
	      text = snaplist;
	      insertedSnaplist = true;
	    }
	  else//is a file
            {
	      grid_file_info file_info = grid_resolve_file_info(file_list,it->getText());
	      if(isOutput(it->getName()))
		{
		  if(file_info.filename_alias.size() == 0)
		    file_info.filename_alias = file_info.filename;
		  file_info.filename += FIELD_DELIMITER + counter_buff.str();
		  file_info.filename_alias += FIELD_DELIMITER + counter_buff.str();
		  file_info.filetype = file_types::temporary_output;
		  file_info.subdirectory = get_or_create_directory(INTERMEDIATE_DIRECTORY,args);
		  //info for recombination
		  recombination_list.push_back(file_info);

		}
	      else if(it->getName() == "traj" && args.extract_trajectory)
		{
		  if(file_info.filename_alias.size() == 0)
		    file_info.filename_alias = file_info.filename;
		  file_info.filename += FIELD_DELIMITER + counter_buff.str();
		  file_info.filetype = file_types::temporary_input;
		  file_info.subdirectory = get_or_create_directory(INTERMEDIATE_DIRECTORY,args);
		  extracted_trajfile = file_info;
		  		      
		  if(file_info.filename_alias.size() != 0)
		    file_info.filename_alias += FIELD_DELIMITER + counter_buff.str();
		}

	      counter_buff.str("");
	      counter_buff << file_list.size();
	      text = counter_buff.str();
	      file_list[text] = file_info;
            }
	  newNode->insertChild(name,text);
		
        }
      if(!insertedSnaplist)
	newNode->insertChild("snap_list",snaplist);//in the previous serial node did not have <snap_list>...</snap_list>
	
      if(args.extract_trajectory)
	insert_traj_extraction(parallelizedNode,extractionlist,extracted_trajfile,parent_process,prereq,process_index,args);

      if(offset != 0)
	{
	  snap_buff.clear();
	  snap_buff.str("");
	  snap_buff << offset;
	  newNode->insertChild("snap_list_offset",snap_buff.str());
	  snap_buff.clear();
	  snap_buff.str("");
	}

      snaplist == "";
      if(args.previous_process >= 0)
        {
	  newNode->insertChild("prereq",prereq.str());
        }

      std::ostringstream gflopbuff;
      gflopbuff << max_snaps_per_branch*MMPBSA_GFLOPS_PER_SNAPSHOT;
      newNode->insertChild("gflop",gflopbuff.str());

      if(args.previous_process >= 0)
	{
	  newNode->insertChild("prereq",prereq.str());
	}
      idbuff.str("");idbuff << ++process_index;
      newNode->insertChild("id",idbuff.str());
      parallelizedNode->insertChild(newNode);
      recombine_mmpbsa_results(parallelizedNode,recombination_list,process_index,file_list,args);
      recombination_list.clear();

      if(args.extract_trajectory)
	insert_traj_cleanup(parallelizedNode, extracted_trajfile, process_index,prereq,process_index,args);

    }

  args.previous_process = process_index;
  return process_index;
}

void attach_mmpbsa_to_md(mmpbsa_utils::XMLNode* moledyn, mmpbsa_utils::XMLNode* mmpbsa,
		mmpbsa_utils::XMLNode* recombination_mmpbsa, std::map<std::string,grid_file_info>& file_info,
		const int& mmpbsa_id, const int& snap_count)
{
	using mmpbsa_utils::XMLNode;
	std::string mdcrd_filename = "";
	XMLNode *it, *curr_mmpbsa;

	if(moledyn == 0 || mmpbsa == 0)
		throw GridException("attach_mmpbsa_to_md: given a null pointer.",NULL_POINTER);

	if(moledyn->siblings != 0)
		throw GridException("attach_mmpbsa_to_md: molecular dynamics segment has an unexpected following procedure. It is unclear what to do with the mmpbsa.");

	for(it = moledyn->children;it != 0;it = it->siblings)
	{
		if(it->getName() == "mdcrd")
		{
			mdcrd_filename = it->getText();
			break;
		}
	}

	if(mdcrd_filename.size() == 0)
	{
		std::cerr << "Warning: cannot attach an mmpbsa calculation to the molecular dynamics calculation "
				<< "because no mdcrd file was provided from it." << std::endl;
		return;
	}


	for(curr_mmpbsa = mmpbsa;curr_mmpbsa != NULL;curr_mmpbsa = curr_mmpbsa->siblings)
	{
		XMLNode *offset_node = NULL;
		for(it = curr_mmpbsa->children;it != 0;it = it->siblings)
		{
			if(it->getName() == "inpcrd" || it->getName() == "traj")
			{
				if(file_info.find(mdcrd_filename) != file_info.end())
				{
					//in this case, mdcrd will be come the inpcrd of
					//mmpbsa runs that follow this md run. Therefore,
					//that needs to be labeled as a temporary INPUT file.
					grid_file_info inpcrd_file;
					std::ostringstream new_index;
					inpcrd_file = file_info[mdcrd_filename];
					inpcrd_file.filetype = file_types::temporary_input;
					new_index << file_info.size();
					file_info[new_index.str()] = inpcrd_file;
					it->setText(new_index.str());
				}
				else
					it->setText(mdcrd_filename);
				it->setName("traj");
			}
			if(it->getName() == "snap_list_offset")
				offset_node = it;
			if(it->getName() == "prmtop")
				it->setName("top");
			if(recombination_mmpbsa != 0)
				if(it->getName() == "mdout")
				{
					std::ostringstream new_file_index;
					grid_file_info orig_mdout = grid_resolve_file_info(file_info,it->getText());
					if(orig_mdout.filename_alias.find_last_of(FIELD_DELIMITER) != std::string::npos && orig_mdout.filename_alias.find_last_of(FIELD_DELIMITER) > orig_mdout.filename_alias.find_last_of('.'))
						orig_mdout.filename_alias.erase(orig_mdout.filename_alias.find_last_of(FIELD_DELIMITER));
					orig_mdout.filetype = file_types::perminant_output;
					new_file_index << file_info.size();
					recombination_mmpbsa->insertChild("recombination_input",new_file_index.str());
					new_file_index.str("");
					new_file_index << mmpbsa_id;
					orig_mdout.parent = new_file_index.str();
					new_file_index.str("");file_info.size();
					file_info[new_file_index.str()] = orig_mdout;
				}

		}
		if(snap_count > 0)
		{
			std::ostringstream offset_buff;
			offset_buff << snap_count;
			if(offset_node == NULL)
			{
				curr_mmpbsa->insertChild("snap_list_offset",offset_buff.str());
			}
			else
			{
				offset_node->setText(offset_buff.str());
			}
		}

	}

	moledyn->siblings = mmpbsa;
}

mmpbsa_utils::XMLNode* attach_to_end_of_queue(mmpbsa_utils::XMLNode* end_of_queue, mmpbsa_utils::XMLNode* new_queue_node, mmpbsa_utils::XMLNode* parent)
{
	mmpbsa_utils::XMLNode* new_end = end_of_queue;

	if(end_of_queue == 0 && parent == 0)
		throw GridException("attach_to_end_of_queue: If all null pointers are given, where would the new node be attached?",NULL_POINTER);
	else if(end_of_queue == 0)
		new_end = parent->children = new_queue_node;
	else
	{
		while(new_end->siblings != 0)
			new_end = new_end->siblings;
		new_end->siblings = new_queue_node;
	}

	while(new_end->siblings != 0)
		new_end = new_end->siblings;
	return new_end;
}

void mdin_to_snap_list(std::vector<size_t>& snap_list, const mdin_t& parm)
{
	size_t snap_count = parm.getSnapCount();
	for(size_t i = 0;i<snap_count;i++)
		snap_list.push_back(i+1);
}

/**
 * Takes a sander molecular dynamics XML queue and produces short processes.
 * Here parallel is a bit of a misnomer, as these processes still must be run
 * in series, as the results of a process are dependent upon those of its
 * preceeding process. However, the segment sizes are more appropriate to the
 * grid.
 *
 * After a string of segmented runs, mdout and mdcrd files need to be concatenated.
 * Therefore, after the last segment run, branch_sander will insert an internally
 * run process, "recombine", that will combine all of the processes' results.
 */
int branch_sander(mmpbsa_utils::XMLNode* parallelizedNode,
        mmpbsa_utils::XMLNode* serial_queue, const std::vector<mdin_t>& params,
        std::map<std::string,grid_file_info>& file_list,
        struct submit_job_arguments& args)
{
	using mmpbsa_utils::XMLNode;

	XMLNode* it,*newTag,*newNode,*mmpbsa_follower,*mmpbsa_recombiner, *end_of_queue;
	int process_id,previous_md_id;
	size_t num_of_segments;
	std::ostringstream suffixBuff;
	std::string temporary_directory,process_id_str;
	std::vector<size_t> snap_list;//blank to trigger branch_mmpbsa to deduce the snap count.
	grid_file_info mdcrdFile_info,last_restrt;
	std::vector<grid_file_info> recombination_list;//lists files that should be recombined after all segments finish.
	size_t recombo_count_list[2];//counts how many of each file type are provided. If there are two or more of any, recombine them.
	int snap_list_offset = 0;

	if(serial_queue == 0 || parallelizedNode == 0)
		throw GridException("branch_sander: null pointer provided for one of the queue nodes.", NULL_POINTER);

	recombo_count_list[0] = recombo_count_list[1] = 0;
	num_of_segments = params.size();

	temporary_directory = get_or_create_directory(INTERMEDIATE_DIRECTORY,args);

	if(serial_queue->siblings != 0 && serial_queue->siblings->getName() == "mmpbsa")
	{
		mmpbsa_follower = new XMLNode("mmpbsa");
		mmpbsa_recombiner = 0;//new XMLNode("recombine_mmpbsa");
	}
	else
		mmpbsa_follower = mmpbsa_recombiner = 0;

	//For efficiency's sake, let's retain a pointer to the end of the parallelized queue.
	//Then we'll attached to (and update) this node, rather than calling insertChild (which I *don't* want charged with maintaining an "end of node chain" pointer).
	end_of_queue = parallelizedNode->children;
	if(end_of_queue != 0)
		while(end_of_queue->siblings != 0)
			end_of_queue = end_of_queue->siblings;

	if(params.begin() != params.end())
	  process_id = params.begin()->getID();

	for(std::vector<mdin_t>::const_iterator parm = params.begin();
			parm != params.end();parm++)
	{
		grid_file_info nextinpcrd,new_file;

		if(parm->size() == 0)//in case an empty set got in somehow.
			continue;

		//new MD process
		newNode = new XMLNode("moledyn");
		//process_id = (mmpbsa_follower) ? (2*parm->getID() - params.begin()->getID()) : parm->getID();
		suffixBuff << process_id;//all MDParams in mdin_t will have the same ID according to serial_to_parallel_namelist
		newNode->insertChild("id",(process_id_str = suffixBuff.str()));
		suffixBuff.clear();suffixBuff.str("");
		suffixBuff << FIELD_DELIMITER << parm->getID();

		//set prerequisite MD run (if any)
		std::ostringstream previousRun;
		if(parm->getID() > 0)
		{
		  if(parm == params.begin())
		    previousRun << parm->getID() - 1;
		  else
		    previousRun << previous_md_id;
		  newNode->insertChild("prereq",previousRun.str());previousRun.str("");
		}

		//provide the gflop count if there is one.
		if(parm->getGFlopCount() > 0)
		{
			std::ostringstream gflop_buff;
			gflop_buff << parm->getGFlopCount();
			newNode->insertChild("gflop",gflop_buff.str());
		}

		//Map serial tags to new parallel tags.
		//Remember: File names and their aliases are stored in
		//file list. The queue xml tree stores the index to that list.
		//The file name is replaced when the job is submitted.
		//This keeps all of the information for a file in one location
		//and allows multiple occurances of one file to be indicated
		//by simply repeating one index rather than all of the
		//data for the file.
		for(it = serial_queue->children;it != 0;it = it->siblings)
		{
			std::string text;

			//Retrieve file information, if it already exists.
			if(it->getName() == "mdin")
			{
				new_file = grid_resolve_file_info(file_list,parm->getFilename());
			}
			else
			{
				new_file = grid_resolve_file_info(file_list,it->getText());
			}

			//set file types and update aliases if necessary
			if(isOutput(it->getName()) && num_of_segments > 1)
			{
				new_file.filename += suffixBuff.str();
				new_file.filename_alias += suffixBuff.str();
			}

			if(it->getName() == "inpcrd" && parm != params.begin())
			{
				if(mdcrdFile_info.filename.size() == 0)
					mdcrdFile_info = new_file;
				else
					new_file = mdcrdFile_info;
				new_file.subdirectory = temporary_directory;
			}

			//things to do for segmented jobs
			if(params.size() > 1)
			{
				if(it->getName() == "restrt")
				{
					nextinpcrd = new_file;
					nextinpcrd.parent = process_id_str;
					nextinpcrd.filetype = file_types::temporary_input;
					new_file.filetype = file_types::temporary_output;
					new_file.subdirectory = temporary_directory;
					last_restrt = new_file;
				}
				if(it->getName() == "mdout" || it->getName() == "mdcrd")
				{
					new_file.filetype = file_types::temporary_output;
					new_file.subdirectory = temporary_directory;
					new_file.parent = process_id_str;
				}
			}

			previousRun.str("");
			previousRun << file_list.size();
			text = previousRun.str();
			previousRun.str("");
			file_list[text] = new_file;
			newNode->insertChild(new XMLNode(it->getName(),text));

			//should this file be recombined after these segments are finished?
			if(it->getName() == "mdout" || it->getName() == "mdcrd")
			{
				if(new_file.filename_alias.size() == 0)
					new_file.filename_alias = new_file.filename;
				if(new_file.filename_alias.find_last_of(FIELD_DELIMITER) != std::string::npos && new_file.filename_alias.find_last_of(FIELD_DELIMITER) > new_file.filename_alias.find_last_of('.'))
					new_file.filename_alias.erase(new_file.filename_alias.find_last_of(FIELD_DELIMITER));
				new_file.filetype = file_types::perminant_output;
				new_file.parent = process_id_str;
				recombination_list.push_back(new_file);
				if(it->getName() == "mdout")
					recombo_count_list[0]++;
				else if(it->getName() == "mdcrd")
					recombo_count_list[1]++;
			}
		}

		//If a restart is produced in a segmented job, it is used as the inpcrd
		//for the next job.
		if(nextinpcrd.filename.size() != 0)
		{
			mdcrdFile_info = nextinpcrd;
		}
		previous_md_id = process_id;

		//If the MD run is followed by MMPBSA on the snapshots, queue MMPBSA
		//after each segment.
		suffixBuff.str("");
		if(mmpbsa_follower != 0 && parm->getSnapCount() > 0)
		{
			int mmpbsa_id;
			mdin_to_snap_list(snap_list,*parm);
			args.previous_process = process_id;
			mmpbsa_id = branch_mmpbsa(mmpbsa_follower,serial_queue->siblings,snap_list,file_list,args);
			attach_mmpbsa_to_md(newNode,mmpbsa_follower->children,mmpbsa_recombiner, file_list, mmpbsa_id, snap_list_offset);
			snap_list_offset += snap_list.size();
			mmpbsa_follower->children = 0;delete mmpbsa_follower;
			if(parm + 1 != params.end())
			{
				mmpbsa_follower = new XMLNode("mmpbsa");
				snap_list.clear();
			}
			process_id = mmpbsa_id;
		}

		end_of_queue = attach_to_end_of_queue(end_of_queue,newNode,parallelizedNode);
		process_id++;
	}


	//create recombination process, if necessary.
	for(size_t i = 0;i<2;i++)
	{
		if(recombo_count_list[i] > 1)
		{
			std::ostringstream recombo_id;
			recombo_id << process_id - 1;// id of previous MD run
			if(mmpbsa_follower)// did an mmpbsa run take the next id?
			  process_id++;	
			newNode = new XMLNode("recombine_moledyn");
			newNode->insertChild(new XMLNode("prereq",recombo_id.str()));
			recombo_id.str("");recombo_id << process_id;
			newNode->insertChild(new XMLNode("id",recombo_id.str()));

			for(size_t j = 0;j < recombination_list.size();j++)
			{
				const grid_file_info& recombine_this = recombination_list.at(j);
				suffixBuff.str("");suffixBuff << file_list.size();
				file_list[suffixBuff.str()] = recombine_this;
				newNode->insertChild(new XMLNode("recombination_input",suffixBuff.str()));
			}
			if(last_restrt.filename.size() > 0)
			{
				if(last_restrt.filename_alias.find_last_of(FIELD_DELIMITER) != std::string::npos && last_restrt.filename_alias.find_last_of(FIELD_DELIMITER) > last_restrt.filename_alias.find_last_of('.'))
					last_restrt.filename_alias.erase(last_restrt.filename_alias.find_last_of(FIELD_DELIMITER));
				suffixBuff.str("");suffixBuff << file_list.size();
				last_restrt.filetype = file_types::perminant_output;
				file_list[suffixBuff.str()] = last_restrt;
				newNode->insertChild(new XMLNode("recombination_input",suffixBuff.str()));
			}
			end_of_queue = attach_to_end_of_queue(end_of_queue,newNode,parallelizedNode);
			args.previous_process = process_id;
			break;
		}
	}

	return process_id;
}

off_t get_crd_filesize(const char *filename)
{
  struct stat fs;
  if(stat(filename,&fs) == 0)
    return fs.st_size;

  return 0;
}



/**
 * Takes a serial queue replaces it with a parallel version of itself
 * for an mmpbsa process using branch_mmpbsa and replace_serial_queue.
 */
void parallelize_mmpbsa(mmpbsa_utils::XMLNode* serial_queue,std::map<std::string,grid_file_info>& file_list,
		struct submit_job_arguments& args)
{
	using namespace mmpbsa_utils;
	using mmpbsa_io::count_snapshots;

	std::string crdFilename = "";
	std::string prmtopFilename = "";
	std::vector<size_t> snaps;
	mmpbsa::SanderParm *prmtop = NULL;
	off_t inpcrd_filesize = 0;
	const off_t max_crd_filesize = MAX_GRID_FILE_SIZE *1048576/* MB to B */;

	std::cout << "Parallelizing MMPBSA" << std::endl;

	for(XMLNode* it = serial_queue->children;it != 0;it = it->siblings)
	{
		if(it->getName() == "snap_list")
		{
			loadListArg(it->getText(),snaps);
		}
		if(it->getName() == "traj")
		  {
		    crdFilename = grid_resolve_filename(file_list,it->getText());
		    inpcrd_filesize = get_crd_filesize(crdFilename.c_str());// get the file size, if the file exists already.
		  }
		if(it->getName() == "prmtop" || it->getName() == "top")
			prmtopFilename = grid_resolve_filename(file_list,it->getText());
	}

	//If we're here, snap list was not provided. Therefore, divide the snapshots
	//into smaller subsets for parallel computing.
	if("" == crdFilename)
		return;// If no coordinate file was given, there may be a problem. We'll let mmpbsa catch this foobar.

	XMLNode* parallelizedNode = new XMLNode("parallel_mmpbsa");
	if(snaps.size() == 0)
	{
		//Try to determine number of snapshots in the trajectory file.
		//First, look for the snapshot number in the input file of
		//molecular dynamics preceeding the mmpbsa that would have
		//produced the mdcrd file mmpbsa is using, if that is the case.
		//If such an input file exists, divide NSTLIM by NTWX; that is
		//the number of snapshots.
		//Second, if the trajectory file was produced outside of this
		//grid job, try to parse the trajectory file and count the number
		//of snapshots.
		//Third, if no file was provided, let mmpbsa handle the user's
		//request. Either it will run the whole thing or it
		//will crash. Either way, the user should ensure proper data is
		//provided.

		bool should_double_check_file = false;
		//check mdin status
		if(args.mdin_mdcrd_map.find(crdFilename) != args.mdin_mdcrd_map.end())
		{
			const MDParams& mdin = args.mdin_mdcrd_map.find(crdFilename)->second;
			if(mdin.find("nstlim") == mdin.end())
			{
				should_double_check_file = true;
			}
			else if(mdin.find("ntwx") == mdin.end())
			{
				should_double_check_file = true;
			}
			else if(mdin.find("ntwx")->second == "0")//in this case, there are no snapshots according to the mdin file.
			{
				should_double_check_file = true;
			}
			else
			{
				int nstlim,ntwx;
				std::istringstream buff(mdin.find("nstlim")->second);
				buff >> nstlim;
				if(buff.fail())
				{
					should_double_check_file = true;
				}
				else
				{
					buff.clear();buff.str(mdin.find("ntwx")->second);
					buff >> ntwx;
					if(buff.fail())
					{
						should_double_check_file = true;
					}
					else
					{
						int num_snapshots;
						if(nstlim % ntwx != 0)
							std::cerr << "parallelize_mmpbsa: Warning: The number of steps is not a multiple of ntwx." << std::endl;
						num_snapshots = nstlim/ntwx;
						for(int i = 0;i<num_snapshots;i++)
							snaps.push_back(i+1);
					}
				}
			}
		}//end of parsing mdin file for number of snapshosts.
		else
		{
			should_double_check_file = true;
		}

		if(should_double_check_file && (prmtopFilename.size() == 0 || crdFilename.size() == 0))//no file, let mmpbsa decide what to do.
		{
			delete parallelizedNode;
			return;//again without coordinates and # of atoms, what do you do? Let mmpbsa decide.
		}
		else if(should_double_check_file)
		{
			try{
				prmtop = new mmpbsa::SanderParm;
				prmtop->raw_read_amber_parm(prmtopFilename);
				std::fstream crdFile(crdFilename.c_str(),std::ios::in);
				size_t numberOfSnaps = mmpbsa_io::count_snapshots(crdFile,prmtop->natom,prmtop->ifbox > 0);
				crdFile.close();
				for(size_t i = 0;i<numberOfSnaps;i++)
				{
					snaps.push_back(i + 1);//for the sake of human use, mmpbsa wants snap lists to be one-indexed.
				}
			}
			catch(mmpbsa::MMPBSAException mmpbsae)
			{
				std::cerr << "parallelize_mmpbsa: could not obtain information about snapshots needed to parallelize mmpbsa." << std::endl;
				std::cerr << "Reason: " << mmpbsae << std::endl;
				if(args.mdin_mdcrd_map.find(crdFilename) == args.mdin_mdcrd_map.end())
					std::cerr << "Also, there was no mdin file to use to decide the number of snap shots." << std::endl;
				delete prmtop;
				delete parallelizedNode;
				throw mmpbsae;
			}
		}
	}// if(snaps.size() == 0)

	// make parallel segments
	
	// There is a maximum number of snapshots allowed to run on a client.
	// If snapshots need to be extracted from a very large trajectory file,
	// the number of atoms is needed.
	if(snaps.size() > MMPBSA_MAX_SNAPSHOTS || inpcrd_filesize > max_crd_filesize)
	  {
	    if(prmtop == NULL)
	      {
		try{
		  prmtop = new mmpbsa::SanderParm;
		  prmtop->raw_read_amber_parm(prmtopFilename);
		}
		catch(mmpbsa::MMPBSAException mmpbsae)
		  {
		    delete prmtop;
		    delete parallelizedNode;
		    throw mmpbsae;
		  }
	      }
	    args.natoms = prmtop->natom;
	    args.ifbox = prmtop->ifbox;
	    args.extract_trajectory = true;

	  }
	delete prmtop;prmtop = NULL;
	branch_mmpbsa(parallelizedNode,serial_queue,snaps,file_list,args);

	replace_serial_queue(parallelizedNode,serial_queue);//deletes serial_queue
	parallelizedNode->children = 0;//children now belong to head of original grid_queue tree, which is becoming parallelized.
	delete parallelizedNode;

}



/**
 * Verifies that the given tag is a valid sander argument for the grid.
 */
void verify_allowed_sander_arg(const MDParams::key_type& tag)
{
    std::vector<std::string> not_allowed = barred_sander_parameters();
    std::vector<std::string>::const_iterator it;
    for(it = not_allowed.begin();it != not_allowed.end();it++)
    {
        if(*it == tag)
            throw GridException("Sander Parameter " + *it + " is currently"
                    " not supported on the grid.",INVALID_INPUT_PARAMETER);
    }
}



/**
 * XMLNode overload of verify_allowed_sander_arg(const MDParams::key_type& tag)
 */
void verify_allowed_sander_arg(mmpbsa_utils::XMLNode* tag)
{
    if(tag == 0)
    	throw GridException("verify_allowed_sander_arg: NULL XMLNode pointer.",INVALID_INPUT_PARAMETER);
	verify_allowed_sander_arg(tag->getName());
}



/**
 * Takes a sander input file (mdin) and alters its parameters to be more appropriate
 * for the grid, based on the number of steps and its position in the queue.
 * If a process follows another one (and is not minimization),
 * irest is set equal to one and ntx is set equal to five.
*/
mdin_t serial_to_parallel_namelist(const mdin_t& serial_params, const size_t& process_number, const size_t& nstlim_val, std::map<std::string,grid_file_info>& file_list)
{
	mdin_t parallelParams;
	std::ostringstream suffix_buff;
	MDParams curr_namelist;
	struct submit_job_arguments default_args;
	grid_file_info curr_file;
	std::string original_filename;
	mdin_t::const_iterator namelist = serial_params.begin();
	curr_file.filetype = file_types::perminant_input;
	original_filename = grid_resolve_filename(file_list,namelist->getFilename());
	curr_file.filename = namelist->getFilename();
	curr_file.filename_alias = namelist->getFilename();
	for(;namelist != serial_params.end();namelist++)
	{
		curr_namelist = *namelist;
		suffix_buff << original_filename << FIELD_DELIMITER << process_number;
		curr_file.filename = curr_file.filename_alias = suffix_buff.str();suffix_buff.str("");//create file alias and store it in file_list
		curr_file.subdirectory = get_or_create_directory(INTERMEDIATE_DIRECTORY,default_args);
		
		curr_file.filetype = file_types::temporary_input;

		suffix_buff << file_list.size();
		file_list[suffix_buff.str()] = curr_file;
		curr_namelist.setFilename(suffix_buff.str());suffix_buff.str("");//create alias to file_list, to be replaced later.
		curr_namelist.setID(process_number);
		suffix_buff << nstlim_val;
		curr_namelist["nstlim"] = curr_namelist["ntwr"] = suffix_buff.str();suffix_buff.str("");//no need to output restart files during a run on the client; it's wasted. Besides, we *do* need a restart at the end.

		if(process_number > 1)
		{
			curr_namelist["irest"] = "1";//needed to indicated a restart from a previous md run.
			curr_namelist["ntx"] = "5";//
			curr_namelist["ig"] = "-1";// need to ensure random number generator does not create artifacts between segments
		}
		parallelParams.push_back(curr_namelist);
	}
	return parallelParams;
}

float get_gflop_estimate(const size_t& nstlim,const size_t& natoms,const size_t& cutoff)
{
  float total_gflops;
  if(cutoff == 0)
    {
      return DEFAULT_GFLOP_COUNT;
    }
  
  //total gflops seems to be linear w.r.t. nstlim and natoms, but quadratic in cutoff.
  total_gflops = 6973.24;//based on 7.8A cutoff, 2500 nstlim and 17617 atoms
  total_gflops += 286.29*cutoff*cutoff - 1452.50*cutoff;

  total_gflops *= (1.0*natoms/17617);
  total_gflops *= (1.0*nstlim/25000);

  total_gflops /= 0.9;// normalization constant derived from statistics of 10-compound batches, each running for 5ns.
  return total_gflops;
}

size_t gcd(size_t a, size_t b)
{
	size_t r;
	while(b != 0)
	{
		r = a % b;
		a = b;
		b = r;
	}
	return a;
}

size_t lcm(size_t a, size_t b)
{
	return (a/gcd(a,b))*b;//do division first. a*b could overload for big number (whether or not submit_job would see them, it's still true)
}

/**
 * Determines the number of segements into which the process should be subdivided
 * in order to make the number of float operations a set number. This is based
 * on curve fitting prior results.
 */
size_t sander_segment_count(const size_t& nstlim,const size_t& natoms,const size_t& cutoff,const size_t& ntwx,
		const struct submit_job_arguments& args)
{
	size_t m;//returnMe,m,n;
	float total_gflops,steps_per_gflop;

	total_gflops = get_gflop_estimate(nstlim,natoms,cutoff);
	steps_per_gflop = 30000;//steps_per_gflop = total_gflops/nstlim;

	//Trivial case
	if(nstlim <= steps_per_gflop)
		return 1;

	//If we don't need to output trajectories, just use the most steps we can allow a grid client
	//to do.
	if(ntwx == 0)
	{
		m = 1;
		while(floor(1.0*nstlim/m) > steps_per_gflop)
			m++;
		return m;
	}

	//If we *do* have to output trajectories, make sure the nstlim of each segment is a multiple of
	//ntwx and goes evenly into nstlim
	m = (size_t) floor(steps_per_gflop/ntwx);//m = (size_t) floor(MD_MAX_GFLOP*steps_per_gflop/ntwx);

	//ensure this value a
	while(nstlim % (m*ntwx) != 0)
		m--;

	if(m == 0)
		throw GridException("sander_segment_count: could not find an good divisor.",EXCEEDED_RESOURCE_LIMIT);

	if(args.verbose_level > 1)
	{
		std::cout << "natoms: " << natoms << "  nstlim: " << nstlim << "  cut off: " << cutoff << std::endl;
		std::cout << "total GFlop: " << total_gflops << std::endl;
		std::cout << "steps per segment: " << m*ntwx << std::endl;
	}

	return nstlim / (m*ntwx);

}

/**
 * Split sander parameters (mdin) into smaller segments.
 * The new mdin data is returned in a vector, in the order in which
 * it should be run.
 */
std::vector<mdin_t> split_sander_parameters(const mdin_t& serialParams,
        const std::string& filenamePrefix,
        std::map<std::string,grid_file_info>& file_list, const std::string& mdin_fileindex,
        const size_t& natoms,
        struct submit_job_arguments& args)
{
	using std::string;
	std::vector<mdin_t> returnMe;
	mdin_t newParams;
	size_t numberOfSteps,cutoff,ntwx,segment_nstlim,numberOfProcesses;
	std::istringstream buff;
	mdin_t::const_iterator namelist = serialParams.begin();
	string nstlim_val,imin_val,cut_off_val,ntp_val,ntr_val,ntwx_val,filename;

   nstlim_val = imin_val = cut_off_val = filename = "";
   ntwx_val = ntp_val = ntr_val = "0";//default

   //gather parameters needed for the rest of this routine.
   for(;namelist != serialParams.end();namelist++)
   {
	   if(filename == "")
		   filename = namelist->getFilename();
	   else if(filename != namelist->getFilename())
		   throw GridException("split_sander_parameters: Filename mismatch in mdin file vector. " + filename
				   + " != " + namelist->getFilename(),INVALID_INPUT_PARAMETER);

	   if(namelist->find("imin") != namelist->end())
		   imin_val = namelist->find("imin")->second;
	   if(namelist->find("nstlim") != namelist->end())
		   nstlim_val = namelist->find("nstlim")->second;
	   if(namelist->find("cut") != namelist->end())
		   cut_off_val = namelist->find("cut")->second;
	   if(namelist->find("ntr") != namelist->end())
		   ntr_val = namelist->find("ntr")->second;
	   if(namelist->find("ntp") != namelist->end())
		   ntp_val = namelist->find("ntp")->second;
	   if(namelist->find("ntwx") != namelist->end())
		   ntwx_val = namelist->find("ntwx")->second;
	   if(namelist->find("ibelly") != namelist->end() && namelist->find("ibelly")->second != "0")
		   std::cerr << "WARNING - ibelly should not be used!" << std::endl;
   }

   //minimization is quick. It does not need to be split.
   //For nstlim, default is run 1 step. Can't split that up.
   //Also, certain MD runs *should not* be split up. For example, constant pressure
   //and restraints in the same run. These restart poorly. Thus, it will also have
   //numberOfProcesses = 1
   if(imin_val == "1" || nstlim_val.size() == 0 || (ntr_val != "0" && ntp_val != "0"))
   {
     newParams = serialParams;
     newParams.setID(++args.previous_process);
     returnMe.push_back(newParams);
     return returnMe;
   }

   //split up mdin parameters for parallel runs.
   buff.str(nstlim_val);
   buff >> numberOfSteps;
   if(buff.bad())
	   throw GridException("Invalid value for NSTLIM in input file: "+filename,INVALID_INPUT_PARAMETER);
   buff.clear();
   if(cut_off_val.size() == 0)
     cut_off_val == "0";
   buff.str(cut_off_val);
   buff >> cutoff;
   if(buff.bad())
   	   throw GridException("Invalid value for CUT in input file: "+filename,INVALID_INPUT_PARAMETER);
   buff.clear();buff.str(ntwx_val);
   buff >> ntwx;
   if(buff.bad())
	   throw GridException("Invalid value for NTWX in input file: " + filename,INVALID_INPUT_PARAMETER);
   

   //Decide the number of steps (NSTLIM) per segment based on curve fitting processor usage.
   //in the mdin file, including cut off and number of atoms (from prmtop)
   numberOfProcesses = sander_segment_count(numberOfSteps,natoms,cutoff,ntwx,args);
   if(args.verbose_level > 1)
	   std::cout << "Will split " << serialParams.getFilename() << " into " << numberOfProcesses << " parts." << std::endl;

   //Too small to break up
   if(numberOfProcesses == 1)
   {
     newParams = serialParams;
     newParams.setID(++args.previous_process);
     newParams.setGFlopCount(get_gflop_estimate(numberOfSteps,natoms,cutoff));
     newParams.setSnapCount(numberOfSteps);
     returnMe.push_back(newParams);
     return returnMe;
   }
   else
	   segment_nstlim = (size_t) ceil(1.0*numberOfSteps/numberOfProcesses);

   //Issue warnings with regards to segmentation
   for(mdin_t::const_iterator warn_it = serialParams.begin();warn_it != serialParams.end();warn_it++)
     {
       if(warn_it->find("ig") != warn_it->end())
	 std::cerr << "WARNING -- parameter ig has been set to " << warn_it->find("ig")->second
		   << " in an MD process which will be broken up into multiple segments."
		   << " Between those segments, ig = -1 will be used." << std::endl;

     }

   for(size_t i = 0;i<numberOfProcesses-1;i++)
   {
	   if(args.verbose_level > 2)
		   std::cout << "producing segment " << i << std::endl;
     newParams = serial_to_parallel_namelist(serialParams,++args.previous_process,segment_nstlim,file_list);
     newParams.setGFlopCount(get_gflop_estimate(segment_nstlim,natoms,cutoff));
     newParams.setSnapCount(( (ntwx) ? segment_nstlim/ntwx : 0 ));
     returnMe.push_back( newParams );
   }

   if(args.verbose_level > 2)
	   std::cout << "producing final segment of " << serialParams.getFilename() << std::endl;
   newParams = serial_to_parallel_namelist(serialParams,++args.previous_process,numberOfSteps - segment_nstlim*(numberOfProcesses-1),file_list);
   newParams.setGFlopCount(get_gflop_estimate(numberOfSteps - segment_nstlim*(numberOfProcesses-1),natoms,cutoff));
   newParams.setSnapCount(( (ntwx) ? (numberOfSteps - segment_nstlim*(numberOfProcesses-1))/ntwx : 0 ));
   returnMe.push_back(newParams);

   return returnMe;
}

std::string get_or_create_directory(const std::string& desired_directory,const struct submit_job_arguments& args)
{
	struct stat dir_info;
	int retval;
	if(0)
	  std::cout << "using stat on " << desired_directory << ": ";

	retval = stat(desired_directory.c_str(),&dir_info);
	if(retval)
	{
		if(args.verbose_level > MEDIUM_VERBOSITY)
				std::cout << " ERROR #" << errno  << " " << strerror(errno) << std::endl;
		switch(errno)
		{
		case EIO:
			throw GridException("get_or_create_directory: Could not use IO due to EIO error with stat for directory "+desired_directory,IO_ERROR);
		case ENOENT:
			break;
		case EACCES:case ENOTDIR: default:
			return ".";
		}
	}


	if(errno != ENOENT && S_ISDIR(dir_info.st_mode))
	{
		if(0 && args.verbose_level > 3)
			std::cout << " Directory exists. Using it." << std::endl;
		return desired_directory;
	}

	if(args.verbose_level > 3)
		std::cout << "Directory does not exist. Trying to create it." << std::endl;

	mode_t old_umask = umask(0);
	retval = mkdir(desired_directory.c_str(),S_IRWXU | S_IRWXG | S_ISGID);
	chown(desired_directory.c_str(),-1,GRID_USERS_GID);
	umask(old_umask);

	retval = stat(desired_directory.c_str(),&dir_info);
	//see if it's accessable
	if(retval)
	{
		if(args.verbose_level > 3)
				std::cout << " ERROR #" <<errno  << " " << strerror(errno) << std::endl;
		switch(errno)
		{
		case EIO:
			throw GridException("get_or_create_directory: Could not use IO due to EIO error with stat for directory "+desired_directory,IO_ERROR);
		case ENOENT:case EACCES:case ENOTDIR: default:
			return ".";
		}
	}

	return desired_directory;
}

/**
 * Outputs mdin data to the file specified by its filename.
 */
void output_parallel_mdin(mdin_t& parallelParams,std::map<std::string,grid_file_info>& file_list,struct submit_job_arguments& args)
{
    using namespace std;
    string output_filename,output_dir;
    grid_file_info info;
    //save the parameters to a file
    if(parallelParams.size() == 0)
    	return;
#if 0//obsolete
    string filename;
    istringstream index_buff;
    size_t index;

    index_buff.str(parallelParams[0].getFilename());
    index_buff >> index;
    if(index_buff.fail())
    {
    	filename = parallelParams[0].getFilename();
    	output_dir = ".";
    }
    else
    {
    	if(index >= file_list.size())
    	{
    		filename = parallelParams[0].getFilename();
    		output_dir = ".";
    	}
    	else
    	{
    		filename = grid_resolve_file_info(file_list, index).filename_alias;
    		output_dir = file_list.at(index).subdirectory;
    	}
    }
#endif

    info = grid_resolve_file_info(file_list,parallelParams[0].getFilename());

    output_filename = (info.subdirectory.size() > 0) ? info.subdirectory : ".";
    output_filename += "/";
    output_filename += (info.filename_alias.size() > 0) ? info.filename_alias : info.filename;
    if(args.verbose_level > 2)
    	std::cout << "Writing temporary mdin file to " << output_filename << std::endl;

    fstream mdin_file(output_filename.c_str(),ios::out);
    for(mdin_t::const_iterator namelist = parallelParams.begin();
    		namelist != parallelParams.end();namelist++)
    {
    	mdin_file << namelist->getTitle() << endl;
    	mdin_file << " " << namelist->getNamelist() << endl;
    	for(MDParams::const_iterator it = namelist->begin();it != namelist->end();it++)
    	{
    		mdin_file << "    " << it->first << " = " << it->second << "," << endl;
    	}
    	mdin_file << " /" << endl;
    }
    mdin_file.close();
    chown(output_filename.c_str(),-1,GRID_USERS_GID);
    chmod(output_filename.c_str(),0664);
}

/**
 * Reads an mdin file and produces an mdin_t object.
 */
mdin_t parse_mdin(const std::string& file_index,std::map<std::string, grid_file_info>& file_list)
{
    using std::string;
    using namespace mmpbsa_io;
    using namespace mmpbsa_utils;

    if(file_index.size() == 0)
    	throw GridException("parse_mdin: invalid file index for the file list provided.",INVALID_INPUT_PARAMETER);

    std::fstream file(grid_resolve_filename(file_list,file_index).c_str(),std::ios::in);

    if(!file.good())
    {
    	std::ostringstream error;
    	error <<"parse_mdin: Could not open file(" << file_index
    			<< "): " << grid_resolve_filename(file_list,file_index);
        throw GridException(error,IO_ERROR);
    }

    mdin_t returnMe;
    MDParams curr_mdin;
    string strParameters,currLine;
    curr_mdin.setTitle(getNextLine(file));
    strParameters = "";
    while(!file.eof())
    {
        currLine = trimString(getNextLine(file));
        if(currLine.size() == 0)
        	continue;
        if(currLine[0] == '/')
        {
        	curr_mdin.add_parameters(strParameters);
        	strParameters = "";
        	returnMe.push_back(curr_mdin);
        	continue;
        }
        if(currLine[0] == '&')
        {
        	curr_mdin.clear();
        	curr_mdin.setNamelist(currLine);
            continue;
        }

        strParameters += toLowerCase(currLine);
    }

    return returnMe;
}

int natoms_for_job(const std::string& prmtop_filename) throw (GridException)
{
	std::fstream prmtop;
	int natoms;
	mmpbsa::SanderParm * sp = new mmpbsa::SanderParm;
	try{
		sp->raw_read_amber_parm(prmtop_filename);
	}
	catch(mmpbsa::SanderIOException sioe)
	{
		delete sp;
		std::ostringstream error;
		error << "natoms_for_job: could not get data from parmtop file " << prmtop_filename
						<< std::endl << "Reason:" << std::endl
						<< sioe.what();
		throw GridException(error);
	}
	natoms = sp->natom;
	delete sp;
	return natoms;
}

/**
 * Takes a serial molecular dynamics process and replaceses
 * it with a series of processes which are more appropriate for the grid.
 * Again, parallel is a misnomer, since these segments are run sequentially.
 */
void parallelize_moldyn(mmpbsa_utils::XMLNode* serial_queue, std::map<std::string,grid_file_info>& file_list,
		struct submit_job_arguments& args) throw (GridException)
{
    using mmpbsa_utils::XMLNode;
    using std::string;
    using std::for_each;
    using std::vector;
    
    std::string mdin_fileindex, mdcrd_fileindex, prmtop_fileindex;
    vector<mdin_t> parameter_branches;
    vector<mdin_t>::iterator branch;
    XMLNode* parallelizedNode;
    mdin_t inputParams;
    size_t natoms,file_index;
    grid_file_info file_info;

    std::cout << "Parallelizing Molecular Dynamics";
    if(serial_queue->siblings && serial_queue->siblings->getName() == "mmpbsa")
      std::cout << " with MMPBSA";
    std::cout << std::endl;

    //Sanity check
    foreach(serial_queue->children,serial_queue->end(),verify_allowed_sander_arg);

    //extract file names needed in this routine.
    //replace values with an index to the file list
    for(XMLNode* it = serial_queue->children;it != 0;it = it->siblings)
    {
    	if(it->getName() == "mdin")
    		mdin_fileindex = it->getText();
    	else if(it->getName() == "mdcrd")
    		mdcrd_fileindex = it->getText();
    	else if(it->getName() == "prmtop")
    		prmtop_fileindex = it->getText();
    }

    //These are *required*.
    if(mdin_fileindex.size() == 0)
        throw GridException("An mdin file is needed for molecular dynamics.",MISSING_INPUT_FILE);
    else if(prmtop_fileindex.size() == 0)
    	throw GridException("A parmtop file is needed for molecular dynamics.",MISSING_INPUT_FILE);

    //Generate list of MDIN namelists.
    inputParams = parse_mdin(mdin_fileindex,file_list);

    //get the number of atoms
    natoms = natoms_for_job(grid_resolve_filename(file_list,prmtop_fileindex));

    //update filenames. Also, update mdin_mdcrd_map
     for(size_t i = 0;i<inputParams.size();i++)
     	inputParams[i].setFilename(mdin_fileindex);


    for(mdin_t::const_iterator namelist = inputParams.begin();
    		namelist != inputParams.end();namelist++)
    {
      if(mdcrd_fileindex.size() != 0 && namelist->find("ntwx") != namelist->end())
    	  args.mdin_mdcrd_map[grid_resolve_filename(file_list,mdcrd_fileindex)] = *namelist;

    	for(MDParams::const_iterator parameter = namelist->begin();
    			parameter != namelist->end();parameter++)
    	{
    		verify_allowed_sander_arg(parameter->first);
    	}
    }

    
    parameter_branches = split_sander_parameters(inputParams,grid_resolve_filename(file_list,mdin_fileindex),file_list,mdin_fileindex,natoms,args);
    
    //If there are multiple branches (eg not a minimization run), output the branches.
    //If there is only one segment (i.e. minimization run), there is no need to re-write
    //the mdin file.
    if(parameter_branches.size() > 1)
      for(branch = parameter_branches.begin();branch != parameter_branches.end();branch++)
    	output_parallel_mdin(*branch, file_list,args);

    parallelizedNode = new XMLNode("parallel_moledyn");
    branch_sander(parallelizedNode,serial_queue,parameter_branches,file_list,args);
    
    replace_serial_queue(parallelizedNode,serial_queue);
    parallelizedNode->children = 0;//children now belong to serial_queue's parent, which is the original grid_queue tree head node.
    delete parallelizedNode;
}

/**
 * Takes an XML queue process and divides into parallel runs. This function
 * reads the queue and decides if it is mmpbsa or molecuar dynamics and then
 * calls the appropriate function.
 */
void parallelize_queue(mmpbsa_utils::XMLNode* serial_queue, std::map<std::string,grid_file_info>& file_list,
		struct submit_job_arguments& args)
{
  using mmpbsa_utils::XMLNode;
    if(serial_queue == 0)
        return;

    int current_process = -1;
    if(serial_queue->getName() == "moledyn" || serial_queue->getName() == "molecular_dynamics")
      parallelize_moldyn(serial_queue, file_list,args);
    else if(serial_queue->getName() == "mmpbsa")
      parallelize_mmpbsa(serial_queue, file_list,args);
    else
      {
        std::cerr << "Do not know how to parallelize "
                << serial_queue->getName() << std::endl;
      }
}


/**
 * Submits a job to the queue system. This function is responsible for all of the
 * database entries for the job submitted by the user.
 */
void submit_job(mmpbsa_utils::XMLNode* jobXML,std::map<std::string,grid_file_info>& file_list,struct submit_job_arguments& args)
{

    using mmpbsa_utils::XMLNode;
    using std::string;
    using std::map;
    using std::cout;
    using std::endl;

    std::istringstream ibuff;
    std::ostringstream obuff;
    int submitter_uid = getuid();;

#if defined(USE_BOINC)
    BoincDBConn boincDB = getBoincDBConn();
    SCHED_CONFIG config;
    string configFilename = boincDB.BOINC_CONFIG_FILE_PATH;
    configFilename += "config.xml";
    FILE * configFile;
    int retval = try_fopen(configFilename.c_str(),configFile,"r");
    
  if(args.verbose_level > 1)
    std::cout << "simulating? " << ((args.simulate_submission) ? "yes" : "no") << std::endl;
  
  if(jobXML == 0)
    throw GridException("Empty XMLNode provided to submit_job");
  
   
    if(retval != 0)
      {
		std::ostringstream error;
		error << "Could not open project config file, expected to be in \""
			  << boincDB.BOINC_CONFIG_FILE_PATH << "\"" << std::endl
			  << "Reason: Boinc Error #" << retval << ": " << boincerror(retval);
		throw GridException(error,BOINC_ERROR);
      }
    config.parse(configFile);
    fclose(configFile);
#endif

    //Get application information
    obuff << "select ID,gridid from applications where name like '" << jobXML->getName() << "'";
    MYSQL_RES* app_ids = 0;
    if(args.simulate_submission)
    	std::cout << obuff.str() << std::endl;
    else
    	app_ids = query_db(obuff);
    obuff.str("");
    MYSQL_ROW app_ids_row;
    if(!args.simulate_submission)
    {
    	if(app_ids == 0 || mysql_num_rows(app_ids) == 0)
    		throw GridException("No such application, " + jobXML->getName() + ", in"
    				" queue database. (lookup 1)",DATABASE_ERROR);
    	if(!args.simulate_submission && mysql_num_rows(app_ids) > 1)
    		throw GridException("Ambiguous application name: " + jobXML->getName()
    				+ ". (lookup 2)",DATABASE_ERROR);
    	app_ids_row = mysql_fetch_row(app_ids);
    	if(app_ids_row[0] == 0)
    		throw GridException("No such application, " + jobXML->getName() + ", in"
    				" queue database. (lookup 3)",DATABASE_ERROR);
    	ibuff.clear();
    	ibuff.str(app_ids_row[0]);
    }
    else
    	ibuff.str("0");

    int app_id; ibuff >> app_id;
    if(ibuff.fail())
    {
        obuff.str("Expected integer instead of \"");
        obuff << app_ids_row[0] << "\".";
        throw GridException(obuff,DATABASE_ERROR);
    }

    //Submit process and get process id.
    obuff << "insert into processes (application,parentid,runid,state,comment,user_directory,uid,gflop_estimate,arguments) values "
            "(" << app_id << ", " ;

    //figure out the parent process, if any.
    //Check for gflop estimate, if any.
    XMLNode* it,*previous_node,*gflops_brother_node,*gflops_node;
    string parent_id = "NULL", process_arguments = "NULL";
    float gflop_count = 0;

    previous_node = gflops_brother_node = gflops_node = 0;
    for(it = jobXML->children;it != 0; it = it->siblings)
    {
        if(it->getName() == "prereq")
        {
            parent_id = it->getText();
        }
        else if(it->getName() == "gflop")
        {
        	//remove the gflop node. Then try to use its getText() value as an estimate of gfloat operations to be done.
        	gflops_brother_node = previous_node;
        	gflops_node = it;
        	std::istringstream gflop_buff(it->getText());
        	gflop_buff >> gflop_count;
        	if(gflop_buff.fail())
        		gflop_count = 0;
        }
	else if(it->getName() == "arguments")
	  {
	    process_arguments = it->getText();
	  }
        previous_node = it;
    }
    if(gflops_node != 0)
    {
    	if(gflops_brother_node == 0)//in this case, the gflop tag was first in the tag list.
    		jobXML->children = gflops_node->siblings;
    	else
    		gflops_brother_node->siblings = gflops_node->siblings;
    	gflops_node->siblings = 0;
    	delete gflops_node;
    	gflops_brother_node = gflops_node = previous_node = 0;
    }

    if(parent_id == "NULL")
        obuff << "NULL";
    else
    {
        size_t parent_id_size_t;
        ibuff.clear();
        ibuff.str(parent_id);
        ibuff >> parent_id_size_t;
        obuff << args.previous_processes[parent_id_size_t];
    }
    
    obuff << ", "<< args.runid << ", " << queue_states::waiting
	  << ", \'Submitted on " << time(NULL) << "\',";

    char currPath[1024];
    if(getcwd(currPath,1024) == 0)
      strcpy(currPath,"./");
    obuff << "\'" << currPath << "\'";

    //add uid to process database, if available
    //add gflop if it is available
    if(args.submitter_uid != -1)
      submitter_uid = args.submitter_uid;
    obuff << "," << submitter_uid <<", ";

    
    if(gflop_count == 0)
      obuff << "NULL";
    else
      obuff << gflop_count;
    
    obuff << ", \"" << process_arguments << "\"";
    
    obuff <<  ");" << std::endl;// end of process sql statement

    if(args.verbose_level > 0 || args.simulate_submission)
      cout << "DB procedure: " << obuff.str() << std::endl;
    my_ulonglong process_id = 0;
    if(!args.simulate_submission)
    {
    	mysql_query(queue_conn,obuff.str().c_str());
		if(mysql_errno(queue_conn) != 0)
		{
			obuff.str("Database Error #");
			obuff << mysql_errno(queue_conn) << ": " << mysql_error(queue_conn);
			throw GridException(obuff,DATABASE_ERROR);
		}
		process_id = mysql_insert_id(queue_conn);//truncate. Should not exceed 2^31. If so, clean database. Seriously, that'd be crazy.
    }
    
    std::string fileprefix;//give files the prefix <process_id>_<unixtime>_
    std::ostringstream fileprefixstream;
    if(jobXML->getName().find("recombine_") == std::string::npos)
    {
      fileprefixstream << std::hex << process_id << FIELD_DELIMITER << get_iso8601_time() << FIELD_DELIMITER;
    	fileprefix = fileprefixstream.str();
    }
    else
    	fileprefix = "";

    //Submit file names.
    obuff.str("");
    map<string,string> file_types = knownParameters();
    
    for(it = jobXML->children; it != 0;it = it->siblings)
    {
    	if(it->getName() == "id")
    	{
    		ibuff.clear();
    		ibuff.str(it->getText());
    		ibuff >> args.previous_process;
    		if(ibuff.fail())
    			throw GridException("Invalid process ID: " + it->getText());
    		args.previous_processes[args.previous_process] = process_id;
    		continue;
    	}
    	for(map<string,string>::const_iterator file_type = file_types.begin();
    			file_type != file_types.end();file_type++)
    	{
    		//get file info from file_list
    		grid_file_info file_info;
    		if(file_type->second == "snap_list")
		  continue;// nothing to do with this. It is not a file.

		if(file_list.find(it->getText()) == file_list.end())
    		{
    			file_info.filename = it->getText();
    			file_info.filename_alias = "";
    			file_info.filetype = (isOutput(it->getName())) ? file_types::perminant_output : file_types::perminant_input;
    		}
    		else
    		{
    			file_info = file_list[it->getText()];
    		}

    		//see if the parent id is aliased to a queue system id
    		std::istringstream index_buff;
    		size_t index;
    		index_buff.str(file_info.parent);
    		index_buff >> index;
    		if(!index_buff.fail())
    		{
    			if(index < args.previous_processes.size())
    			{
    				std::ostringstream file_parent_buff;
    				if(process_id != args.previous_processes[index])
    				{
    					file_parent_buff << args.previous_processes[index];
    					file_info.parent = file_parent_buff.str();
    				}
    				else
    					file_info.parent = "NULL";
    			}
    		}

    		//add insert sql line for this file
    		if(file_type->second == it->getName() || it->getName() == "recombination_input")
    		{

    			file_info.filename = fileprefix + file_info.filename;
    			if(args.verbose_level > 0 || args.simulate_submission)
    			{
			  cout << "File " << it->getText() << ": DB procedure: file_name: " << file_info.filename << " file_name_alias: " << file_info.filename_alias
    						<< " subdirectory: " << file_info.subdirectory << " parent_process: " << file_info.parent << " file_type: " << file_info.filetype << endl;

    			}
    			if(!args.simulate_submission)
    			{
    				set_file_info(file_info,process_id);//catch should happen later.
    			}
    			if(it->getName() == "recombination_input")
    			{
    				//stop this loop because we only want to add it once, but we do want to treat it like we would any other input file. So rather than duplication the code in two different if statements, there is one if block and we simply end the loop for recombination.
    				break;
    			}
    		}

    		//add prefix to queue file.
    		if(file_type->second == it->getName())
    		{
    			if(file_info.filename_alias.size() == 0)
    				it->setText(fileprefix + file_info.filename);
    			else
    				it->setText(fileprefix + file_info.filename_alias);
    		}
    	}
    	obuff.clear();
    	obuff.str("");
    }

    //write queue.xml for this job, unless it is an internal process. In which case, ignore the queue file
    if(jobXML->getName().find("recombine_") == std::string::npos)
    {
    	obuff << "insert into process_input_files (process_ID,file_name,file_type,subdirectory)"
    			" values (" << process_id << ", '" << fileprefix << "queue.xml', " << file_types::temporary_input << ","
    			<< "'" << get_or_create_directory(INTERMEDIATE_DIRECTORY,args) << "');";
    	if(args.verbose_level > 0 || args.simulate_submission)
    		cout << "DB procedure: " << obuff.str() << endl;
    	if(!args.simulate_submission)
    	{
    		mysql_query(queue_conn,obuff.str().c_str());
    		if(mysql_errno(queue_conn))
    		{
    			obuff.str("Database Error #");
    			obuff << mysql_errno(queue_conn) << ": " << mysql_error(queue_conn);
    			throw GridException(obuff,DATABASE_ERROR);
    		}

    		mysql_free_result(app_ids);
    	}

    	obuff.clear();obuff.str("");
    	obuff << get_or_create_directory(INTERMEDIATE_DIRECTORY,args) << "/" << fileprefix << "queue" << ".xml";
    	std::fstream queueFile(obuff.str().c_str(),std::ios::out);
    	if(!queueFile.is_open())
    		throw GridException("Could not write queue file.",IO_ERROR);
    	else if(args.verbose_level > 1)
    		std::cout << "writing queue file to " << obuff.str() << std::endl;
    	queueFile << "<" << MMPBSA_QUEUE_TITLE << ">" << std::endl
    			<< "<" << jobXML->getName() << ">" << std::endl;
    	if(jobXML->children)
    		queueFile << jobXML->children->toString() << std::endl;
    	queueFile << "</" << jobXML->getName() << ">" << std::endl
    			<< "</" << MMPBSA_QUEUE_TITLE << ">" << std::endl;
    	queueFile.close();
	chown(obuff.str().c_str(),-1,GRID_USERS_GID);
    	chmod(obuff.str().c_str(),0664);
    }
}

/**
 * Sets the job id for the batch submitted by the user, based on those presently in the queue
 * using max(runid).
 */
void update_runid(struct submit_job_arguments& args)
{
	if(args.simulate_submission)
		return;

	connect_db();
	MYSQL_RES* runids = query_db("select max(runid) from processes");
	if(runids == 0 || mysql_num_rows(runids) == 0)
	{
		args.runid = 0;
		return;
	}

	if(mysql_errno(queue_conn))
	{
		std::ostringstream error;
		error << "Database Error #" << mysql_errno(queue_conn) << ": " << mysql_error(queue_conn);
		throw GridException(error,DATABASE_ERROR);
	}

	MYSQL_ROW row = mysql_fetch_row(runids);
	if(row == 0 || row[0] == 0)
	{
		args.runid = 0;
		return;
	}

	std::istringstream ibuff(row[0]);
	ibuff >> args.runid;
	if(ibuff.bad())
		throw GridException("Invalid value for runid.",DATABASE_ERROR);
	args.runid++;

}

/**
 * For each segment in the XML queue list, submit_job is called.
 */
void submit_queue(mmpbsa_utils::XMLNode* queueXML,
		std::map<std::string,grid_file_info>& file_list, struct submit_job_arguments& args)
{
	mmpbsa_utils::XMLNode* it;
	if(args.runid < 0)
		update_runid(args);

	if(queueXML == 0 || queueXML->children == 0)
	{
		throw GridException("Empty XMLNode given to submit_queue.");
	}
	if(args.verbose_level)
	std::cout << "submit_queue: iterating through serial queue" << std::endl;
	

	if(!args.simulate_submission && !connect_db())
	{
		std::ostringstream error;
		error << "submit_queue: Could not connect to queue system database." << std::endl
				<< "Reason: ";
		if(queue_conn == 0)
			error << "No database object.";
		else
			error << "MySQL Error(" << mysql_errno(queue_conn) << "): " << mysql_error(queue_conn);
		throw GridException(error,MYSQL_ERROR);
	}
	
	for(it = queueXML->children;it != queueXML->end();it = it->siblings)
		submit_job(it,file_list,args);
}

void dump_graph(const mmpbsa_utils::XMLNode* queueXML, const std::string& filename)
{
  const mmpbsa_utils::XMLNode *processes = NULL, *properties = NULL;
  FILE *dot_file = NULL;
  std::string id,prereq;

  if(queueXML == NULL || filename.size() == 0 || queueXML->children == NULL)
    return;
  
  dot_file = fopen(filename.c_str(),"w");
  if(dot_file == NULL)
    {
      fprintf(stderr,"ERROR - Could not open graph file (%s) for writing.\nReason: %s",filename.c_str(),strerror(errno));
      return;
    }

  fprintf(dot_file, "digraph g {\n");
  fprintf(dot_file, "\tfirstcmd [shape=box,label=\"START\"];\n");

  processes = queueXML->children;
  for(;processes != NULL;processes = processes->siblings)
    {
      properties = processes->children;
      id = ""; 
      prereq = "firstcmd";
      for(;properties != NULL;properties = properties->siblings)
	{
	  if(properties->getName() == "id")
	    id = "cmd" + properties->getText();
	  else if(properties->getName() == "prereq")
	    prereq = "cmd" + properties->getText();
	}
      
      fprintf(dot_file, "\t%s [label=\"%s\"];\n",id.c_str(),processes->getName().c_str());
      fprintf(dot_file, "\t%s -> %s\n",prereq.c_str(),id.c_str());
    }
  
  fprintf(dot_file,"}\n");
  fclose(dot_file);
}

void default_submit_job_args(struct submit_job_arguments *args)
{
  // Not verbose
  args->verbose_level = 0;
  
  // Indicates the beginning of parallization process
  args->previous_process = -1;
  
  // Go live
  args->simulate_submission = false;
  
  // Indicates sander parameters have not yet been read
  args->natoms = args->ifbox = -1;
  
  // Full trajectory is used
  args->extract_trajectory = false;
  
  // Command graph is not produced
  args->generate_graph = false;
}

//Reads a job submission file and creates segmented (some parallel, some series)
//jobs that are run on the grid. Then processes are then started by grid_master.
int submit_job_main(struct submit_job_arguments& args, std::map<std::string,grid_file_info>& file_list)
{
	if(!connect_db() && !args.simulate_submission)
	{
		std::ostringstream error;
		error << "submit_job: Could not connect to grid database." << std::endl;
		if(queue_conn == 0)
			error << "No database object. ";
		else
			error << "Reason: MySQL Error #" << mysql_errno(queue_conn) << ": " << mysql_error(queue_conn);
		throw GridException(error,DATABASE_ERROR);
	}


	args.previous_process = -1;//could alter this to allow for an offset in the queue order.
	args.runid = -1;
	args.submitter_uid = -1;
	
	using std::string;
	std::fstream input;
	input.open(args.job_file.c_str(),std::ios::in);
	if(!input.is_open())
	{
		std::cerr << "Could not read input from: " << args.job_file << std::endl;
		return 1;
	}

	//Serial Queue based on submitted job file.
	if(args.verbose_level > 0)
		std::cout << "Parsing submitted commands" << std::endl;
	mmpbsa_utils::XMLNode* queueXML = parse_job_submission(input,file_list);

	if(args.verbose_level >= WICKED_VERBOSE)
		std::cout << "Serial Queue: " << std::endl << queueXML->toString() << std::endl;

	if(queueXML == 0)
	{
		std::cerr << "Could not process job from: " << args.job_file << std::endl;
		return 1;
	}
	input.close();

	//Take serial queue and split it into parallel segments.
	//Replace queueXML(which start out serial) with a parallel version.
	try{
	  mmpbsa_utils::XMLNode* it = queueXML->children;
		while(it != 0)
		{
			try{
				if(args.verbose_level > 0)
					std::cout << "Parallelizing queue" << std::endl;
				parallelize_queue(it, file_list, args);
			}
			catch(mmpbsa::MMPBSAException mmpbsae)
			{
				return mmpbsae.getErrType();
			}
			catch(GridException ge)
			{
				std::cerr << ge << std::endl;
				return ge.getErrType();
			}
			mmpbsa_utils::XMLNode* temp = it->siblings;
			if(temp == 0)//in this case, no parallelization has been done.
				break;
			if(temp->getName() == "mmpbsa" && it->getName() != "mmpbsa")
			  {
			    mmpbsa_utils::XMLNode *mmpbsa = temp;
			    temp = queueXML->children;
			    while(temp->siblings != mmpbsa)
			      temp = temp->siblings;
			    temp->siblings = mmpbsa->siblings;
			    mmpbsa->siblings = NULL;
			    delete mmpbsa;
			    temp = temp->siblings;
			  }
			it->siblings = 0;
			delete it;
			it = temp;
		}
		if(args.verbose_level)
			std::cout << "Submitting job" << std::endl;
		if(args.verbose_level > 30)
		  std::cout << "Parallel Queue: " << std::endl << queueXML->toString() << std::endl;
		
		if(args.generate_graph)
		  {
		    if(args.graph_filename.size() == 0)
		      {
			fprintf(stderr,"WARNING - No file name provided for graph file. Cannot create graph.\n");
		      }
		    else
		      {
			if(args.verbose_level)
			  std::cout << "Create graph of parallel commands as file \""  << args.graph_filename << "\"" << std::endl;
			dump_graph(queueXML,args.graph_filename);
		      }
		  }

		submit_queue(queueXML,file_list,args);
		if(args.verbose_level)
			std::cout << "Job submitted." << std::endl;
	}
	catch(GridException ge)
	{
		delete queueXML;
		std::cerr << ge.identifier() << ": " << ge.what() << std::endl;
		return ge.getErrType();
	}


	time_t now;
	time(&now);
	struct tm* timeinfo = localtime(&now);
	std::cout << "Job #" << args.runid << " has been submitted on " << asctime(timeinfo) << std::endl;

	delete queueXML;

	return 0;
}


