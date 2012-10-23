"""
boinctools
===========

"""

project_path = '/boinc/projects/stjudeathome'

class BoincException(Exception):
    """
    General Exception Class used in boinctools routine.
    """
    pass

def dump_traceback(e):
    """
    Writes traceback information to standard output.

    Arguments: Exception
    No return value.
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

    Arguments: original_filename String representation of the filename
    
    Returns: UTF-8 representation of the full path to file under the download hierarchy tree

    @throw: BoincException if dir_hier_path fails.
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
    
    @type input_file: dict
    @param input_file: Dictionary that maps physical filename (full path) to the name of the file within the downloads directory (i.e. name used by server).
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
    @throw: Exception if create_work fails
    """
    import os.path as OP
    import os
    import subprocess as SP

    if not procs:
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

    Arguments:
    \tfilename -- String name of file to be copied
    \twuname -- String name of workunit

    Returns nothing.

    Raises a BoincException if the invalid_results directory does not
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
