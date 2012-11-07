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
  PyObject *main_module = NULL, *main_dict = NULL, *boincresult, *boinctools_mod = NULL;
  std::vector<std::string> *paths;
  int retval = 0;
  
  Py_Initialize();

  main_module = PyImport_AddModule("__main__");
  if(main_module == NULL)
    {
      fprintf(stderr,"Could not add module __main__\n");
      if(errno)
	fprintf(stderr,"Reason: %s\n",strerror(errno));
      Py_Finalize();
      return ERR_OPENDIR;
    }

  init_boinc_result(main_module);
    
  main_dict = PyModule_GetDict(main_module);
  if(main_dict == NULL)
    {
      fprintf(stderr,"Could not get globals.\n");
      if(errno)
	fprintf(stderr,"Reason: %s\n",strerror(errno));
      Py_Finalize();
      return ERR_OPENDIR;
    }

  paths = new std::vector<std::string>;
  get_output_file_paths(result,*paths);
  data = (void*)paths;

  boincresult = import_result(main_module, "a", paths, result);

  PyRun_SimpleString("print('%s running app number %d' %(a.name, a.appid))");

  printf("Marking Process as closed.\n");
  
  boinctools_mod = PyImport_Import(PyString_FromString("boinctools"));
  if(boinctools_mod != NULL)
    {
      PyObject *dict, *funct, *exception = NULL;
      dict = PyModule_GetDict(boinctools_mod);
      funct = PyDict_GetItemString(dict,"update_process");
      if(funct != NULL)
	{
	  if(PyCallable_Check(funct))
	    {
	      PyObject *pyresult = NULL;
	      printf("Calling update_process\n");
	      pyresult = PyObject_CallFunction(funct,"(O)",boincresult);
	      if((exception = PyErr_Occurred()) != NULL)
		{
		  const char *exc_name = PyString_AsString(PyObject_GetAttrString(exception,"__name__"));
		  if(exc_name == NULL || strcmp(exc_name,"NoSuchProcess") != 0)
		    {
		      printf("Python Exception (%s) happened\n",(exc_name)?exc_name : "NULL");
		      PyErr_Print();
		      exit(1);
		    }
		}

	      if(pyresult != NULL && pyresult != Py_None)
		{
		  PyObject *str_rep = PyObject_Str(pyresult);
		  if(str_rep != NULL)
		    printf("Result: %s\n",PyString_AsString(str_rep));
		  Py_XDECREF(pyresult);
		}
	    }
	}
      Py_XDECREF(boinctools_mod);
    }

  Py_Finalize();

  return retval;
}

/**
 * Using the application id (appid) and the validators dict from the users Python code, this routine decides which user Python code to run to validate two results.
 */
int compare_results(RESULT& r1, void* _data1, RESULT const&  r2, void* _data2, bool& match) 
{
  PyObject *retval;

  Py_Initialize();
  
  retval = py_user_code_on_results(2,&r1,_data1,&r2,_data2,"validators");
  if(retval == Py_None || retval == NULL)
    {
      fprintf(stderr,"There was a python error when validating %s.\nExiting.\n",r1.name);
      exit(1);
    }
  
  match = (PyObject_IsTrue(retval));

  Py_Finalize();

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
  PyObject *retval = Py_None;
  PyObject *result = Py_None;

  Py_Initialize();

  retval = py_user_code_on_results(1,&r,data,&r,NULL,"cleaners");

  if(retval == Py_None || retval == NULL)
    {
      fprintf(stderr,"There was a python error when cleaning %s.\nExiting.\n",r.name);
      exit(1);
    }

  result = py_boinctools_on_result(r,"continue_children");
  if(result != NULL)
    {
      Py_XDECREF(result);
    }
  
  if(PyErr_Occurred())
    {
      printf("%s.%s failed\n","boinctools","continue_children");
      PyErr_Print();
      Py_Finalize();
      if(data != NULL)
	{
	  delete (std::vector<std::string>*)data;
	  data = NULL;
	}

      return 1;
    }
  
  Py_Finalize();

  if(data != NULL)
    {
      delete (std::vector<std::string>*)data;
      data = NULL;
    }

  return 0;
}
