//
// ServerTools
// Copyright (C) 2012 David Coss, PhD
//
// You should have received a copy of the GNU General Public License
// in the file COPYING.  If not, see <http://www.gnu.org/licenses/>.

//
// This program embeds python into the BOINC validator. The three
// validation routines, init_results, compare_results and clean_result
// all package the result objects into Python objects and call 
// corresponding Python functions.
//
// Usage: In a Python source code file, define a dict entry in 
// validators that maps the application id (as a string) to the 
// name of the Python function that should be called. See the
// example/ subdirectory for examples of these files.
//


#include <Python.h>
#include <structmember.h>
#include <vector>
#include <string>

#include "boinc/error_numbers.h"
#include "boinc/boinc_db.h"
#include "boinc/sched_util.h"
#include "boinc/validate_util.h"

#include "pyboinc.h"


/**
 * Takes a RESULT objects and initializes the data set for it.
 *
 * This function then calls the Python functions boinctools.update_process,
 * with the result as an argument. This allows users to perform any
 * initialization functions they may wish to use.
 * 
 */
int init_result(RESULT& result, void*& data) 
{
  OUTPUT_FILE_INFO fi;
  std::vector<std::string> *paths;

  int retval = 0;
  
  paths = new std::vector<std::string>;

  get_output_file_paths(result,*paths);

  data = (void*)paths;

  return retval;
}


/**
 * Using the application id (appid) and the validators dict from the users Python code, this routine decides which user Python code to run to validate two results.
 */
int compare_results(RESULT& r1, void* _data1, RESULT const&  r2, void* _data2, bool& match) 
{
  PyObject *retval;

  initialize_python();
#if 0 // OLD
  retval = py_user_code_on_results(2,&r1,_data1,&r2,_data2,"validators");
  if(retval == Py_None || retval == NULL)
    {
      fprintf(stderr,"There was a python error when validating %s.\nExiting.\n",r1.name);
      if(retval == NULL)
	fprintf(stderr,"validator function returned NULL.\n");
      else if(retval == Py_None)
	fprintf(stderr,"validator function returned None.\n");
      PyErr_Print();
      finalize_python();
      exit(1);
    }
  
  match = (PyObject_IsTrue(retval));

  Py_XDECREF(retval);
#else// NEW
  std::string command;
  
  // Import Module
  if(PyRun_SimpleString("import boinctools"))
    {
      fprintf(stderr,"Could not import boinctools python module.\n");
      if(PyErr_Occurred())
	PyErr_Print();
      finalize_python();
      exit(1);
    }
  
  // Create Result Class
  command = "res1 = " + result_init_string(r1);
  if(PyRun_SimpleString(command.c_str()))
    {
      fprintf(stderr,"Could not create result object.\n");
      fprintf(stderr,"Python command: %s",command.c_str());
      if(PyErr_Occurred())
	PyErr_Print();
      finalize_python();
      exit(1);
    }
  load_paths("res1",r1,_data1);

  command = "res2 = " + result_init_string(r2);
  if(PyRun_SimpleString(command.c_str()))
    {
      fprintf(stderr,"Could not create result object.\n");
      fprintf(stderr,"Python command: %s",command.c_str());
      if(PyErr_Occurred())
	PyErr_Print();
      finalize_python();
      exit(1);
    }
  load_paths("res2",r2,_data2);

  command = "is_valid = boinctools.validate(res1,res2)";
  if(PyRun_SimpleString(command.c_str()))
    {
      fprintf(stderr,"Could not validate result objects.\n");
      fprintf(stderr,"Python command: %s",command.c_str());
      if(PyErr_Occurred())
	PyErr_Print();
      finalize_python();
      exit(1);
    }

  PyObject *m = PyImport_AddModule("__main__");
  PyObject *result = PyObject_GetAttrString(m,"is_valid");
  if(result != NULL)
    {
      PyObject *str = PyObject_Str(result);
      if(str)
	printf("Valid? %s\n",PyString_AsString(str));
      match = PyObject_IsTrue(result);
      Py_XDECREF(str);
    }
  else
    {
      fprintf(stderr,"Could not check validity of results.\n");
      if(PyErr_Occurred())
	PyErr_Print();
      finalize_python();
      exit(1);
    }
  Py_XDECREF(result);

#endif

  return 0;
}

/**
 * This function does two things. First, it calls the Python function
 * boinctools.continue_children. This function may be used to start processes
 * after the work unit has finished. Second, it frees the memory corresponding
 * to the address stored in "data". 
 */
int cleanup_result(RESULT const& r, void* data) 
{
  PyObject *retval = NULL;
  PyObject *result = NULL;

  initialize_python();
#if 0// OLD

  retval = py_user_code_on_results(1,&r,data,NULL,NULL,"cleaners");

  if(retval == Py_None || retval == NULL)
    {
      fprintf(stderr,"There was a python error when cleaning %s.\nExiting.\n",r.name);
      finalize_python();
      exit(1);
    }
  Py_DECREF(retval);
  
  result = py_boinctools_on_result(r,"continue_children");
  Py_XDECREF(result);
    
  if(PyErr_Occurred())
    {
      printf("%s.%s failed\n","boinctools","continue_children");
      PyErr_Print();
      if(data != NULL)
	{
	  delete (std::vector<std::string>*)data;
	  data = NULL;
	}

      return 1;
    }
  
#else// New
  std::string command;
  
  if(PyRun_SimpleString("import boinctools"))
    {
      fprintf(stderr,"Could not import boinctools python module.\n");
      if(PyErr_Occurred())
	PyErr_Print();
      finalize_python();
      exit(1);
    }
  
  // Create Result Class
  command = "res1 = " + result_init_string(r);
  if(PyRun_SimpleString(command.c_str()))
    {
      fprintf(stderr,"Could not create result object.\n");
      fprintf(stderr,"Python command: %s",command.c_str());
      if(PyErr_Occurred())
	PyErr_Print();
      finalize_python();
      exit(1);
    }
  load_paths("res1",r,data);
  
  if(PyRun_SimpleString("boinctools.clean(res1)"))
    {
      fprintf(stderr,"Could not clean result objects.\n");
      fprintf(stderr,"Python command: %s",command.c_str());
      if(PyErr_Occurred())
	PyErr_Print();
      finalize_python();
      exit(1);
    }

#endif

  if(data != NULL)
    {
      delete (std::vector<std::string>*)data;
      data = NULL;
    }

  return 0;
}
