/*
 * thermostat module
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <gpiod.h>
#include <syslog.h>
#include <errno.h>

#include "thermostat.h"
#include "util.h"

struct state_str {
	long sequence;
	struct timespec timestamp;
};

/*
 * private functions
 */

static int
log_data(const struct state_str *state, const char *data_dir)
{
	FILE *out;
	struct tm bdt;		/* broken down time */
	char date_buf[20];

	if (localtime_r(&state->timestamp.tv_sec, &bdt) == NULL) {
		syslog(LOG_ERR, "localtime_r: %s", strerror(errno));
		return -1;
	}

	out = open_dayfile(data_dir, &bdt);
	if (out == NULL) {
		syslog(LOG_ERR, "open_dayfile: %s", strerror(errno));
		return -1;
	}

	if (strftime(date_buf, sizeof date_buf, "%w %Y%m%d%H%M%S", &bdt) == 0) {
		syslog(LOG_ERR, "strftime: buffer overflow");
		return -1;
	}

	fprintf(out, "%7ld %10ld %9ld %s\n",
		state->sequence,
		state->timestamp.tv_sec,
		state->timestamp.tv_nsec,
		date_buf);

	if (out != stdout) {
		if (fclose(out) == EOF) {
			syslog(LOG_ERR, "fclose: %s", strerror(errno));
			return -1;
		}
	}

	return 0;
}

/*
 * public functions
 */

int
tstat_control(struct gpiod_line *line, int mcp9808_fd, const char *data_dir)
{
	struct state_str state = {
		.sequence = 0,
	};

	if (wait_for_next_second() == -1) {
		syslog(LOG_ERR, "wait: %s", strerror(errno));
		return -1;
	}
	for (;;) {
		if (clock_gettime(CLOCK_REALTIME, &state.timestamp) == -1) {
			syslog(LOG_ERR, "clock_gettime: %s", strerror(errno));
			return -1;
		}

		log_data(&state, data_dir);

		state.sequence++;
		if (wait_for_next_second() == -1) {
			syslog(LOG_ERR, "wait: %s", strerror(errno));
			return -1;
		}
	}

	return 0;
}
