#include <Python.h>
#include <vector>

#include "boinc/boinc_db_types.h"

#include "validate_util.h"
#include "pyboinc.h"

int assimilate_handler(WORKUNIT& wu, std::vector<RESULT>& results, RESULT& canonical_result)
{
  PyObject *mod = Py_None;
  PyObject *main_module = NULL;
  bool child_starter_failed = false;
  const char funct_name[] = "assimilate_workunit";

  printf("\nUSING ASSIMILATE_HANDLER!!!!\n");

  Py_Initialize();

  main_module = PyImport_AddModule("__main__");
  if(main_module == NULL)
    {
      fprintf(stderr,"Could not add module __main__\n");
      if(errno)
	fprintf(stderr,"Reason: %s\n",strerror(errno));
      Py_Finalize();
      return 42;
    }
  if(init_boinc_result(main_module) != 0)
    return -1;
      
  mod = PyImport_Import(PyString_FromString("dag_utils"));
  if(mod != NULL)
    {
      PyObject *dict, *funct;
      std::vector<std::string> paths;
      dict = PyModule_GetDict(mod);
      funct = PyDict_GetItemString(dict,funct_name);
      if(funct != NULL)
	{
	  if(PyCallable_Check(funct))
	    {
	      PyObject *funct_result = NULL, *result_list;
	      PyObject *exception = NULL;
	      result_list = PyList_New(0);
	      if(result_list == NULL)
		{
		  printf("Could not create result list.\n");
		  return 42;
		}
	      for(std::vector<RESULT>::iterator result = results.begin();result != results.end();result++)
		{
		  paths.clear();
		  get_output_file_paths(*result,paths);
		  PyList_Append(result_list,import_result(main_module,"a",&paths,*result));
		}
	      if((exception = PyErr_Occurred()) != NULL)
		{
		  printf("There was an error while loading results.\n");
		  PyErr_Print();
		  Py_Finalize();
		  return 42;
		}
  
	      printf("Calling assimilate_workunit with canonical result %d.\n",canonical_result.id);
	      paths.clear();
	      get_output_file_paths(canonical_result,paths);
	      funct_result = PyObject_CallFunction(funct,"(OO)",result_list,import_result(main_module,"a",&paths,canonical_result));
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
	      if(funct_result != NULL)
		{
		  Py_XDECREF(funct_result);
		}

	    }
	  else
	    {
	      printf("%s is not callable!\n",funct_name);
	      return 42;
	    }
	}
      else
	{
	  printf("Function '%s' was not found.\n",funct_name);
	  return 42;
	}
      Py_XDECREF(mod);
    }
  else
    {
      printf("Could not load dag_utils module.\n");
      return 42;
    }
  
  Py_Finalize();

  return 0;
}
