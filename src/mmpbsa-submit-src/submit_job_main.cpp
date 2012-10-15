/**
 * This program parses a job file, which is a list of commands that should be run on the grid (albeit
 * segmented). Based on the job file, segmented version of the commands are place in the queue, to be
 * run on the grid.
 *
 * In general, this program would not be run by typical users. Instead, gsub should be used, because
 * submit_job only places the segmented jobs into the queue system, without actually starting them.
 * However, this program could be used if the user (probably an admin) wants to stage the job without
 * starting it, i.e. for debugging purposes. Also, via command line arguments, one can simulate the
 * submission of a job (cf submit_job --help)
 *
 * Created by: David Coss, 2010
 */

#include "submit_job.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <cstdio>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <getopt.h>


static void print_help()
{
  printf("submit_job -- Parses a list of commands and produces a parallel work load to be submitted to the grid.\n\n");
  printf("Usage: submit_job [options] <FILE>\n\n");
  printf("Options:\n");
  printf("--verbose,\t-v\tProduces verbose output\n");
  printf("--debug,\t-d INT\tVaries the level of verbosity. The higher the number, the more verbose.\n\t\t\tAlternatively, zero produces little or no output.\n");
  printf("--graph,\t-g\tProduces a graphviz .dot file of the parallel batch of commands.\n");
  printf("--dotfile FILE\t\tSpecify the file name of the graphviz .dot file.\n");
  printf("--version\t\tPrints the version and exits.\n");
  printf("--simulate,\t-s\tOptional flag to simulate the submission of the job, without\n\t\t\tactually sending it to the queue system.\n");
  fflush(stdout);
}

enum {SUBMIT_JOB_VFLAG=2,SUBMIT_JOB_DOTFILENAME};
static struct option long_opts[] = {
  {"help",0,NULL,'h'},
  {"usage",0,NULL,'h'},
  {"verbose",0,NULL,'v'},
  {"debug",1,NULL,'d'},
  {"graph",0,NULL,'g'},
  {"dotfile",1,NULL,SUBMIT_JOB_DOTFILENAME},
  {"version",0,NULL,SUBMIT_JOB_VFLAG},
  {"simulate",0,NULL,'s'},
  {NULL,0,NULL,0}
};
static const char short_opts[] = "d:ghsv";

void set_verbosity(struct submit_job_arguments * args, char* arg)
{
	if(arg == 0 || strlen(arg) == 0)
	{
		args->verbose_level = 1;
		return;
	}

	if(sscanf(arg,"%d",&args->verbose_level) != 1)
	{
		args->verbose_level = 1;
		fprintf(stderr,"Could not read verbose arg: %s\nUsing default value %d\n",arg,args->verbose_level);
	}
	if(args->verbose_level)
	  printf("Using verbosity level %d\n",args->verbose_level);
	return;
}

static void parse_opt(char key, char* arg, struct submit_job_arguments * args)
{
  switch(key)
    {
    case SUBMIT_JOB_DOTFILENAME:
      if(arg == NULL)
	break;
      args->graph_filename = arg;
      break;
    case 'v':
      args->verbose_level = 1;
      break;
    case 'd':
      if(arg == NULL)
	args->verbose_level = 1;
      else
	set_verbosity(args,arg);
      break;
    case 'g':
      args->generate_graph = true;
      break;
    case 's':
      args->simulate_submission = true;
      break;
    case SUBMIT_JOB_VFLAG:
      std::cout << PACKAGE_VERSION << std::endl;
      exit(0);
      break;
    case 'h':
      print_help();
      exit(0);
      break;
    default:
      fprintf(stderr,"Unknown flag: %c\nFor help, run ./submit_job --help\n",key);
      exit(-1);
      break;
    }
}

int main(int argc, char** argv) {

	struct submit_job_arguments args;
	std::map<std::string,grid_file_info> file_list;
	char optflag = -1;

	default_submit_job_args(&args);
	while((optflag = getopt_long(argc,argv,short_opts,long_opts,NULL)) != -1)
	  {
	    parse_opt(optflag,optarg,&args);
	  }
	
	if(optind >= argc)
	  {
	    fprintf(stderr,"A file is required from which commands will be read.\n");
	    exit(-1);
	  }
	
	args.job_file = argv[optind];

	if(args.generate_graph && args.graph_filename.size() == 0)
	  args.graph_filename = args.job_file + ".dot";
	
	return submit_job_main(args,file_list);

}
