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
#include "mcp9808.h"

#define N_AVG 60

struct state_str {
	unsigned long sequence;
	struct timespec timestamp;
	float temp_degc;
	float temp_arr[N_AVG];
	float temp_sum;
};

/*
 * private functions
 */

static int
get_temperature(struct state_str *state, int mcp9808_fd)
{
	int idx;

	/* measure temperature */
	if (mcp9808_read_temp(mcp9808_fd,
			      NULL, &state->temp_degc) == -1) {
		syslog(LOG_ERR, "mcp9808 read temp: %s",
		       strerror(errno));
		return -1;
	}

	/* averaging */
	idx = state->sequence % ARRAY_SIZE(state->temp_arr);
	state->temp_sum -= state->temp_arr[idx];
	state->temp_arr[idx] = state->temp_degc;
	state->temp_sum += state->temp_degc;

	return 0;
}

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

	fprintf(out, "%7lu %10ld %9ld %s %7.4f %7.4f\n",
		state->sequence,
		state->timestamp.tv_sec,
		state->timestamp.tv_nsec,
		date_buf,
		state->temp_degc,
		state->temp_sum / ARRAY_SIZE(state->temp_arr));

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
		.temp_sum = 0.0f,
	};

	memset(&state.temp_arr, 0, sizeof state.temp_arr);

	for (;;) {
		if (wait_for_next_second() == -1) {
			syslog(LOG_ERR, "wait: %s", strerror(errno));
			return -1;
		}

		state.sequence++;

		if (clock_gettime(CLOCK_REALTIME, &state.timestamp) == -1) {
			syslog(LOG_ERR, "clock_gettime: %s", strerror(errno));
			return -1;
		}

		if (get_temperature(&state, mcp9808_fd) == -1)
			return -1;

		log_data(&state, data_dir);

		/* wait for temperature average to settle */
		if (state.sequence < ARRAY_SIZE(state.temp_arr))
			continue;
	}

	return 0;
}
