/**
 * combine_mmpbsa_data
 *
 * Reads MMPBSA data files (in XML format) and combines the results into one
 * XML data file.
 *
 * Created by: David Coss, 2010
 */
#include <iostream>
#include <string>
#include <vector>

#include "libmmpbsa/XMLParser.h"

#include "GridException.h"


/**
 * Returns true of the test_element is less than the partition_element. Otherwise, false 
 * is returned.
 *
 * Function exits if either of the two contain no "ID" field or if the "ID" value is not
 * an integer; a message will be written to cerr.
 */
bool sort_algo(mmpbsa_utils::XMLNode* test_element, mmpbsa_utils::XMLNode* partition_element)
{
	std::string te_ID,pe_ID,te_off_str,pe_off_str;
	mmpbsa_utils::XMLNode* it;

	// Collect data from test element
	for(it = test_element->children;it != 0;it = it->siblings)
	{
		if(it->getName() == "ID")
			te_ID = it->getText();
		else if(it->getName() == "snap_list_offset")
			te_off_str = it->getText();
	}

	// Collect data from partition element
	for(it = partition_element->children;it != 0;it = it->siblings)
	{
		if(it->getName() == "ID")
			pe_ID = it->getText();
		else if(it->getName() == "snap_list_offset")
			pe_off_str = it->getText();
	}


	// Do we have ID's from both elements?
	if(te_ID.size() == 0 || pe_ID.size() == 0)
	{
		std::cerr << "sort_algo: could not get id from one of the nodes" << std::endl;
		exit(1);
	}

	// Compare data
	std::istringstream buff;
	int peid,teid,pe_off = 0,te_off = 0;
	if(sscanf(pe_ID.c_str(),"%d",&peid) != 1)
		throw GridException("sort_algo: could not get id from partition element",INVALID_INPUT_PARAMETER);
	if(sscanf(te_ID.c_str(),"%d",&teid) != 1)
			throw GridException("sort_algo: could not get id from test element",INVALID_INPUT_PARAMETER);
	if(pe_off_str.size() > 0)
		if(sscanf(pe_off_str.c_str(),"%d",&pe_off) != 1)
			throw GridException("sort_algo: could not get id from partition element ID offset",INVALID_INPUT_PARAMETER);
	if(te_off_str.size() > 0)
		if(sscanf(te_off_str.c_str(),"%d",&te_off) != 1)
			throw GridException("sort_algo: could not get id from test element ID offset",INVALID_INPUT_PARAMETER);

	return (teid + te_off) < (peid + pe_off);
}

struct mmpbsa_combine_struct{
  bool operator()(mmpbsa_utils::XMLNode* lhs,mmpbsa_utils::XMLNode* rhs)
  {
    return sort_algo(lhs,rhs);
  }
};

/**
 * Iterates through the list of data file names. Each file is loaded and added to an
 * XMLNode.
 */
mmpbsa_utils::XMLNode* recombine_mmpbsa_files(const std::vector<std::string>& infile_names) throw (mmpbsa::XMLParserException, GridException)
{
	if(infile_names.size() < 1)
	{
		std::ostringstream error;
		error << "recombine_mmpbsa_files needs at least one file to recombine, but was given " << infile_names.size();
		throw GridException(error,INVALID_INPUT_PARAMETER);
	}
	std::vector<std::string>::const_iterator filename = infile_names.begin();

	//load files
	mmpbsa_utils::XMLParser segment;
	mmpbsa_utils::XMLNode *recombined,*nodeSegment,*child_iterator;
	recombined = new mmpbsa_utils::XMLNode("mmpbsa_energy");
	for(;filename != infile_names.end();filename++)
	{
		segment.parse(*filename);
		nodeSegment = segment.detachHead();
		if(nodeSegment->children == 0)
			continue;//empty document. do not give insertChild a null pointer.

		//explanation of next line: attachChildren attaches the provided node
		//as the children of the "this" node and returns the previous children.
		//Therefore, this line will effectively detach the children of
		//nodeSegment, allowing us to delete nodeSegment, which is necessary
		//so as not to memory-leak nodeSegment.
		recombined->insertChild(nodeSegment->attachChildren(0));
		delete nodeSegment;
	}
	return recombined;
}


/**
 * Takes a list of XML data
 */
int mmpbsa_combine_worker(const std::vector<std::string>& input_filenames, const std::string& out_filename)
{
	using mmpbsa_utils::XMLNode;
	using mmpbsa_utils::XMLParser;

	std::set<XMLNode*,mmpbsa_combine_struct> index_array;
	std::set<XMLNode*,mmpbsa_combine_struct>::iterator it;
	XMLNode *combined_result,*temp,*original_file = NULL;

	if(access(out_filename.c_str(),R_OK) == 0)
	{
		std::fstream previous_data(out_filename.c_str(),std::ios::in);
		if(previous_data.good())
		{
			original_file = XMLParser::parse(previous_data);
			previous_data.close();
		}
	}

	try
	{
		combined_result = recombine_mmpbsa_files(input_filenames);
		if(combined_result == NULL || combined_result->children == NULL || combined_result->children->getName() != "snapshot")
		{
			if(original_file != NULL)
				delete original_file;
			throw GridException("Empty set of result XML nodes.");
		}

		// If we had original data, append the new data
		// to the end of combined_results
		if(original_file != NULL)
		{
			if(original_file->children == NULL)
			{
				original_file->children = combined_result->children;
			}
			else
			{
				temp = original_file->children;
				while(temp->siblings != NULL)
					temp = temp->siblings;
				temp->siblings = combined_result->children;
			}
			combined_result->children = NULL;
			delete combined_result;
			combined_result = original_file;
		}

		// Sort the data by ID, using mmpbsa_combine_struct as the
		// sort structure
		temp = combined_result->children;
		while(temp != NULL)
		  {
		    index_array.insert(temp);
		    temp = temp->siblings;
		  }

		it = index_array.begin();
		if(it != index_array.end())
		{
			combined_result->children = *it;
			temp = *it;
			it++;
			for(;it != index_array.end();it++)
			{
				temp->siblings = *it;
				temp = *it;
			}
			temp->siblings = 0;
		}
		XMLParser xmlDoc(combined_result);
		xmlDoc.write(out_filename);
	}
	catch(GridException ge)
	{
		std::cerr << "combine_mmpbsa_worker: Could not recombine mmpbsa files." << std::endl
				<< "Reason: " << ge << std::endl;
		return ge.getErrType();
	}
	catch(mmpbsa::XMLParserException xmlpe)
	{
		std::cerr << "combine_mmpbsa_worker: Could not recombine mmpbsa files." << std::endl
						<< "Reason: " << xmlpe << std::endl;
		return xmlpe.getErrType();
	}

	return 0;
}

#ifdef STANDALONE_COMBINE_MMPBSA

/**
 * Loads argv[1..argc-2] into a vector and sends it to mmpbsa_combine_worker with argv[argc-1] as the
 * output filename.
 */
int main(int argc, char** argv)
{
	using std::string;
	using std::cout;
	using std::endl;

	std::vector<string> input_filenames;
	string out_filename;

	if(argc < 3)
	{
		std::cout << "Usage: " << argv[0] << " <file1> <file2> ... <newfile>";
		return 0;
	}

	cout << "Combining ";

	for(int i = 1;i<argc - 1;i++)
	{
		input_filenames.push_back(argv[i]);
		cout << argv[i] << " ";
	}
	out_filename = argv[argc - 1];
	cout << "with " << out_filename << endl;

	return mmpbsa_combine_worker(input_filenames,out_filename);

}

#endif
