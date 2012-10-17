#include <Python.h>


#include <cstdio>


int main(int argc, char **argv)
{

  Py_Initialize();
  PySys_SetArgvEx(argc, argv, 0);

  PyRun_SimpleString("from dag.update_dag import update_dag as gmod");
  PyRun_SimpleString("gmod()");

  Py_Finalize();


  return 0;
}
