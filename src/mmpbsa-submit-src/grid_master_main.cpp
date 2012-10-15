#include "grid_master.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

void args_usage();

static char grid_master_doc[] = "grid_master -- Program for managing jobs on the grid.";
static char grid_master_usage[] = "Usage: grid_master [options] COMMAND PARAMETER";

/**
 * Options provide to the command line
 */
struct grid_options{
	struct option getoptions;/* getopt.h option type. */
	std::string help_string,arg_type;
};

/**
 * Possible options provided to the command line.
 */
static struct grid_options options[] = {
		{{"simulate",no_argument,0,'s'},"Simulates the database queries, without actually sending the job to the grid.",""},
		{{"verbose",optional_argument,0,'v'},"Sets the verbosity of the program. Higher the level, the more verbose.","INTEGER"},
		{{"help",no_argument,0,'h'},"This help dialog",""},
		{{"usage",no_argument,0,0},"Same as --help",""},
		{{0,0,0,0},"",""}
};

void parse_opt(const int& key, struct grid_master_arguments& args, char** argv)
{
	std::istringstream buff;
	switch(key)
	{
	case 's':
		args.simulate_spawn = true;
		break;
	case 'v':
	  if(optarg == 0)
	    {
	      args.grid_master_verbosity = 1;
	      break;
	    }
		buff.str(optarg);
		buff >> args.grid_master_verbosity;
		if(buff.fail())
		{
		  std::cerr << "Warning: " << argv[optind] << " is an invalid verbose level. Need integer, but got: " << optarg << std::endl;
			exit(INVALID_INPUT_PARAMETER);
		}
		break;
	case 0:case '?':case 'h':
	  args_usage();
	  break;
	default:
		std::cerr << "Unknown flag: " << (char)key << std::endl;
		std::cerr << "run grid_master --help for more information" << std::endl;
		exit(1);
	}
}

void grid_master_defaults(struct grid_master_arguments& args)
{
	args.simulate_spawn = false;
	args.grid_master_verbosity = 0;
	args.processID = -1;
	args.initialization = true;
}

void args_usage()
{
	using namespace std;
	struct grid_options curr_opts;
	size_t opt_counter;

	cout << grid_master_doc << endl;
	cout << grid_master_usage << endl;
	cout << endl;
	cout << "Commands: " << endl;
	cout << "close -- cancels the job." << endl;
	cout << "process  -- determines whether or not the job should run or"
			" continue to wait and acts accordingly." << endl;
	cout << "recombine -- recombines files that belong specified recombination process ID." << endl;
	cout << "start -- starts a job if it is waiting. If it depends on other process, those are started first, if they have not yet finished." << endl;
	cout << "status  -- prints the status of the process." << endl;
	cout << "list  -- prints the jobs in the queue system owned by the user. If a number is specified, all process in that job batch are listed." << endl;
	cout << endl;
	cout << "Options:" << endl;
	opt_counter = 0;
	curr_opts = options[opt_counter];
	while(curr_opts.getoptions.name != 0)
	{
		if(curr_opts.getoptions.val > 0x41)
			cout << "-" << (char)curr_opts.getoptions.val << ", ";
		else
			cout << "    ";
		cout << "--" << curr_opts.getoptions.name;
		if(curr_opts.arg_type.size() != 0)
		{
			cout << "=";
			if(curr_opts.getoptions.has_arg == optional_argument)
				cout << "[";
			cout << curr_opts.arg_type;
			if(curr_opts.getoptions.has_arg == optional_argument)
				cout << "]";
		}
		cout << "\t" << curr_opts.help_string << endl;
		curr_opts = options[++opt_counter];
	}
	exit(0);
}

int main(int argc, char** argv)
{
	struct grid_master_arguments args;

	if(argc < 2)
		args_usage();

	grid_master_defaults(args);

	int getopt_retval,option_index;
	struct option * long_opts;
	struct grid_options curr_grid_opt;
	size_t num_opts = 0;

	curr_grid_opt = options[num_opts];
	while(curr_grid_opt.getoptions.name != 0)
		curr_grid_opt = options[++num_opts];
	num_opts++;//for the null opt at the end.
	long_opts = new struct option[num_opts];
	for(size_t i = 0;i<num_opts;i++)
		long_opts[i] = options[i].getoptions;

	while(true)
	{
		getopt_retval = getopt_long(argc,argv,"sv::",long_opts,&option_index);
		if(getopt_retval == -1)
			break;

		parse_opt(getopt_retval,args,argv);
	}

	if(optind > argc - 1)
		args_usage();

	args.command = argv[optind++];
	if((args.command != "list") && (args.command != "running") && (args.command != "next")  && (args.command != "finished") && optind >= argc)
	  {
	    std::cout << "Invalid parameter or number of arguments." << std::endl << "run grid_master --help for assistance." << std::endl;
	    exit(INVALID_CLI_ARGUMENT);
	  }
	else if((args.command == "list" || args.command == "next"|| args.command == "finished" || args.command == "running") && optind == argc)
		args.parameter = "";
	else
		args.parameter = argv[optind++];

	delete [] long_opts;
	return grid_master_worker(args);
}
