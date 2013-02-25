"""
boinctools
==========

@author: David Coss, PhD
@date: November 7, 2012
@license: GPL version 3 (see COPYING or http://www.gnu.org/licenses/gpl.html for details)

This python module provides interface between BOINC C API and Python user code.
"""

from local_boinc_settings import project_path

class BoincException(Exception):
    """
    General Exception Class used in boinctools routines.
    """
    pass

class BoincResult():
    def __init__(self,name,id,appid,exit_status,validate_state,cpu_time):
        self.name = name
        self.output_files = []
        self.id = id
        self.appid = appid
        self.exit_status = exit_status
        self.validate_state = validate_state
        self.cpu_time = cpu_time

    def __str__(self):
        return "Name: {0}\nID: {1}\nApp ID: {2}\nExit Status: {3}\nValidate State: {4}\nCPU Time: {5}".format(self.name,self.id,self.appid,self.exit_status,self.validate_state,self.cpu_time)

    def add_output_file(self,path,logical_name):
        file_tuple = (path,logical_name)
        self.output_files.append(file_tuple)
  

def dump_traceback(e):
    """
    Writes traceback information to standard output.

    @type e: Exception
    """
    
    import sys
    import traceback
    exc_type, exc_value, exc_traceback = sys.exc_info()
    print("*** print_tb:")
    traceback.print_tb(exc_traceback, limit=1, file=sys.stdout)
    print("*** print_exception:")
    traceback.print_exception(exc_type, exc_value, exc_traceback,
                              limit=2, file=sys.stdout)
    print("*** print_exc:")
    traceback.print_exc()
    print("*** format_exc, first and last line:")
    formatted_lines = traceback.format_exc().splitlines()
    print(formatted_lines[0])
    print(formatted_lines[-1])
    print "*** format_exception:"
    print(repr(traceback.format_exception(exc_type, exc_value,exc_traceback)))
    print("*** extract_tb:")
    print(repr(traceback.extract_tb(exc_traceback)))
    print("*** format_tb:")
    print(repr(traceback.format_tb(exc_traceback)))
    print("*** tb_lineno:", exc_traceback.tb_lineno)


def dir_hier_path(original_filename):
    """
    Wrapper for BOINC dir_hier_path.

    @param original_filename: representation of the filename
    @type original_filename: String
    
    @return: UTF-8 representation of the full path to file under the download hierarchy tree

    @raise BoincException: if dir_hier_path fails.
    """
    import subprocess as SP
    import os
    from os import path as OP
    orig_pwd = os.getcwd()
    os.chdir(project_path)

    retval = SP.Popen(["bin/dir_hier_path",original_filename],stdout=SP.PIPE,stderr=SP.PIPE)
    (tmpl_path,errmsg) = retval.communicate()
    os.chdir(orig_pwd)
    if retval.wait() != 0:
            
        raise BoincException("Error calling dir_hier_path for file \"%s\"\nError Message: %s" % (file.physical_name,errmsg))
    return tmpl_path.decode('utf-8').strip()
        
def stage_files(input_files,source_dir = None, set_grp_perms = True, overwrite = True):
    """
    Stages files to the downloads directory of the BOINC server, using dir_hier_path
    
    @type input_files: dict
    @param input_files: Dictionary that maps physical filename (full path) to the name of the file within the downloads directory (i.e. name used by server).
    @param source_dir: Option parameter to set the source directory of input files
    @type source_dir: String
    @param set_grp_perms: Whether or not Group Permissions Should be set to READ/Write explictly (Default: True)
    @type set_grp_perms: Boolean
    @param overwrite: Whether or not files should be overwritten (Default: True)
    @type overwrite: Boolean
    """
    import os.path as OP
    import os
    import subprocess as SP
    import stat
    
    if not source_dir:
        source_dir = os.getcwd()
    orig_pwd = os.getcwd()
    os.chdir(project_path)
    for source_path in input_files:
        physical_name = OP.basename(source_path)
        unique_name = input_files[source_path]
        tmpl_path = dir_hier_path(unique_name)
        if not overwrite and OP.exists(tmpl_path):
            continue
        retval = SP.Popen("cp {0} {1}".format(source_path,tmpl_path).split(),stdout=SP.PIPE,stderr=SP.PIPE)
        if retval.wait() != 0:
            errmsg = retval.communicate()[1]
            raise BoincException("Could not copy file {0} to {1}.\nError Message: {2}".format(source_path,tmpl_path,errmsg))
        if set_grp_perms:
            os.chmod(tmpl_path,stat.S_IRUSR|stat.S_IWUSR|stat.S_IRGRP|stat.S_IWGRP)
    os.chdir(orig_pwd)

