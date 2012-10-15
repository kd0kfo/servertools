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


#endif
