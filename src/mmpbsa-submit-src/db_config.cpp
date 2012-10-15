#include "db_config.h"

//Database declarations that should not be placed in a repository and should have tighter
//file permissions.
//place code here that has passwords that will not get put into the repository.
#include "db_config.secret.cpp"

MYSQL *queue_conn;

MYSQL_RES * query_db(const std::ostringstream& stream)
{
    return query_db(stream.str());
}

MYSQL_RES * query_db(const std::string& sql)
{
	if(queue_conn == 0)
		throw GridException("query_db: Could not query database. No object.",DATABASE_ERROR);

    int retval = mysql_query(queue_conn, sql.c_str());
    if(retval != 0)
        return NULL;

    return mysql_store_result(queue_conn);
}

void disconnect_db()
{
  mysql_close(queue_conn);
  queue_conn = NULL;
}


grid_file_info get_file_info(const int * processID, const std::string * file_name) throw (GridException)
{
	grid_file_info returnMe;

	std::ostringstream sql;
	sql << "select file_name,file_name_alias,file_type,subdirectory,parent from process_input_files where ";
	if(processID == 0 && file_name == 0)
		sql << " 1";
	if(processID == 0)
		sql << " file_name like '" << *file_name << "'";
	if(processID != 0)
	{
		if(file_name != 0)
			sql << " and ";
		sql << "process_ID = " << *processID;
	}
	MYSQL_RES* fileinfo_res = query_db(sql);

	if(fileinfo_res == 0 || mysql_num_rows(fileinfo_res) == 0)
	{
		sql.clear();sql.str("");
		sql << "get_file_info: could not get file info from queue system." << std::endl;
		if(file_name != 0)
			sql << " File name: " << *file_name;
		if(*processID != 0)
			sql << " Process ID: " << *processID;
		throw GridException(sql,MYSQL_ERROR);
	}

	MYSQL_ROW row = mysql_fetch_row(fileinfo_res);
	returnMe = get_file_info(row);

	mysql_free_result(fileinfo_res);
	return returnMe;
}

grid_file_info get_file_info(MYSQL_ROW row) throw (GridException)
{
	grid_file_info returnMe;
	if(row == 0)
		throw GridException("get_file_info: Null Pointer provided in lieu of MYSQL_ROW.",INVALID_INPUT_PARAMETER);
	returnMe.filename = (row[0]) ? row[0] : "";
	returnMe.filename_alias = (row[1]) ? row[1] : "";
	returnMe.filetype = (row[2]) ? row[2] : "";
	returnMe.subdirectory = (row[3]) ? row[3] : "";
	returnMe.parent = (row[4]) ? row[4] : "";
	return returnMe;
}

const grid_file_info& set_file_info(const grid_file_info& file_info, const int& process_id) throw (GridException)
{
	std::ostringstream sql;
	sql << "select * from process_input_files where file_name = " << file_info.filename;
	sql << " and file_type = " << file_info.filetype;
	MYSQL_RES * fileinfo_res = query_db(sql);
	sql.clear();sql.str("");

	if(fileinfo_res == 0 || mysql_num_rows(fileinfo_res) == 0)
	{
	  sql << "insert into process_input_files (ID,process_ID,file_name,file_name_alias,file_type,subdirectory,parent) values (NULL," << process_id << ", ";
		sql << "'" << file_info.filename << "', ";
		if(file_info.filename_alias.size() == 0)
			sql << "NULL, ";
		else
			sql <<"'" << file_info.filename_alias << "', ";
		sql << file_info.filetype << ", ";
		if(file_info.subdirectory.size() == 0)
			sql << "NULL, ";
		else
			sql <<"'" << file_info.subdirectory << "', ";
		if(file_info.parent.size() == 0)
			sql << "NULL ";
		else
			sql << file_info.parent;

		sql << ")";
	}
	else
	{
	  sql << "update process_input_files set process_ID = " << process_id << " file_name = " << file_info.filename << ", file_name_alias = ";
		if(file_info.filename_alias.size() == 0)
			sql << "NULL, ";
		else
			sql <<"'" << file_info.filename_alias << "', ";
		sql << "file_type = " << file_info.filetype;
		sql << ", subdirectory = ";
		if(file_info.filename_alias.size() == 0)
			sql << "NULL, ";
		else
			sql <<"'" << file_info.subdirectory << "', ";
		sql << "parent = ";
		if(file_info.parent.size() == 0)
			sql << "NULL ";
		else
			sql << file_info.parent;
		sql << " where file_name like '" << file_info.filename << "' limit 1;";
	}

	mysql_free_result(fileinfo_res);
	fileinfo_res = query_db(sql);
	mysql_free_result(fileinfo_res);
	if(mysql_errno(queue_conn) != 0)
	{
		sql.clear();sql.str("");
		sql << "set_file_info: could not update file info. " << std::endl;
		sql << "Reason: MySQL Error #" << mysql_errno(queue_conn) << " : " << mysql_error(queue_conn);
		throw GridException(sql,MYSQL_ERROR);
	}
	return file_info;
}
