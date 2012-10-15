#include "GridException.h"

std::string error_msg(const int& err)
{
    switch(err)
    {
    case MISSING_INPUT_FILE:
    	return "An input file was expected, but not provided.";
    case INVALID_INPUT_PARAMETER:
    	return "An input parameter to a subroutine is invalid.";
    case DATABASE_ERROR:
    	return "An error occured within the queue system database. This is in contrast to a MYSQL error, which is a problem with the database itself, not the contents of the database.";
    case INVALID_CLI_ARGUMENT:
    	return "The provided command line argument is not valid.";
    case MYSQL_ERROR:
    	return "There is a problem with the mysql server/database.";
    case NO_ERROR:
    	return "Success";
    case MULTIPLE_ROWS:
    	return "Process query returned multiple rows. Double check database.";
    case NOT_A_PROCESS:
    	return "The process id supplied does not represent a valid process.";
    case INVALID_APP_ID:
    	return "The application id associated with the process does not match known applications.";
    case UNKNOWN_COMMAND:
    	return "The provided command is unknown.";
    case BOINC_ERROR:
    	return "Error within the BOINC system.";
    case IO_ERROR:
    	return "Error on file read/write.";
    case INSUFFICIENT_PERMISSION:
    	return "You do not have the right to alter this process.";
    default:
    	break;
    }
    std::ostringstream buff;
    buff << "Error #" << err;
    buff.setf(std::ios_base::hex,std::ios_base::basefield);
    buff << " (0x" << err << ")";
    buff.setf(std::ios_base::dec,std::ios_base::basefield);
#ifdef USE_BOINC
    buff << std::endl << "BOINC Message (in case this is a boinc error): " << boincerror(err);
#endif
    return buff.str();
}

std::string error_msg(const GridException& ge){return error_msg(ge.getErrType());}

std::ostream& operator<<(std::ostream& stream, GridException& ge)
{
	stream << ge.identifier() << "(" << ge.getErrType() << "): " << ge.what() << std::endl;
	stream << "Error Message for this error return value:" << std::endl << error_msg(ge.getErrType()) << std::endl;
	return stream;
}

