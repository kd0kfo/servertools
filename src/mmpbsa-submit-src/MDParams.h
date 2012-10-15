/**
 * Storage object for Sander input parameter files.
 *
 * An MDParams object acts a map for the name lists in the mdin file. MDParams also stores
 * the filename and title of the name list. MDParams are linked to their respective places
 * in the queue system with the ID variable.
 *
 * mdin_t is a list of the namelists; essentially, mdin_t is an object for each mdin file.
 *
 * Created by David Coss, 2010
 */

#ifndef MDPARAMS_H
#define	MDPARAMS_H

#include <map>
#include <string>
#include <vector>

#include "libmmpbsa/StringTokenizer.h"
#include "libmmpbsa/mmpbsa_utils.h"

#include "GridException.h"

class MDParams : public std::map<std::string,std::string> {
public:
    MDParams() : std::map<std::string,std::string>(),title(""),filename(""),
      id(0),namelist(""),gflop_count(0),snap_count(0){parent_id = 0;}
    MDParams(const std::string& newTitle) : std::map<std::string,std::string>(),
        title(newTitle),filename(""),id(0),namelist(""),gflop_count(0),snap_count(0){parent_id = 0;}
    MDParams(const std::string& newTitle, const std::map<std::string,std::string>& newMap) :
      std::map<std::string,std::string>(newMap),title(newTitle),filename(""),id(0),namelist(""),gflop_count(0),snap_count(0){parent_id = 0;}
    
    virtual ~MDParams(){}

    /**
     * Returns a reference to a namelist title.
     */
    const std::string& getTitle()const{return title;}

    /**
     * Changes the namelist title.
     */
    void setTitle(const std::string& newTitle){title = newTitle;}

    /**
     * Returns a reference to a namelist file name.
     */
    const std::string& getFilename()const{return filename;}

    /**
     * Changes the namelist filename.
     */
    void setFilename(const std::string& newFilename){filename = newFilename;}

    /**
     * Returns a reference to the queue system ID for the process in
     * which the MDParams object belongs.
     */
    const size_t& getID()const{return id;}

    /**
     * Changes the ID for the process in which the MDParams object belongs.
     */
    void setID(const size_t& newID){id = newID;}

    /**
     * Returns a reference to the string equivalent of the namelist which
     * was taken directly from the mdin file.
     */
    const std::string& getNamelist()const{return namelist;}

    /**
     * Changes the string of the namelist.
     */
    void setNamelist(const std::string& newNamelist){namelist = newNamelist;}

    /**
     * Parses this string of parameters, as they would appear in an mdin file
     * and adds them to the MDParam object. The left hand side of the equals
     * is the key value and the right hand side is the object value.
     */
    void add_parameters(const std::string& new_parameters);

    /**
     * Returns a reference to the ID of the proceeding process of the process to which
     * the MDParams object belongs. If this object is the first process,
     * this value is zero.
     */
    const size_t& getParentID()const{return parent_id;}

    /**
     * Changes the ID of the proceeding process of the process to which
     * the MDParams object belongs. If this object is the first process,
     * this value should be zero.
     */
    void setParentID(const size_t& newParentID){parent_id = newParentID;}

    /**
     * Sets the estimated number of floating point operations to be 
     * performed (*10^9)
     */
    void setGFlopCount(const float& newGFlopCount){gflop_count = newGFlopCount;}
    
    /**
     * Gives an estimate of the amount of floating point operations performed.
     * (in units of 10^9 floating point operations)
     */
    const float& getGFlopCount()const{return gflop_count;}
    

    /**
     * Returns a reference to the ID of the proceeding process of the process to which
     * the MDParams object belongs. If this object is the first process,
     * this value is zero.
     */
    const size_t& getSnapCount()const{return snap_count;}

    /**
     * Changes the ID of the proceeding process of the process to which
     * the MDParams object belongs. If this object is the first process,
     * this value should be zero.
     */
    void setSnapCount(const size_t& newSnapCount){snap_count = newSnapCount;}

private:
    std::string title,filename,namelist;
    size_t id,parent_id,snap_count;
    float gflop_count;
};

class mdin_t : public std::vector<MDParams>
{
 public:
  mdin_t() : std::vector<MDParams>(){}
    
    virtual ~mdin_t(){}
    
    void setID(const size_t&);
    const size_t& getID()const;
    const std::string& getFilename()const;
    void setFilename(const std::string& newFilename);
    void setGFlopCount(const float& newGFlopCount);
    const float& getGFlopCount()const;
    void setSnapCount(const size_t& newSnapCount);
    const size_t& getSnapCount()const;
      };

#endif	/* MDPARAMS_H */