def cancel_workunits(workunit_names):
    """
    Calls a custom cancel_jobs that will remove workunits based on
    workunit name.

    @type workunit_names: List
    @param workunit_names: Workunit names to be canceled
    @return: no return value
    @raise BoincException: if create_work fails
    """
    import os.path as OP
    import os
    import subprocess as SP

    if not workunit_names:
        return

    cmd_list = ["bin/cancel_jobs","--by_name"] + workunit_names

    orig_pwd = os.getcwd()
    os.chdir(project_path)

    retval = SP.Popen(cmd_list,stdout=SP.PIPE,stderr=SP.PIPE)
    if retval.wait() != 0:
        errmsg = retval.communicate()[1]
        raise BoincException("Could not cancel work unit.\nRan: %s\n\nError Message: %s\n" % (" ".join(cmd_list), errmsg.decode('utf-8')))

    os.chdir(orig_pwd)

def schedule_work(cmd,workunit_name,wu_tmpl,res_tmpl,input_filenames):
    """
    Creates a Workunit by calling create_work with the provided parameters.

    @param cmd: Application Name (corresponds to appname in applications database table)
    @type cmd: String
    @param workunit_name: Name to be used for workunit
    @type workunit_name: String
    @param wu_tmpl: File name of workunit template within templates/ subdirectory
    @type wu_tmpl: String
    @param res_tmpl: File name of result template within templates/ subdirectory
    @type res_tmpl: String
    @param input_filenames: List of input file names to be used by the work unit
    @type input_filenames: List of Strings
    """
    import os
    import os.path as OP
    import subprocess as SP
    
    orig_pwd = os.getcwd()
    os.chdir(project_path)
    cmd_args = "--appname %s --wu_name %s --wu_template templates/%s --result_template templates/%s" % (cmd, workunit_name, wu_tmpl, res_tmpl)
    for input_file in input_filenames:
        cmd_args += " %s" % input_file
    cmd_list = [OP.join( OP.join(project_path,"bin") ,"create_work")]
    cmd_list.extend(cmd_args.split())
    retval = SP.Popen(cmd_list,stdout=SP.PIPE,stderr=SP.PIPE)
    if retval.wait() != 0:
        errmsg = retval.communicate()[1]
        raise BoincException("Could not create work.\nRan: %s\n\nError Message: %s\n" % (" ".join(cmd_list), errmsg.decode('utf-8')))
    os.chdir(orig_pwd)

def save_bad_res_output(filename,wuname):
    """
    Copies a result output file and saves it to the invalid_results
    subdirectory of the project. The file is put in a subdirectory
    named after the workunit.

    @param filename: Name of file to be copied
    @type filename: String
    @param wuname: Name of workunit
    @type wuname: String

    @raise BoincException: if the invalid_results directory does not
    exist in the project path.
    """
    
    import os.path as OP
    import shutil

    if not OP.isfile(filename):
        return
    
    bad_res_path = OP.join(project_path,"invalid_results")
    if OP.isdir(bad_res_path):
        import os
        bad_wu_path = OP.join(bad_res_path,wuname)
        if not OP.isdir(bad_wu_path):
            try:
                os.mkdir(bad_wu_path)
            except:
                bad_wu_path = bad_res_path
        shutil.copy(filename,bad_wu_path)
    else:
        raise BoincException("'invalid_results' directory does not exist. Data lost.")

def validate(result1, result2):
    import os.path as OP
    init_filename = OP.join(project_path,"boincdag_init.py")
    variables = {}
    execfile(init_filename, variables)

    function_name = variables['validators'][str(result1.appid)]
    function = variables[function_name]
    is_valid = function(result1,result2)
    return is_valid

def clean(result):
    import os.path as OP

    print("Cleaning %s" % result.name)
    init_filename = OP.join(project_path,"boincdag_init.py")
    variables = {}
    execfile(init_filename, variables)

    function_name = variables['cleaners'][str(result.appid)]
    function = variables[function_name]
    function(result)

def assimilator(result_list,canonical_result):
    import os.path as OP

    print("Assimilating %d results" % len(result_list))
    init_filename = OP.join(project_path,"boincdag_init.py")
    variables = {}
    execfile(init_filename, variables)

    # Get Assimilator
    if not 'assimilators' in variables.keys():
        raise BoincException("Error - 'assimilators' is not a defined dict.")
    assimilators = variables['assimilators']

    # Get appid
    if canonical_result.id:
        appid = str(canonical_result.appid)
    else:
        if not result_list:
            raise BoincException("Error - There are no results provided to the assembler.")
        appid = str(result_list[0].appid)
    if not appid in assimilators:
        raise BoincException("Error - There is no assimilator defined for app #%s" % appid)

    function_name = assimilators[appid]
    if not function_name in variables:
        raise BoincException("Error - '%s' is not a defined function." % function_name)
    function = variables[function_name]
    function(result_list,canonical_result)

