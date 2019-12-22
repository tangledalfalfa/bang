/*

bang: DIY thermostat, designed to run on a Raspberry Pi, or any Linux
      system with GPIO and an I2C bus.
Copyright (C) 2019 Todd Allen

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of  MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <getopt.h>
#include <errno.h>

/*
 * private functions
 */

/* help message */
static void
print_help(void)
{
	printf("Usage: %s [OPTIONS]\n",
		program_invocation_short_name);
	printf("DIY thermostat, designed for Raspberry Pi or similar system\n");
	printf("\n");
	printf("Options:\n");
	printf("  -h, --help:\t\tdisplay this message and exit\n");
	printf("  -v, --version:\tdisplay the version and exit\n");
}

/* version */
static void
print_version(void)
{
	printf("%s v%s\n",
	       program_invocation_short_name, ""); /* FIXME */
	printf("Copyright (C) 2019 Todd Allen\n");
	printf("License: GNU GPLv3\n");
	printf("This is free software: you are free to change and"
	       " redistribute it.\n");
	printf("There is NO WARRANTY, to the extent permitted by law.\n");
}

static int
get_options(int argc, char *argv[])
{
	static const struct option longopts[] = {
		{
			.name = "help",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'h',
		},
		{
			.name = "version",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'v',
		},
		{ NULL, 0, NULL, 0 }  /* terminate */
	};
	static const char *const shortopts = ":hv";
	int optc, opti;

	for (;;) {
		optc = getopt_long(argc, argv, shortopts, longopts, &opti);
		if (optc < 0)
			break;

		switch (optc) {
		case 'h':
			print_help();
			exit(EXIT_SUCCESS);

		case 'v':
			print_version();
			exit(EXIT_SUCCESS);

		case '?':
			syslog(LOG_ERR, "unrecognized option: %c",
			       optopt);
			return -1;

		case ':':
			syslog(LOG_ERR, "missing argument: %c",
			       optopt);
			return -1;

		default:
			syslog(LOG_ERR, "unexpected return from getopt: %d",
			       optc);
			return -1;
		}
	}

	return 0;
}

/* M A I N */
int
main(int argc, char *argv[])
{
	int ret;

	openlog(program_invocation_short_name, LOG_ODELAY, LOG_USER);
	syslog(LOG_INFO, "started");

	ret = get_options(argc, argv);
	if (ret == -1)
		exit(EXIT_FAILURE);

	syslog(LOG_INFO, "exiting");
	closelog();
	exit(EXIT_SUCCESS);
}