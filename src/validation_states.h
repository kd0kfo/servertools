/*
 * validation_states.h
 *
 *  Created on: Sep 28, 2010
 *      Author: dcoss
 */

#ifndef VALIDATION_STATES_H
#define VALIDATION_STATES_H

enum ValidationStates{
    GOOD_VALIDATION = 0,
    UNEQUAL_NUMBER_OF_SNAPSHOTS,
    MISSING_SNAPSHOT,
    EMPTY_SET,/*No files provided*/
    EMPTY_FILE,/*A file was provided, but it had no data. */
    VALIDATOR_IO_ERROR,/*i.e. could not open the file. */
    BEYOND_TOLERANCE,
    DATA_FORMAT_ERROR,/*When problems occur in the program unrelated to the data on disk */
    XML_FILE_ERROR
};


#endif /* VALIDATION_STATES_H_ */
