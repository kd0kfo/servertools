/* 
 * GridException
 *
 * Exception class thrown when errors occur within the grid system.
 *
 * Created by David Coss, 2010
 */
#ifndef GRIDEXCEPTION_H
#define	GRIDEXCEPTION_H

#include <stdexcept>
#include <string>
#include <sstream>


#ifdef USE_BOINC
#include "boinc/str_util.h"
#endif


/**
 * Error types use when throwing a GridException, establishing uniformity. Some of these values
 * have error messages which can be produced using error_msg.
 */
enum GridErrorTypes
{
    NO_ERROR = 0,
    UNKNOWN_ERROR = 1,
    MISSING_INPUT_FILE,
    IO_ERROR,
    INVALID_INPUT_PARAMETER,
    DATABASE_ERROR,
    NOT_A_PROCESS,
    INVALID_CLI_ARGUMENT,
    MULTIPLE_ROWS, /* Returned when the process query generated multiple rows, i.e. process id was not unique. THIS IS BAD!*/
    MYSQL_ERROR,
    INVALID_APP_ID,
    UNKNOWN_COMMAND,
    BOINC_ERROR,
    INVALID_WORKUNIT,
    INSUFFICIENT_PERMISSION,
    EXCEEDED_RESOURCE_LIMIT, /* when the bounds of grid resources (i.e. cpu usages, disk space) is exceeded.*/
    NULL_POINTER /*  O_O */
};

class GridException : public std::runtime_error
{
    public:
        /**
         * Very General GridException. Use more specific exception if one is
         * available.
         *
         * @param error
         */
    GridException( const std::string& error) : runtime_error(error) {errorType = UNKNOWN_ERROR;}

    /**
     * Creates an exception, with a specified error type.
     *
     * @param error
     * @param errorType
     */
    GridException(const std::string& error, const GridErrorTypes& errorType) : runtime_error(error){this->errorType = errorType;}
    GridException(const std::ostringstream& error, const GridErrorTypes& errorType) : runtime_error(error.str()){this->errorType = errorType;}
    GridException(const std::ostringstream& error) : runtime_error(error.str()),errorType(UNKNOWN_ERROR){}
    /**
     * Returns the error type, corresponding to the error types listed below.
     * These should be returned if the exception is caught and the program
     * dies gracefully.
     *
     * @return int error type.
     */
    const GridErrorTypes& getErrType()const{return errorType;}


    virtual const char* identifier(){return "General Grid Tool Error";}

private:
    GridErrorTypes errorType;

};

/**
 * Outputs the Error code, Exception message and value returned by error_msg for the GridException Error Code.
 *
 */
std::ostream& operator<<(std::ostream& stream, GridException& ge);

/**
 * Gives an error message based on the provided error code.
 *
 * @param err integer error code.
 */
std::string error_msg(const int& err);

std::string error_msg(const GridException& ge);

#endif	/* GRIDEXCEPTION_H */

