#include <cstdio>
#include <Python.h>
#include <vector>

#include "boinc/error_numbers.h"
#include "boinc/boinc_db.h"
#include "boinc/sched_util.h"
#include "boinc/validate_util.h"
#include "assimilate_handler.h"

WORKUNIT wu;
RESULT result1,result2;
void *data1 = NULL, *data2 = NULL;

int test_module_found()
{
  const char *needed_modules[] = {"boinctools",NULL};
  size_t index = 0;

  printf("Checking for necessary modules\n");
  Py_Initialize();
  
  while(needed_modules[index] != NULL)
    {
      if(PyImport_ImportModule(needed_modules[index]) == NULL)
	{
	  printf("Could not load module %s\n",needed_modules[index]);
	  Py_Finalize();
	  return 1;
	}
      index++;
    }


  PyRun_SimpleString("import boinctools");

  if(PyErr_Occurred())
    PyErr_Print();
  
  Py_Finalize();

  return 0;
}

int test_validator_init_result()
{
  extern int init_result(RESULT& result, void*& data);

  printf("Testing init_result in pyvalidator.cpp\n");

  // fake parameters
  strcpy(wu.name,"test-workunit");
  strcpy(result1.name, "test-workunit_0");
  strcpy(result2.name, "test-workunit_1");
  result1.appid = result2.appid = 42;
  result1.exit_status = result2.exit_status = 42;

  strcat(result1.xml_doc_in,"<file_ref> \
        <file_name>ple-773564750_0_0</file_name> \
        <open_name>hid_UTR.fasta.out</open_name> \
        <copy_file/> \
    </file_ref>");
  strcpy(result2.xml_doc_in,result1.xml_doc_in);

  return init_result(result1, data1) || init_result(result2,data2);
}

int test_validator_compare_result()
{
  extern int compare_results(RESULT& r1, void* _data1, RESULT const& r2, void* _data2, bool& match);

  bool match = false;
  int retval = 0;
  RESULT different;

  printf("Testing compare_results in pyvalidator.cpp\n");
  
  printf("Comparing same results...\n");
  retval = compare_results(result1, data1, result2, data2, match);
  retval =  !match || retval;
  if(retval)
    return retval;
  
  printf("Comparing different results...\n");
  different = result1;
  different.appid = 123;
  retval = compare_results(result1,data1,different,data2,match);
  
  return retval || match;
}

int test_validator_clean_result()
{
  extern int cleanup_result(RESULT const& r, void* data);

  printf("Testing cleanup_result in pyvalidator.cpp\n");
  
  return cleanup_result(result1, data1) || cleanup_result(result2, data2);
}


int test_assimilate_handler()
{
  int retval;
  printf("Testing assimilate_handler.\n");
  std::vector<RESULT> results;
  results.push_back(result1);
  results.push_back(result2);
  retval = assimilate_handler(wu,results,result1);
  
  return retval;
}

int main(int argc, char **argv)
{
  int retval;
  int pass_counter = 0;

  if((retval = test_module_found()) != 0)
    {
      printf("FAILED: Module Import\n");
      return retval;
    }
  else
    {
      printf("PASSED\n\n");
      pass_counter++;
    }
  
  if((retval = test_validator_init_result()) != 0)
    {
      printf("FAILED: Validator init_result\n");
      return retval;
    }
  else
    {
      printf("PASSED\n\n");
      pass_counter++;
    }

  if((retval = test_validator_compare_result()) != 0)
    {
      printf("FAILED: Validator compare_result\n");
      return retval;
    }
  else
    {
      printf("PASSED\n\n");
      pass_counter++;
    }

  if((retval = test_validator_clean_result()) != 0)
    {
      printf("FAILED: Validator clean_result\n");
      return retval;
    }
  else
    {
      printf("PASSED\n\n");
      pass_counter++;
    }

  if((retval = test_assimilate_handler()) != 0)
    {
      printf("FAILED: Validator clean_result\n");
      return retval;
    }
  else
    {
      printf("PASSED\n\n");
      pass_counter++;
    }



  printf("%d tests passed\n",pass_counter);

  if(access("jobs.dag",W_OK) == 0)
    unlink("jobs.dag");
  
  return 0;
}

