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

#include <Python.h>
#include <vector>

#include "boinc/boinc_db_types.h"

#include "validate_util.h"
#include "pyboinc.h"

int assimilate_handler(WORKUNIT& wu, std::vector<RESULT>& results, RESULT& canonical_result)
{
  PyObject *retval;

  Py_Initialize();
  
  retval = py_user_code_on_workunit(results, &canonical_result, "assimilators");
  if(retval == NULL)
    {
      fprintf(stderr,"[%s:%s] There was a python error when assimilating %s.\nExiting.\n",__FILE__,__LINE__,canonical_result.name);
      exit(1);
    }

  if(PyErr_Occurred())
    {
      printf("Assimilation failed\n","boinctools","continue_children");
      PyErr_Print();
      Py_Finalize();
      return 1;
    }
  
  
  Py_Finalize();

  return 0;


}
