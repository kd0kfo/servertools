#include <Python.h>
#include <structmember.h>
#include <vector>
#include <string>

#include "boinc/error_numbers.h"
#include "boinc/boinc_db.h"
#include "boinc/sched_util.h"
#include "boinc/validate_util.h"

#include "pyboinc.h"

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
	      PyObject *result = NULL;
	      printf("Calling update_process\n");
	      result = PyObject_CallFunction(funct,"(O)",boincresult);
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

	      if(result != NULL && result != Py_None)
		{
		  printf("Result: %s\n",PyString_AsString(PyString_FromFormat("%s",result)));
		  Py_XDECREF(result);
		}
	    }
	}
      Py_XDECREF(boinctools_mod);
    }

  Py_Finalize();

  return retval;
}


PyObject *pyoperate_on_results(int num_results, const RESULT *r1, void* _data1, RESULT const *r2, void* _data2, const char *function_dict_name)
{
  int retval = 0;
  PyObject *main_module = NULL, *main_dict = NULL, *validator_funct = NULL, *funct = NULL, *valid_value = NULL;
  PyObject *pyresult1, *pyresult2;
  BoincResult *obj1, *obj2;
  const char init_filename[] = "/boinc/projects/stjudeathome/boincdag_init.py";
  FILE *init_file = NULL;
  
  if(function_dict_name == NULL)
    return Py_None;
  if(num_results < 1 || num_results > 2)
    return Py_None;
  if(r1 == NULL || _data1 == NULL)
    return Py_None;
  if(num_results == 2 && (r2 == NULL || _data2 == NULL))
    return Py_None;

  obj1 = (BoincResult*)_data1;
  obj2 = (BoincResult*)_data2;
  
  init_file = fopen(init_filename,"r");
  if(init_file == NULL)
    {
      char cwd[FILENAME_MAX];
	  strcpy(cwd,"UNKNOWN");
      if(getcwd(cwd,FILENAME_MAX))
	{
	  fprintf(stderr,"Could not get current working directory. Reason: %s\n",strerror(errno));
	}
      fprintf(stderr,"Could not open %s for reading.\nCWD: %s\n",init_filename,cwd);
      fprintf(stderr,"Reason: %s\n",strerror(errno));
      return Py_None;
    }

  Py_Initialize();

  main_module = PyImport_AddModule("__main__");
  if(main_module == NULL)
    {
      fprintf(stderr,"Could not add module __main__\n");
      if(errno)
	fprintf(stderr,"Reason: %s\n",strerror(errno));
      return Py_None;
    }

  init_boinc_result(main_module);
    
  main_dict = PyModule_GetDict(main_module);
  if(main_dict == NULL)
    {
      fprintf(stderr,"Could not get globals.\n");
      if(errno)
	fprintf(stderr,"Reason: %s\n",strerror(errno));
      return Py_None;
    }

  pyresult1 = import_result(main_module,"result1",(const std::vector<std::string>*)_data1,*r1);
  if(num_results == 2)
    pyresult2 = import_result(main_module,"result2",(const std::vector<std::string>*)_data2,*r2);
    
  PyRun_SimpleString("validators = {}");
  PyRun_SimpleString("cleaners = {}");
  retval = PyRun_SimpleFile(init_file,init_filename);
  fclose(init_file);init_file = NULL;
  if(retval)
    {
      fprintf(stderr,"Error running %s.\n",init_filename);
      return Py_None;
    }
  
  funct = PyObject_GetAttrString(main_module,function_dict_name);
  if(funct == NULL)
    {
      fprintf(stderr,"Missing %s dict.\n",function_dict_name);
      return Py_None;
    }
  funct = PyDict_GetItem(funct, PyString_FromFormat("%d",r1->appid));
  if(funct == NULL)
    {
      fprintf(stderr,"Missing %s for %d.\n",function_dict_name,r1->appid);
      return Py_None;
    }
  validator_funct = PyObject_GetAttr(main_module, funct);
  if(validator_funct == NULL)
    {
      fprintf(stderr,"Missing %s function '%s' for %d.\n",function_dict_name,PyString_AsString(funct),r1->appid);
      return Py_None;
    }
  if(!PyCallable_Check(validator_funct))
    {
      fprintf(stderr,"Object is not callable.\n");
      return Py_None;
    }

  if(num_results == 2)
    valid_value = PyObject_CallObject(validator_funct,Py_BuildValue("OO",pyresult1, pyresult2));
  else
    valid_value = PyObject_CallObject(validator_funct,Py_BuildValue("(O)",pyresult1));

  if(PyErr_Occurred())
    {
      Py_XDECREF(valid_value);
      PyErr_Print();
      return Py_None;
    }

  if(valid_value == NULL)
    {
      fprintf(stderr,"Error running %s (%s).\n",function_dict_name,PyString_AsString(PyObject_GetAttrString(validator_funct,"__name__")));
      return Py_None;
    }
  

  return valid_value;
}

int compare_results(RESULT& r1, void* _data1, RESULT const&  r2, void* _data2, bool& match) 
{
  PyObject *retval;

  Py_Initialize();
  
  retval = pyoperate_on_results(2,&r1,_data1,&r2,_data2,"validators");
  if(retval == Py_None || retval == NULL)
    {
      fprintf(stderr,"There was a python error when validating %s.\nExiting.\n",r1.name);
      exit(1);
    }
  
  match = (PyObject_IsTrue(retval));

  Py_Finalize();

  return 0;
}

int cleanup_result(RESULT const& r, void* data) 
{
  PyObject *retval = Py_None, *mod = Py_None;
  bool child_starter_failed = false;

  Py_Initialize();

  retval = pyoperate_on_results(1,&r,data,&r,NULL,"cleaners");

  if(retval == Py_None || retval == NULL)
    {
      fprintf(stderr,"There was a python error when cleaning %s.\nExiting.\n",r.name);
      exit(1);
    }
  
  mod = PyImport_Import(PyString_FromString("boinctools"));
  if(mod != NULL)
    {
      PyObject *dict, *funct;
      dict = PyModule_GetDict(mod);
      funct = PyDict_GetItemString(dict,"continue_children");
      if(funct != NULL)
	{
	  if(PyCallable_Check(funct))
	    {
	      PyObject *result = NULL, *exception = NULL;
	      result = PyObject_CallFunction(funct,"(s)",r.name);
	      if((exception = PyErr_Occurred()) != NULL)
		{
		  const char *exc_name = PyString_AsString(PyObject_GetAttrString(exception,"__name__"));
		  if(exc_name == NULL || strcmp(exc_name,"NoSuchProcess") != 0)
		    {
		      child_starter_failed = true;
		      printf("Python Exception (%s) happened\n",(exc_name)?exc_name : "NULL");
		      PyErr_Print();
		      exit(1);
		    }
		}
	      if(result != NULL)
		{
		  Py_XDECREF(result);
		}
	      else
		printf("start_children failed\n");
	    }
	}
      Py_XDECREF(mod);
    }
  
  Py_Finalize();

  if(data != NULL)
    {
      delete (std::vector<std::string>*)data;
      data = NULL;
    }

  return 0;
}
