//
// ServerTools
// Copyright (C) 2012 David Coss, PhD
//
// You should have received a copy of the GNU General Public License
// in the file COPYING.  If not, see <http://www.gnu.org/licenses/>.
//
// This source code embeds Python functions into the BOINC assimilator.
// 
// Usage: For each Application ID (appid), add an entry to the "assimilators" dict
// in the project init file that maps the appid, as a String, to the function
// to be called, with the Result as an argument.
//

#include "validate_util.h"
#include "pyboinc.h"

#include "boinc/boinc_db_types.h"

#include <Python.h>
#include <vector>


int assimilate_handler(WORKUNIT& wu, std::vector<RESULT>& results, RESULT& canonical_result)
{
  PyObject *retval;

  initialize_python();
  
  retval = py_user_code_on_workunit(results, &canonical_result, "assimilators");
  if(retval == NULL)
    {
      fprintf(stderr,"[%s:%d] There was a python error when assimilating %s.\nExiting.\n",__FILE__,__LINE__,canonical_result.name);
      finalize_python();
      exit(1);
    }
  Py_DECREF(retval);

  if(PyErr_Occurred())
    {
      printf("Assimilation failed\n");
      PyErr_Print();
      finalize_python();
      return 1;
    }
  
  return 0;


}
