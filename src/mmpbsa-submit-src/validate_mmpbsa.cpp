/**
 * Validates mmpbsa results for agreement of data values.
 *
 * Created by David Coss, 2010
 */

#include "libmmpbsa/XMLNode.h"
#include "libmmpbsa/XMLParser.h"
#include "libmmpbsa/EMap.h"
#include "libmmpbsa/mmpbsa_utils.h"
#include "libmmpbsa/globals.h"

#include "validation_states.h"

#include <iostream>
#include <string>
#include <cstring>
#include <stdio.h>
#include <vector>
#include <utility>
#include <getopt.h>
#include <cerrno>

struct option long_opts[] = {
		{"help",0,NULL,'h'},
		{"output",1,NULL,'o'},
		{"tolerance",1,NULL,'t'},
		{"usage",0,NULL,'h'},
		{"verbose",2,NULL,'v'},
		{NULL,0,NULL,0}
};
static const char short_opts[] = "ho:t:v";
static int validate_mmpbsa_verbosity = 0;
static std::string validate_mmpbsa_output_filename = "";


static void print_help()
{
	printf("validate_mmpbsa -- program which reads multiple mmpbsa data XML files and determines whether the results are with a specified tolerance.\n");
	printf("Usage:\n");
	printf("\t-o, --output[=FILENAME]\t\tSave a summary of the statistics of the data in the specified file.");
	printf("\t-t, --tolerance\t\tSpecify an EMap XML file to be the acceptable tolerance.\n");
	printf("\t-v, --verbose[=INT]\t\tSet verbosity\n");
	printf("\t-h, --help\t\tDisplay this helpful message.\n");
	printf("\t  , --usage\n");
}

/**
 * Takes an array of XMLNodes that contain Complex, Receptor and Ligand data
 * and returns an array of pairs, which map a molecule type to its EMap data.
 */
std::pair<std::string,mmpbsa::EMap>*
getEnergyData(mmpbsa_utils::XMLNode** snapshots,size_t numberOfData)
{
    using std::string;
    using std::pair;
    using mmpbsa::EMap;
    using mmpbsa_utils::XMLNode;

     if(snapshots == 0)
        return 0;

    pair<string,EMap>* returnMe = new pair<string,EMap>[numberOfData*snapshots[0]->hasChildren()];
    size_t dataCounter = 0;
    for(int i =0;i<numberOfData;i++)
    {
        const XMLNode* currEnergyTree = snapshots[i];
        for(const XMLNode* energyData = currEnergyTree->children;
                    energyData !=0;energyData = energyData->siblings)
        {
            std::string dataType = energyData->getName();
            if(dataType == "ID")
                continue;
            if(dataType == "COMPLEX" || dataType == "RECEPTOR" || dataType == "LIGAND")
                returnMe[dataCounter++] = pair<string,EMap>(dataType,EMap::loadXML(energyData));
            else if(validate_mmpbsa_verbosity > 1)
                fprintf(stderr,"getEnergyData was given an unknown data type (%s), "
                        "which is being ignored.\n",dataType.c_str());
        }
    }
    return returnMe;
    
}

/**
 * Takes a vector of XML data and checks whether or not each snapshot (per receptor, ligand, complex)
 * is within a certain provided tolerance of the average.
 *
 * If the result is invalid but data was compared, a pointer to XML document is produced
 * that summarizes the differences between the data. The state, according to validation_states.h is assigned
 * to validationState.
 *
 * @param energyData vector of XML file contents.
 * @param tolerance maximum standard deviation which is considered valid.
 * @param validationState cf validation_states.h
 */
