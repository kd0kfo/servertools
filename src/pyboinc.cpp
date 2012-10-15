#include <vector>
#include <string>
#include <Python.h>
#include <structmember.h>

#include "boinc/validate_util.h"
#include "boinc/boinc_db_types.h"

#include "pyboinc.h"


static void BoincResult_dealloc(BoincResult *self)
{
  if(self == NULL)
    return;

  //printf("Dealloc'ed \n");
  Py_XDECREF(self->name);
  Py_XDECREF(self->output_files);
  self->ob_type->tp_free((PyObject*)self);
  
  
}

static PyMemberDef pyboinc_RESULT_members[] = {
  {"name", T_OBJECT_EX, offsetof(BoincResult,name)},
  {"output_files", T_OBJECT_EX, offsetof(BoincResult,output_files)},
  {"id", T_INT, offsetof(BoincResult,id)},
  {"appid", T_INT, offsetof(BoincResult,appid)},
  {"exit_status", T_INT, offsetof(BoincResult,exit_status)},
  {"validate_state", T_INT, offsetof(BoincResult,validate_state)},
  {"cpu_time", T_DOUBLE, offsetof(BoincResult,cpu_time)},
  {NULL}
};

static PyTypeObject pyboinc_RESULT = {
  PyObject_HEAD_INIT(NULL)
  0,
  "BoincResult",
  sizeof(BoincResult),
  0,                         /*tp_itemsize*/
  (destructor)BoincResult_dealloc,                         /*tp_dealloc*/
  0,                         /*tp_print*/
  0,                         /*tp_getattr*/
  0,                         /*tp_setattr*/
  0,                         /*tp_compare*/
  0,                         /*tp_repr*/
  0,                         /*tp_as_number*/
  0,                         /*tp_as_sequence*/
  0,                         /*tp_as_mapping*/
  0,                         /*tp_hash */
  0,                         /*tp_call*/
  0,                         /*tp_str*/
  0,                         /*tp_getattro*/
  0,                         /*tp_setattro*/
  0,                         /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT,        /*tp_flags*/
  "Boinc Result Struct",           /* tp_doc */
};

static PyObject* BoincResult_new(PyTypeObject *type, PyObject *args, PyObject *kwrds)
{
  BoincResult *obj;
  obj = (BoincResult*)type->tp_alloc(type,0);
  if(obj == NULL)
    return (PyObject*)obj;

  obj->name = PyString_FromString("UNINITIALIZED");
  if(obj->name == NULL)
    {
      printf("Could not initialize the string: name\n");
      Py_DECREF(obj);
      return NULL;
    }

  obj->output_files = PyList_New(0);
  if(obj->output_files == NULL)
    {
      printf("Could not initialize the list: output_files\n");
      Py_DECREF(obj);
      return NULL;
    }
  Py_INCREF(obj->output_files);

  obj->appid = 0;
  obj->id = 0;
  obj->exit_status = 0;
  obj->cpu_time = 0.0;
  obj->validate_state = 0;
  
  return (PyObject *)obj;
  
}



static int BoincResult_init(BoincResult *self, PyObject *args, PyObject *kwds)
{
  
  PyObject *name=NULL,  *tmp = NULL;
  int id, appid, exit_status,validate_state;
  double cpu_time;
  static char *kwlist[] = {"name","appid","exit_status","validate_state","id","cpu_time",NULL};


  if(!PyArg_ParseTupleAndKeywords(args,kwds,"|Oiiid",kwlist,&name,&appid,&exit_status,&id,&cpu_time))
    return -1;

  if(name)
    {
      tmp = self->name;
      Py_INCREF(name);
      self->name = name;
      Py_XDECREF(tmp);
    }

  self->appid = appid;
  self->id = appid;
  self->exit_status = exit_status;
  self->cpu_time = cpu_time;
  self->validate_state = validate_state;

  return 0;
  
}

PyObject* import_result(PyObject *module, const char *variable_name, const std::vector<std::string> *paths, const RESULT& result)
{
  PyObject *boincresult, *tmp;
  BoincResult *the_struct = NULL;

  if(module == NULL || variable_name == NULL)
    {
      fprintf(stderr,"NULL pointer for module or variable name\n");
      return NULL;
    }

  boincresult = BoincResult_new(&pyboinc_RESULT,NULL,NULL);
  if(boincresult == NULL)
    return NULL;

  the_struct = (BoincResult*)boincresult;

    // Set name
  tmp = the_struct->name;
  the_struct->name = PyString_FromString(result.name);
  Py_INCREF(the_struct->name);
  Py_XDECREF(tmp);

  // app id
  the_struct->appid = result.appid;
  the_struct->exit_status = result.exit_status;
  the_struct->cpu_time = result.cpu_time;
  the_struct->validate_state = result.validate_state;
  the_struct->id = result.id;
  if(paths != NULL)
    {
      std::string logical_name,physical_name;
      RESULT non_const_res = result;
      for(std::vector<std::string>::const_iterator path = paths->begin();path != paths->end();path++)
	{
	  //PyList_Append(the_struct->output_files,PyString_FromString(path->c_str()));
	  physical_name = *path;
	  if(get_logical_name(non_const_res,physical_name,logical_name) != 0)
	    printf("[WARNING]\tCould not get logical name for %s",physical_name.c_str());
	  PyList_Append(the_struct->output_files,Py_BuildValue("(ss)",path->c_str(),logical_name.c_str()));
	}
    }

  PyModule_AddObject(module,variable_name,boincresult);

  return (PyObject*)boincresult;
}

int init_boinc_result(PyObject *module)
{
  pyboinc_RESULT.tp_new = BoincResult_new;
  pyboinc_RESULT.tp_init = (initproc)BoincResult_init;// __init__
  pyboinc_RESULT.tp_members = pyboinc_RESULT_members;

  if(PyType_Ready(&pyboinc_RESULT) < 0)
    {
      fprintf(stderr,"Could not ready pyboinc_RESULT\n");
      return -1;
    }
  Py_INCREF(&pyboinc_RESULT);
  PyModule_AddObject(module, "BoincResult", (PyObject *)&pyboinc_RESULT);

  return 0;

}
