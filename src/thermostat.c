/*
 * thermostat module
 */

#include "config.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <gpiod.h>
#include <syslog.h>
#include <errno.h>

#include "thermostat.h"
#include "util.h"
#include "mcp9808.h"
#include "schedule.h"

#define N_AVG 60

#ifndef HYST_DEGC
#define HYST_DEGC 0.5f
#endif

struct state_str {
	unsigned long sequence;
	struct timespec timestamp;
	float temp_degc;
	float temp_arr[N_AVG];
	float temp_sum;
	float temp_avg;
	bool heat_req;
	float setpoint_degc;
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

	state->temp_avg = state->temp_sum / ARRAY_SIZE(state->temp_arr);


	return 0;
}

static int
set_heat_request(struct state_str *state, struct gpiod_line *line,
		       bool req)
{
	if (gpiod_line_set_value(line, req) != 0) {
		syslog(LOG_ERR, "gpiod line set value: %s",
		       strerror(errno));
		return -1;
	}

	state->heat_req = req;

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

	fprintf(out, "%7lu %10ld %9ld %s %7.4f %7.4f %4.1f %d\n",
		state->sequence,
		state->timestamp.tv_sec,
		state->timestamp.tv_nsec,
		date_buf,
		state->temp_degc,
		state->temp_avg,
		state->setpoint_degc,
		state->heat_req);

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
		.setpoint_degc = 0.0f,
	};

	memset(&state.temp_arr, 0, sizeof state.temp_arr);
	if (set_heat_request(&state, line, false) == -1)
		return -1;

	for (;;) {
		if (wait_for_next_second() == -1) {
			syslog(LOG_ERR, "wait: %s", strerror(errno));
			return -1;
		}

		if (clock_gettime(CLOCK_REALTIME, &state.timestamp) == -1) {
			syslog(LOG_ERR, "clock_gettime: %s", strerror(errno));
			return -1;
		}

		state.sequence++;

		if (get_temperature(&state, mcp9808_fd) == -1)
			return -1;

		state.setpoint_degc
			= sched_get_setpoint(state.timestamp.tv_sec);

		/* wait for temperature average to settle */
		if (state.sequence < ARRAY_SIZE(state.temp_arr))
			;
		else if ((!state.heat_req)
		    && (state.temp_avg < state.setpoint_degc - HYST_DEGC)) {
			if (set_heat_request(&state, line, true) == -1)
				return -1;
		} else if ((state.heat_req)
			   && (state.temp_avg > state.setpoint_degc)) {
			if (set_heat_request(&state, line, false) == -1)
				return -1;
		}

		log_data(&state, data_dir);
	}

	return 0;
}