mmpbsa_utils::XMLNode* validateMMPBSA(const std::vector<mmpbsa_utils::XMLParser*> energyData,const mmpbsa::EMap& tolerance,int& validationState)
{
	size_t numberOfTrees = energyData.size();
	validationState = EMPTY_SET;
    if(numberOfTrees == 0)
	{
    	return 0;
	}

    using mmpbsa_utils::XMLNode;
    using mmpbsa::EMap;
    using std::vector;
    size_t numberOfSnaps = energyData[0]->getHead()->hasChildren();

    XMLNode** currSnap = new XMLNode*[numberOfTrees];
    for(int i = 0;i<numberOfTrees;i++)
        currSnap[i] = 0;
    
    std::string snapName;
    XMLNode* avgs = new XMLNode("average");
    XMLNode* rmses = new XMLNode("rms");
    XMLNode* variances = new XMLNode("coefficient_of_variation");
    validationState = GOOD_VALIDATION;
    for(size_t i = 0;i<numberOfSnaps;i++)
    {
        for(size_t j = 0;j<numberOfTrees;j++)
        {
            if(currSnap[j] == 0)
                currSnap[j] = energyData[j]->getHead()->children;
            else
                currSnap[j] = currSnap[j]->siblings;
            
            if(currSnap == 0)
            {
                delete[] currSnap;
                delete avgs;
                delete rmses;
                validationState = MISSING_SNAPSHOT;
                return 0;
            }
        }

        std::pair<std::string,EMap>* snapData = getEnergyData(currSnap,numberOfTrees);
        char snapLabel[256];
        std::string molLabel;
        sprintf(snapLabel,"snapshot_%d",i+1);
        for(int j = 0;j<3;j++)//complex,receptor,ligand
        {
            mmpbsa::EMap average,rms,variance,it;
            molLabel = snapLabel;
            molLabel += "_" + snapData[j].first;
            for(int k = 0;k<numberOfTrees;k++)
            {
                it = snapData[3*k+j].second;
                average += it;
                rms += it*it;
            }

            average /= numberOfTrees;
            rms /= numberOfTrees;
            avgs->insertChild(average.toXML(molLabel));
            rmses->insertChild(sqrt(rms).toXML(molLabel));

            variance = rms - average*average;
            variance = sqrt(variance);

            // convert stddev to coefficient of variation and then
            // determine if that value is within tolerance levels
            variance = abs( variance.elementwise_division(average) );
            if(variance.molsurf_failed)
            {
            	variance.area = 0;
            	variance.sasol = 0;
            }
            variances->insertChild(variance.toXML(molLabel));


            if(variance > tolerance)
            {
            	validationState = BEYOND_TOLERANCE;
            	if(validate_mmpbsa_verbosity > 1)
            	{
            		switch(j)
            		{
            		case 0:
            			printf("COMPLEX ");
            			break;
            		case 1:
            			printf("RECEPTOR ");
            			break;
            		case 2:
            			printf("LIGAND ");
            			break;
            		default:
            			break;
            		}
            		printf(" of snapshot %d is beyond tolerance levels\nVariance:\n",i);
            		std::cout << variance << std::endl;
            	}
            }
        }
        delete[] snapData;
    }

    XMLNode* dataSummary = new XMLNode("data_summary");
    dataSummary->insertChild(variances);
    dataSummary->insertChild(avgs);
    dataSummary->insertChild(rmses);
    dataSummary->insertChild(tolerance.toXML("tolerance"));

    delete[] currSnap;
    return dataSummary;
}

int load_mmpbsa_tolerance(const char *filename, mmpbsa::EMap& tolerance)
{
	using namespace mmpbsa_utils;
	using mmpbsa::EMap;
	XMLNode *data = NULL, *xml_emap = NULL;
	std::ifstream file;
	if(filename == NULL || strlen(filename) == 0)
	{
		fprintf(stderr,"load_tolerance: Null pointer for filename\n");
		return -1;
	}
	if(access(filename,R_OK) != 0)
		return errno;
	file.open(filename);
	data = XMLParser::parse(file);
	if(data == NULL || data->children == NULL)
	{
		fprintf(stderr,"load_tolerance: Empty dataset for file %s\n",filename);
		return -1;
	}
	if(data->getName() != EMap::DEFAULT_XML_TAG && data->getName() != MMPBSA_XML_TITLE)
	{
		fprintf(stderr,"load_tolerance: Unknown data structure: %s\n",data->getName().c_str());
		return -1;
	}

	xml_emap = data->children;
	if(xml_emap->getName() != "snapshot")
		xml_emap = data;
	try{
		tolerance = EMap::loadXML(xml_emap);
	}
	catch(mmpbsa::MMPBSAException mmpbsae)
	{
		fprintf(stderr,"load_tolerance: Error loading XML data. Error Message:\n%s",mmpbsae.what());
		return mmpbsae.getErrType();
	}

	delete data;
	return 0;
}

