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

int init_boinc_result(PyObject *module);

struct RESULT;
PyObject* import_result(PyObject *module, const char *variable_name, const std::vector<std::string> *paths, const RESULT& result);

PyObject* py_user_code_on_results(int num_results, const RESULT *r1, void* _data1, RESULT const *r2, void* _data2, const char *function_dict_name);

PyObject* py_user_code_on_workunit(std::vector<RESULT>& results, RESULT *canonical_result, const char *function_dict_name);

PyObject*  py_boinctools_on_result(const RESULT& r, const char *function_name);
PyObject* py_boinctools_on_workunit(const std::vector<RESULT>& results, const RESULT *canonical_result , const char *function_name);

#endif
