#ifndef VALIDATE_MOLEDYN_STRUCTS_H
#define VALIDATE_MOLEDYN_STRUCTS_H

struct validate_moledyn_arguments
{
	std::string stats_file,tolerance;//tolerance is the file containing the tolerance data.
	std::vector<std::string> mdout_files;
	int verbose_level;
};

#endif //VALIDATE_MOLEDYN_STRUCTS_H

