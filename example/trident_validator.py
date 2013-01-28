def update_dag_job_state(result,is_valid):
	import dag.boinc
	from dag import States,strstate
	import dag

	root_dag = dag.boinc.result_to_dag(result.name)
	if not root_dag:
		print("Could not get DAG file for %s" % result.name)
		return
	wuname = dag.boinc.name_result2workunit(result.name)
	proc = root_dag.get_process(wuname)
	if not proc:
		print("In dag %s, could not find workunit %s" % (root_dag.filename, wuname))
		return

	if is_valid:
		proc.state = States.SUCCESS
	else:
		proc.state = States.FAIL
	print("Marking %s as %s" % (result.name, strstate(proc.state)))
	try:
		root_dag.save()
	except dag.DagException as de:
		print("Could not save dag file '%s'" % root_dag.filename)
		raise de

def validate(result1, result2):
	import dag.util as dagu
	import dag
	import re
	from os import path as OP
	import os

	print("Validating %s & %s" % (result1.name, result2.name))

	output_file1 = result1.output_files[0][0]
	output_file2 = result2.output_files[0][0]

	# Check if the files are missing. If they are missing
	# and the computation did actually run, there simply were
	# no hits found.
	# Short DNA segments may be very quick (especially in linux)
	# These short runs may produce no hits, which will mean there
	# is no output file (zero bytes).
	## if result1.cpu_time < 60 and result2.cpu_time < 60:
## 		if not OP.isfile(output_file1) and not OP.isfile(output_file2):
## 			valid = True
## 			update_dag_job_state(result1,valid)
## 			return valid
	
## 	if not OP.isfile(output_file1) and result1.cpu_time > 60:
## 		valid = False
## 		update_dag_job_state(result1,valid)
## 		return valid
## 	if not OP.isfile(output_file2) and result2.cpu_time > 60:
## 		valid = False
## 		update_dag_job_state(result1,valid)
## 		return valid

	# If BOTH are empty results, no output was produced -> Consensus
	# If one or the other is empty, but not both, no Consensus
	if not OP.isfile(output_file1) and not OP.isfile(output_file2):
		valid = True
		try:
			update_dag_job_state(result1,valid)
		except dagu.NoDagMarkerException as ndme:
			print("Warning: Dag Marker not found.")
			print("Message:")
			print(ndme.message)
 		return valid
	if not OP.isfile(output_file1) or not OP.isfile(output_file2):
		valid = False
		try:
			update_dag_job_state(result1,valid)
		except dagu.NoDagMarkerException as ndme:
			print("Warning: Dag Marker not found.")
			print("Message:")
			print(ndme.message)
		return valid	

	valid = (OP.getsize(output_file1) == OP.getsize(output_file2))

	if not valid:
		print("File sizes differ")
		for i in [result1.output_files[0][0], result2.output_files[0][0]]:
			print("Size of %s: %d" % (i,OP.getsize(i)))

	try:
		update_dag_job_state(result1,valid)
	except dagu.NoDagMarkerException as ndme:
		print("Warning: Dag Marker not found.")
		print("Message:")
		print(ndme.message)
	except dag.MissingDAGFile as mdf:
		print("Missing dag file for result '%s'. Skipping process update." % result1.name)
		return True# True to move on and give the volunteer credit

	return valid


## This function prints the number of instances of each class
##
## Example usage:
## refcounts = get_refcounts()
## start = 50
## if start > len(refcounts):
## 	start = 0
## end = 100
## if end > len(refcounts):
## 	end = len(refcounts)
## print("Ref counts[%d - %d]:" % (start,end))
## for n,c in refcounts[start:end]:
## 	print("%10d %s" % (n,c.__name__))
## print("End Ref counts")
#
def get_refcounts(): 
    import sys
    import types
    
    d = {}
    sys.modules
    # collect all classes
    for m in sys.modules.values():
	for sym in dir(m):
	    o = getattr (m, sym)
	    if type(o) is types.ClassType:
		d[o] = sys.getrefcount (o)
    # sort by refcount
    pairs = map (lambda x: (x[1],x[0]), d.items())
    pairs.sort()
    pairs.reverse()
    return pairs

