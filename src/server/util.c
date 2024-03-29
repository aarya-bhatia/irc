#include "include/server.h"

/**
 * Reads motd from given file
 *
 * Selects the (day of year % total lines) line.
 */
char *get_motd(char *fname)
{
	FILE *file = fopen(fname, "r");

	if (!file)
	{
		log_warn("failed to open %s", fname);
		return NULL;
	}

	char *res = NULL;
	size_t res_len = 0;
	size_t num_lines = 1;

	// count number of lines
	for (int c = fgetc(file); c != EOF; c = fgetc(file))
	{
		if (c == '\n')
		{
			num_lines = num_lines + 1;
		}
	}

	fseek(file, 0, SEEK_SET); // go to beginning

	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	size_t line_no = tm.tm_yday % num_lines; // select a line
	// from file to
	// use

	for (size_t i = 0; i < line_no + 1; i++)
	{
		if (getline(&res, &res_len, file) == -1)
		{
			perror("getline");
			break;
		}
	}

	if (res)
	{
		size_t len = strlen(res);
		if (res[len - 1] == '\n')
		{
			res[len - 1] = 0;
		}
	}

	fclose(file);

	return res;
}
