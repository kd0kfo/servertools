#include <Python.h>

#include <cstdio>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  PyObject *module, *funct, *retval;

  if(argc == 1)
    {
      fprintf(stderr,"Missing filename\n");
      exit(1);
    }

  Py_Initialize();
  PySys_SetArgvEx(argc, argv, 0);

  module = PyImport_Import(PyString_FromString("dag.gsub"));
  if(module == NULL)
    {
      fprintf(stderr,"Could not import gsub\n");
      exit(1);
    }

  funct = PyObject_GetAttrString(module,"gsub");
  if(funct == NULL)
    {
      fprintf(stderr,"Could not import gsub.gsub\n");
      exit(1);
    }

  if(!PyCallable_Check(funct))
    {
      fprintf(stderr,"gsub.gsub is not callable.\n");
      exit(1);
    } 

  retval = PyObject_CallObject(funct,PyTuple_Pack(1,PyString_FromString(argv[1])));
  if(retval == NULL)
    {
      fprintf(stderr,"gsub.gsub failed with submission script \"%s\"\n",argv[1]);
      Py_Finalize();
      return 1;
    }

  if(retval == Py_None)
    return 1;
  
  printf("%s\n",PyString_AsString(PyString_FromFormat("%s",retval)));

  Py_Finalize();


  return 0;
}
