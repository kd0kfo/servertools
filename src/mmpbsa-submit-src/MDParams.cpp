#include "MDParams.h"

void MDParams::add_parameters(const std::string& new_parameters)
{
	using std::string;
	using namespace mmpbsa_utils;
	mmpbsa_utils::StringTokenizer parameters(new_parameters,",");
	string strParameters;

	std::pair<MDParams::key_type,MDParams::mapped_type> currPair;
	while(parameters.hasMoreTokens())
	{
		strParameters = parameters.nextToken();
		if(strParameters == "")
			continue;
		string::size_type eqlLoc = strParameters.find_first_of("=");
		if(eqlLoc != string::npos)
		{
			currPair.first = trimString(strParameters.substr(0,eqlLoc));
			currPair.second = trimString(strParameters.substr(eqlLoc+1,strParameters.size()-1-eqlLoc));
		}
		else
		{
			currPair.first = trimString(strParameters);
			currPair.second = "";
		}
		if(currPair.first == "scee" || currPair.first == "scnb")
		  continue;
		this->insert(currPair);
	}
}


void mdin_t::setID(const size_t& newid)
{
  mdin_t::iterator namelist = this->begin();
  for(;namelist != this->end();namelist++)
    namelist->setID(newid);
}

const size_t& mdin_t::getID()const
{
  if(this->size() == 0)
    throw GridException("mdin_t::getID: No id for empty mdin_t");
  
  return this->at(0).getID();
}

void mdin_t::setFilename(const std::string& newFilename)
{
  mdin_t::iterator namelist = this->begin();
  for(;namelist != this->end();namelist++)
    namelist->setFilename(newFilename);
}

const std::string& mdin_t::getFilename()const
{
  if(this->size() == 0)
    throw GridException("mdin_t::getFilename: No id for empty mdin_t");
  
  return this->at(0).getFilename();
}


void  mdin_t::setGFlopCount(const float& newGFlopCount)
{
   mdin_t::iterator namelist = this->begin();
  for(;namelist != this->end();namelist++)
    namelist->setGFlopCount(newGFlopCount);
}

const float& mdin_t::getGFlopCount()const
{
  if(this->size() == 0)
    throw GridException("mdin_t::getFilename: No gflop count for empty mdin_t");

  return this->at(0).getGFlopCount();
}

void mdin_t::setSnapCount(const size_t& newSnapCount)
{
	mdin_t::iterator namelist = this->begin();
	for(;namelist != this->end();namelist++)
		namelist->setSnapCount(newSnapCount);
}

const size_t& mdin_t::getSnapCount()const
{
	if(this->size() == 0)
		throw GridException("mdin_t::getSnapCount: No snap count for empty mdin_t");

	return this->at(0).getSnapCount();
}
