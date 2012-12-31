//
// ServerTools
// Copyright (C) 2012 David Coss, PhD
//
// You should have received a copy of the GNU General Public License
// in the file COPYING.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef PYBOINC_H
#define PYBOINC_H

#include <Python.h>
#include <string>
#include <vector>

typedef struct {
  PyObject_HEAD
  PyObject *name;
  PyObject *output_files;
  int id; // RESULT id
  int appid;
  int exit_status;
  int validate_state;
  double cpu_time;// in seconds
  
}BoincResult;

struct RESULT;

void initialize_python();
void finalize_python();


/**
 * Creates the BoincResult class and places it in the module provided
 * as a parameter.
 *
 * Returns 0 upon success and -1 otherwise.
 *
 * @param module
 * @return int 
 */
int init_boinc_result(PyObject *module);

/**
 * Creates a BoincResult object using the provided path and RESULT data
 * and stores it in the module parameter using the supplied variable name.
 *
 * The variable paths may be NULL, in which case BoincResults.output_files 
 * will equal an empty List.
 *
 * Returns a borrowed reference to the newly created BoincResults object.
 *
 * NOTE: This function will create a new BoincResult object. HOWEVER, that object will belong only to the variable represented by the string variable_name in the provided module. THUS the reference count on the object returned by the function is ONE. Do not call Py_DECREF on the returned value.
 *
 */
PyObject* import_result(PyObject *module, const char *variable_name, const std::vector<std::string> *paths, const RESULT& result);

/**
 * Returns a New Reference
 */
PyObject* py_user_code_on_results(int num_results, const RESULT *r1, void* _data1, RESULT const *r2, void* _data2, const char *function_dict_name);

/**
 * Returns a New Reference
 */
PyObject* py_user_code_on_workunit(std::vector<RESULT>& results, RESULT *canonical_result, const char *function_dict_name);

/**
 * Returns a New Reference
 */
PyObject*  py_boinctools_on_result(const RESULT& r, const char *function_name);
PyObject* py_boinctools_on_workunit(const std::vector<RESULT>& results, const RESULT *canonical_result , const char *function_name);

/**
 * Returns a string that represents a Python BoincResult class
 * initialization for the provided Result.
 */
std::string result_init_string(const RESULT& res);

/**
 * For the given variable_name, output file info data is appended to 
 * the variable's "output_files" list.
 */
void load_paths(const std::string& variable_name, const RESULT& res, void *data);

#endif
