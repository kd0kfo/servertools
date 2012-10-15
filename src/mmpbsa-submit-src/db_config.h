/**
 * db_config
 *
 * General database functions and objects used by the queue system to utilize the database
 * for process management.
 *
 * Created by David Coss, 2010
 */

#ifndef DB_CONFIG_H
#define	DB_CONFIG_H

#include <string>

//Database stuff
#include <mysql/mysql.h>

#include "globals.h"

#include "GridException.h"

/**
 * BOINC project data, e.g. user name and directory.
 */
struct BoincDBConn
{
public:
char DB_HOST_IP[256],BOINC_DB_NAME[256],BOINC_DB_USER[256],BOINC_DB_PASSWORD[256],BOINC_CONFIG_FILE_PATH[4096];
  uid_t BOINC_DAEMON_UID;
};

/**
 * The object is created in a separate file that can be protected, since it needs to contain the
 * database password.
 */
extern BoincDBConn getBoincDBConn();//define variables elsewhere.

/**
 * MySQL database object.
 */
extern MYSQL *queue_conn;

//provide the connection code elsewhere (probably because I do not want to check passwords into a repository...)
extern bool connect_db();

//disconnect the database
extern void disconnect_db();

/**
 * Links id numbers (integral type) to those of the mysql query for process information.
 * This garantees uniformity and simplifies identified elements of a MYSQL_ROW for
 * a process query.
 */
namespace process_fields{enum FIELD_ID{ID,parentid,application,runid,state,user_directory,appname,uid,gflop_estimate,arguments,total_fields};}

namespace file_types{
	static const std::string perminant_input = "1";
	static const std::string perminant_output = "2";
	static const std::string temporary_input = "3";
	static const std::string temporary_output = "4";
}

/**
 * Mask for the database values of the state of processes within the queue system.
 */
namespace queue_states{
    static const std::string success = "1";
    static const std::string waiting = "2";
    static const std::string running = "3";
    static const std::string invalid = "4";
}

typedef struct {
	std::string filename,filename_alias,filetype,subdirectory,parent;
}grid_file_info;

/**
 * Retrieves file information for a specific file in the queue system. The parameters are pointers
 * to a file name string or process id used to retrieve the file information. Pointers are used
 * so that one or more descriptors can be used to query the database. A parameter with a null
 * pointer will not be used in the "where" section of the query. If all parameters are null,
 * all files will be retrieved, but the first one will be returned.
 *
 * An exception is thrown if there is a database error.
 */
grid_file_info get_file_info(const int * processID, const std::string * file_name) throw (GridException);

/**
 * Fills the grid_file_info structure with the values of the row.
 * The order of the fields *must be* "file_name,file_name_alias,file_type,subdirectory"
 */
grid_file_info get_file_info(MYSQL_ROW db_file_info) throw (GridException);


/**
 * Inserts or updates file information for the specified file.
 */
const grid_file_info& set_file_info(const grid_file_info& file_info, const int& process_id) throw (GridException);

/**
 * ostringstream overload of query_db(const std::string& sql)
 */
MYSQL_RES * query_db(const std::string& sql);

/**
 * Performs mysql_query on queue_conn. Then mysql_store_result is returned.
 */
MYSQL_RES * query_db(const std::ostringstream& stream);

#endif	/* DB_CONFIG_H */