def clean(result):
	import re
	import dag.util as dag_utils
	import boinctools
	import dag,dag.boinc
	import shutil
	import stat
	from os import path as OP
	import os

	if len(result.name) >= 2:
		if result.name[-2:] != "_0":
			print("Not cleaning %s" % result.name)
			return True

    	print("Cleaning %s" % result.name)

	wuname = re.findall(r"^(.*)_\d*$",result.name)
	if len(wuname) == 0:
		print("Malformed result name")
		return None
	wuname = wuname[0]
	try:
		the_dag = dag.boinc.result_to_dag(result.name)
	except dag_utils.NoDagMarkerException as ndme:
		print("Warning: Missing dag")
		print("Skipping clean up" )
		print("Message:")
		print(ndme.message)
		return False
	except dag.MissingDAGFile as mdf:
		print("Missing dag file for result '%s'. Attempting to move output to invalid_results directory" % result.name)
		for output_file in result.output_files:
			boinctools.save_bad_res_output(output_file[0],wuname)
		return False
		
	if not the_dag:
		return False

	dagpath = dag.boinc.marker_to_dagpath(dag.boinc.dag_marker_filename(wuname))
	dagdir = OP.split(dagpath)[0]
	
	print("Getting process %s" % wuname)
	proc = the_dag.get_process(wuname)
	if not proc:
		print("%s was not found in the job batch file %s. Moving resultfile to invalid_results" % (wuname, dagpath))
		for output_file in result.output_files:
			dag.boinc.save_bad_res_output(output_file[0],wuname)
		return False

	if not proc.output_files:
		return True

	source_file = "%s_0" % result.name
	output_file = proc.output_files[0]
	logical_name = output_file.logical_name
	print("Clean filename \"%s\"" % logical_name)

	# Get output dir
	if output_file.dir:
		output_dir = proc.directory # final destination (If it can be written there)
	else:
		output_dir = dagdir

	# If the result is valid, but the data in the bad_results
	# directory.
	if proc.state not in [dag.States.SUCCESS,dag.States.RUNNING]:
		output_dir = OP.join(output_dir,"bad_results") 
		print("Process has not been marked as successful. It is %s instead. Saving output in %s" % (dag.strstate(proc.state), output_dir))
		if not OP.isdir(output_dir):
			os.mkdir(output_dir)

	dest_file = OP.join(output_dir,output_file.logical_name)
	upload_path = boinctools.dir_hier_path(source_file).replace("/download","/upload")
	if not OP.isfile(upload_path):
		print("Output file not found: '%s'" % upload_path)
		return False

	if not OP.isfile(dest_file):
		# Copy file. If it does not exist, move it to the invalid_results directory
		print("Copying {0} to {1}.".format(upload_path,dest_file))
		try:
			shutil.copy(upload_path,dest_file)
			OP.os.chmod(dest_file,stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IWGRP)
		except Exception as e:
			dag.boinc.save_bad_res_output(upload_path,wuname)
			print("ERROR - Could not copy result output file to data directory, %s. It was copied to invalid_results/%s" % (dagdir,wuname))
			print("ERROR - Message:\n%s" % e.message)
			if isinstance(e,IOError):
				print(e.strerror)
			raise e
	else: #output already exists, append.
		try:
			with open(dest_file,"a") as old_file:
				old_file.write("\n")
				old_file.write(open(upload_path,"r").read())
		except Exception as e:
			dag.boinc.save_bad_res_output(upload_path,wuname)
			print("ERROR - Could not copy result output file to data directory, %s. It was copied to invalid_results/%s" % (dagdir,wuname))
			print("ERROR - Message:\n%s" % e.message)
			if isinstance(e,IOError):
				print(e.strerror)
			raise e
	return True

def assimilator(results, canonical_result):

	if canonical_result:
		print("Have canonical result. Exit code: %d" % canonical_result.exit_status)
	if results:
		print("Have %d results. Exit code of first: %d" % (results[0].exit_status, canonical_result.exit_status))
	
