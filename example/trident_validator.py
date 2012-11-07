def update_dag_job_state(result,is_valid):
	import dag.boinc
	from dag import States,strstate

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
	root_dag.save()
	

def validate(result1, result2):
	import dag.util as dagu
	import re
	from os import path as OP
	import os

	output_file1 = result1.output_files[0][0]
	output_file2 = result2.output_files[0][0]

	# Check if the files are missing. If they are missing
	# and the computation did actually run, there simply were
	# no hits found.
	# Short DNA segments may be very quick (especially in linux)
	# These short runs may produce no hits, which will mean there
	# is no output file (zero bytes).
	if result1.cpu_time < 60 and result2.cpu_time < 60:
		if not OP.isfile(output_file1) and not OP.isfile(output_file2):
			valid = True
			update_dag_job_state(result1,valid)
			return valid
	
	if not OP.isfile(output_file1) and result1.cpu_time > 60:
		valid = False
		update_dag_job_state(result1,valid)
		return valid
	if not OP.isfile(output_file2) and result2.cpu_time > 60:
		valid = False
		update_dag_job_state(result1,valid)
		return valid

	valid = (OP.getsize(result1.output_files[0][0]) == OP.getsize(result2.output_files[0][0]))

	if not valid:
		print("File sizes differ")
		for i in [result1.output_files[0][0], result2.output_files[0][0]]:
			print("Size of %s: %d" % (i,OP.getsize(i)))

	update_dag_job_state(result1,valid)
	return valid

def clean(result):
	import re
	import dag.util as dag_utils
	import boinctools
	import dag,dag.boinc
	import shutil
	import stat
	from os import path as OP
	import os

	print("Cleaning %s" % result.name)
	
	wuname = re.findall(r"^(.*)_\d*$",result.name)
	if len(wuname) == 0:
		print("Malformed result name")
		return None
	wuname = wuname[0]
	try:
		the_dag = dag.boinc.result_to_dag(result.name)
	except dag_utils.NoDagMarkerException as ndme:
		print("Missing dag %s\nSkipping clean up")
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
	
	return None
