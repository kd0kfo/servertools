/**
 * gsub
 *
 * Job creation program. This is the program that users should have access to. It will create
 * a series of workunits for a submitted job file. Then the work units that have no dependencies
 * on other work units will be started.
 *
 * This program simply acts as a wrapper for the programs "submit_job" and "grid_master". First,
 * the job file is sent to submit_job, which places the segmented version of the job in the queue
 * system. Then gsub calls grid_master with the argument "process" and the first process id. That
 * submit_job used. This file, via submit_job, reads a file for a list of commands (i.e. sander command
 * line arguments). Therefore, one can make a bash-like script of sander & mmpbsa commands and either
 * call "gsub <filename>" OR place "#!/grid_tools/path/bin/gsub" in the first line of the job file.
 *
 * Created by: David Coss, 2010
 */

#include <iostream>
#include <cstring>
#include <sstream>

#include <argp.h>
#include "db_config.h"
#include "submit_job_structs.h"
#include "grid_master.h"
#include "submit_job.h"

static char doc[] = "gsub -- Submits and starts jobs on the grid.";
static char args_doc[] = "<Job File>";
static struct argp_option options[] = {
		{"simulate",'s',0,0,"Simulates grid process submission without sending it to the grid."},
  {"verbose",'v',"LEVEL",OPTION_ARG_OPTIONAL,"Produce verbose output. 0 = not verbose, 1 = verbose."},
  {0}
};

void set_verbosity(struct submit_job_arguments * args, char* arg)
{
	if(arg == 0 || strlen(arg) == 0)
	{
		args->verbose_level = 1;
		return;
	}
	std::istringstream buff(arg);
	buff >> args->verbose_level;
	if(buff.fail())
		args->verbose_level = 1;
	if(args->verbose_level)
		std::cout << "Using verbosity level " << args->verbose_level << std::endl;
	return;
}

static error_t parse_opt(int key, char* arg, struct argp_state* state)
{
	struct submit_job_arguments * args = (struct submit_job_arguments*) state->input;

	switch(key)
	{
	case 'v':
		set_verbosity(args,arg);
		break;
	case 's':
		args->simulate_submission = true;
		break;
	case ARGP_KEY_ARG:
		if(state->arg_num > 1)
			argp_usage(state);
		args->job_file = arg;
		break;
	case ARGP_KEY_NO_ARGS:
		argp_usage(state);
	case ARGP_KEY_END:
		if (state->arg_num != 1)
			argp_usage (state);//too few args
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = {options,parse_opt, args_doc, doc};


#include <mysql/mysql.h>


int main(int argc, char** argv)
{
  int retval;

  struct submit_job_arguments sj_args;
  struct grid_master_arguments gm_args;

  // Setup parameters
  default_submit_job_args(&sj_args);
  
  // Parse command line arguments
  argp_parse(&argp,argc,argv,0,0,&sj_args);


  // Setup more parameters
  gm_args.command = "start";
  gm_args.grid_master_verbosity = sj_args.verbose_level;
  gm_args.simulate_spawn = sj_args.simulate_submission;
  gm_args.processID = -1;
  gm_args.initialization = true;

  // Perform submission procedure
  retval = submit_and_start(sj_args,gm_args);

  // Rest
  return retval;
}

