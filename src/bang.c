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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <syslog.h>
#include <getopt.h>
#include <errno.h>

#define PGM_NAME program_invocation_short_name

/* option defaults */
#define DFLT_GPIO_DEVICE      "gpiochip0"
#define DFLT_GPIO_OFFSET      26
#define DFLT_GPIO_ACTIVE_LOW  true
#define DFLT_I2C_DEVICE       "i2c-1"
#define DFLT_MCP9808_I2C_ADDR 0x18

struct options_str {
	const char *gpio_device;
	unsigned gpio_offset;
	bool gpio_active_low;
	const char *i2c_device;
	uint8_t mcp9808_i2c_addr;
	const char *data_dir;
	bool force;
	bool test;
};

/*
 * private functions
 */

/* help message */
static void
print_help(void)
{
	printf("Usage: %s [OPTION]...\n",
		PGM_NAME);
	printf("DIY thermostat, designed for Raspberry Pi or similar system\n");
	printf("\n");
	printf("Options:\n");
	printf("  -h, --help:\t\tdisplay this message and exit\n");
	printf("  -v, --version:\tdisplay the version and exit\n");
	printf("  -g, --gpio-dvc=DVC:\tgpio device name (default: %s)\n",
		DFLT_GPIO_DEVICE);
	printf("  -n, --gpio-num=OFST:\tgpio offset number (default: %u)\n",
		DFLT_GPIO_OFFSET);
	printf("  -p, --gpio-pol=POL:\tgpio active polarity, 0 or 1"
	       " (default: %d)\n",
		!DFLT_GPIO_ACTIVE_LOW);
	printf("  -i, --i2c-dvc=DVC:\tI2C bus device name (default: %s)\n",
		DFLT_I2C_DEVICE);
	printf("  -a, --i2c-addr=ADDR:\tslave address of MCP9808"
	       " (default: 0x%02x)\n",
		DFLT_MCP9808_I2C_ADDR);
	printf("  -d, --data-dir=DIR:\tdata directory (default: %s)\n",
		"stdout");
	printf("  -f, --force:\t\toverride option warnings\n");
	printf("  -T, --test:\t\tperform hardware test\n");
}

/* version message */
static void
print_version(void)
{
	printf("%s v%s\n",
	       PGM_NAME, VERSION);
	printf("Copyright (C) 2019 Todd Allen\n");
	printf("License: GNU GPLv3\n");
	printf("This is free software: you are free to change and"
	       " redistribute it.\n");
	printf("There is NO WARRANTY, to the extent permitted by law.\n");
}

static int
get_options(int argc, char *argv[], struct options_str *options)
{
	static const struct option longopts[] = {
		{       .name = "help",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'h',
		},
		{       .name = "version",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'v',
		},
		{       .name = "gpio-dvc",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'g',
		},
		{       .name = "gpio-num",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'n',
		},
		{       .name = "gpio-pol",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'p',
		},
		{       .name = "i2c-dvc",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'i',
		},
		{       .name = "i2c-addr",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'a',
		},
		{       .name = "data-dir",
			.has_arg = required_argument,
			.flag = NULL,
			.val = 'd',
		},
		{       .name = "force",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'f',
		},
		{       .name = "test",
			.has_arg = no_argument,
			.flag = NULL,
			.val = 'T',
		},
		{ NULL, 0, NULL, 0 }  /* terminate */
	};
	static const char *const shortopts = ":hvg:n:p:i:a:d:fT";
	int optc, opti;
	const char *n_arg = NULL;
	const char *p_arg = NULL;
	const char *a_arg = NULL;
	long long val;
	char *endptr;

	options->gpio_device = DFLT_GPIO_DEVICE;
	options->gpio_offset = DFLT_GPIO_OFFSET;
	options->gpio_active_low = DFLT_GPIO_ACTIVE_LOW;
	options->i2c_device = DFLT_I2C_DEVICE;
	options->mcp9808_i2c_addr = DFLT_MCP9808_I2C_ADDR;
	options->data_dir = NULL; /* stdout */
	options->force = false;
	options->test = false;

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

		case 'g':
			options->gpio_device = optarg;
			break;

		case 'n':
			n_arg = optarg;
			break;

		case 'p':
			p_arg = optarg;
			break;

		case 'i':
			options->i2c_device = optarg;
			break;

		case 'a':
			a_arg = optarg;
			break;

		case 'd':
			options->data_dir = optarg;
			break;

		case 'f':
			options->force = true;
			break;

		case 'T':
			options->test = true;
			break;

		case '?':
			fprintf(stderr, "%s: unrecognized option: %c\n",
				PGM_NAME, optopt);
			print_help();
			return -1;

		case ':':
			fprintf(stderr, "%s: missing argument: -%c\n",
				PGM_NAME, optopt);
			print_help();
			return -1;

		default:
			fprintf(stderr,
				"%s: unexpected return from getopt: %d\n",
				PGM_NAME, optc);
			return -1;
		}
	}

	if (n_arg != NULL) {
		val = strtoll(n_arg, &endptr, 0);
		if ((val < 0) || (val > 0xFFFF) || (*endptr != '\0')) {
			fprintf(stderr, "%s: gpio number %lld invalid\n",
				PGM_NAME, val);
			return -1;
		}
		options->gpio_offset = (unsigned)val;
	}

	if (p_arg != NULL) {
		val = strtoll(p_arg, &endptr, 0);
		if ((val < 0) || (val > 1) || (*endptr != '\0')) {
			fprintf(stderr, "%s: gpio polarity %lld invalid\n",
				PGM_NAME, val);
			return -1;
		}
		options->gpio_active_low = !val;
	}

	if (a_arg != NULL) {
		val = strtoll(a_arg, &endptr, 0);
		if ((val < 0) || (val > 0xFF) || (*endptr != '\0')) {
			fprintf(stderr,
				"%s: MCP9808 I2C address %lld invalid\n",
				PGM_NAME, val);
			return -1;
		}
		options->mcp9808_i2c_addr = val;
	}

	return 0;
}

/* M A I N */
int
main(int argc, char *argv[])
{
	int ret;
	struct options_str options;

	openlog(program_invocation_short_name, LOG_ODELAY, LOG_USER);
	syslog(LOG_INFO, "started");

	ret = get_options(argc, argv, &options);
	if (ret == -1)
		exit(EXIT_FAILURE);

	syslog(LOG_INFO, "options:");
	syslog(LOG_INFO, "    gpio-dvc: %s", options.gpio_device);
	syslog(LOG_INFO, "    gpio-num: %u", options.gpio_offset);
	syslog(LOG_INFO, "    gpio-pol: %d", !options.gpio_active_low);
	syslog(LOG_INFO, "    i2c-dvc: %s", options.i2c_device);
	syslog(LOG_INFO, "    i2c-addr: 0x%02X", options.mcp9808_i2c_addr);
	syslog(LOG_INFO, "    data-dir: %s",
	       (options.data_dir == NULL) ? "stdout" : options.data_dir);
	syslog(LOG_INFO, "    force: %s", options.force ? "true" : "false");
	syslog(LOG_INFO, "    test: %s", options.test ? "true" : "false");

	syslog(LOG_INFO, "exiting");
	closelog();
	exit(EXIT_SUCCESS);
}