mmpbsa_utils::XMLNode* validateMMPBSAWorker(const std::vector<std::string>& filenames, const mmpbsa::EMap& tolerance, int& validationState)
{
	using mmpbsa_utils::XMLParser;
	using mmpbsa::EMap;
	using std::vector;
	using std::string;
	size_t numberOfTrees = filenames.size();
	
	vector<XMLParser*> energyTrees;
	vector<XMLParser*>::iterator tree;
	

	if(validate_mmpbsa_verbosity > 2)
	{
		mmpbsa_utils::XMLNode *txml = tolerance.toXML();
		std::cout << txml->toString() << std::endl;
		delete txml;
	}

	for(vector<string>::const_iterator filename = filenames.begin();filename != filenames.end();filename++)
	{
		if(filename->size() == 0)
		{
			fprintf(stderr,"Validator needs at least two filenames\n");
			validationState = VALIDATOR_IO_ERROR;
			return NULL;
		}

		if(access(filename->c_str(),R_OK) != 0)
		{
			fprintf(stderr,"validateMMPBSAWorker: Cannot read from %s\n", filename->c_str());
			fprintf(stderr,"Reason: %s\n");
			validationState = VALIDATOR_IO_ERROR;
			return NULL;
		}
		energyTrees.push_back(new XMLParser());
		tree = energyTrees.end() - 1;
		try{
			if(validate_mmpbsa_verbosity)
				fprintf(stderr,"Loading %s...",(*filename).c_str());
			(*tree)->parse(*filename);
			if(validate_mmpbsa_verbosity)
				fprintf(stderr," done\n");
		}
		catch(mmpbsa::XMLParserException xmlpe)
		{
			std::cerr << "validateMMPBSAWorker: Error opening " << *filename << std::endl;
			std::cerr << "Reason: " << xmlpe << std::endl;
			validationState = XML_FILE_ERROR;
			return NULL;
		}
	}

	size_t numberOfSnaps;
	if(energyTrees.size() == 0)
	{
		fprintf(stderr,"validateMMPBSAWorker: Could not load energy data\n");
		validationState = XML_FILE_ERROR;
		return NULL;
	}
	numberOfSnaps = ((XMLParser*)*energyTrees.begin())->getHead()->hasChildren();

	for(tree = energyTrees.begin()+1;tree != energyTrees.end();tree++)
		if(numberOfSnaps != (*tree)->getHead()->hasChildren())
		{
			fprintf(stderr,"Energy data must all of equal number of snapshots.\n");
			validationState = UNEQUAL_NUMBER_OF_SNAPSHOTS;
			return NULL;
		}

	if(numberOfSnaps == 0)
	{
		fprintf(stderr,"No energy data was given to validate.\n");
		validationState = EMPTY_SET;
		return NULL;
	}

	mmpbsa_utils::XMLNode* dataSummary = validateMMPBSA(energyTrees,tolerance,validationState);

	for(tree = energyTrees.begin();tree != energyTrees.end();tree++)
		delete *tree;

	return dataSummary;
}

#ifdef STANDALONE
int main(int argc,char** argv)
{
    using mmpbsa::EMap;
    using mmpbsa_utils::XMLParser;
    using mmpbsa_utils::XMLNode;


    int validationState;
    EMap tolerance;
    mmpbsa_utils::XMLNode* dataSummary = NULL;
    char cmd_flag = '?';
    std::vector<std::string> filenames;

    if(argc < 2)
    {
        fprintf(stderr,"An XML file listing the energy data is required.\n");
        return EMPTY_SET;
    }

    while((cmd_flag = getopt_long(argc,argv,short_opts,long_opts,NULL)) != -1)
      {
	switch(cmd_flag)
	  {
	  case 'h':case '?':
	    print_help();
	    if(cmd_flag == 'h')
	      validationState = GOOD_VALIDATION;
	    else
	      validationState = EMPTY_SET;
	    return validationState;
	  case 'o':
	    validate_mmpbsa_output_filename = optarg;
	    break;
	  case 't':
	    {
	      int retval = load_mmpbsa_tolerance(optarg,tolerance);
	      if(retval != 0)
		  return VALIDATOR_IO_ERROR;
	      break;
	    }
	  case 'v':
	    {
	      validate_mmpbsa_verbosity = 1;
	      if(optarg != NULL)
		{
		  if(sscanf(optarg,"%u",&validate_mmpbsa_verbosity) != 1)
		    {
		      validate_mmpbsa_verbosity = 1;
		    }
		}
	      break;
	    }
	  default:
	    fprintf(stderr,"Unknown flag: %c\n",cmd_flag);
	    return VALIDATOR_IO_ERROR;
	  }
      }

    if(optind + 2 > argc)
      {
	fprintf(stderr,"Validator needs at least two filenames\n");
	return VALIDATOR_IO_ERROR;
      }

    while(optind < argc)
      filenames.push_back(argv[optind++]);

    //validate
    dataSummary = validateMMPBSAWorker(filenames,tolerance,validationState);

    // Save validation data?
    if(validate_mmpbsa_output_filename.size() > 0)
    {
    	std::fstream xmlDoc(validate_mmpbsa_output_filename.c_str(),std::ios::out);
    	if(xmlDoc.is_open() && dataSummary != NULL)
    		xmlDoc << dataSummary->toString();
    	xmlDoc.close();
    }

    // Provide output
    if(validationState == GOOD_VALIDATION)
      std::cout << "Files are valid." << std::endl;
    else
      {
	std::cout << "Files are not valid. ";
	if(validate_mmpbsa_verbosity)
	  std::cout << "Validation state: " << validationState;
	std::cout << std::endl;
      }

    return validationState;
}
#endif //standalone
