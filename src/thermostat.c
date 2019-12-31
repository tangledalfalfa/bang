/*
 * thermostat module
 */
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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gpiod.h>
#include <syslog.h>
#include <errno.h>

#include "thermostat.h"
#include "util.h"
#include "mcp9808.h"
#include "schedule.h"
#include "cfgfile.h"
#include "controls.h"

#define N_AVG 60

#ifndef HYST_DEGC
#define HYST_DEGC 0.5
#endif

struct state_str {
	unsigned long sequence;
	struct timespec timestamp;
	double temp_degc;
	double temp_arr[N_AVG];
	double temp_sum;
	double temp_avg;
	bool heat_req;
	double setpoint_degc;
};

/*
 * private functions
 */

static int
sync_to_second(struct state_str *state)
{
	if (wait_for_next_second() == -1) {
		syslog(LOG_ERR, "wait: %s", strerror(errno));
		return -1;
	}

	if (clock_gettime(CLOCK_REALTIME, &state->timestamp) == -1) {
		syslog(LOG_ERR, "clock_gettime: %s", strerror(errno));
		return -1;
	}

	state->sequence++;

	return 0;
}

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
update_schedule(struct schedule_str *schedule)
{
	struct stat statbuf;
	struct schedule_str temp_sched;
	size_t i;

	if (stat(schedule->config_fname, &statbuf) == -1) {
		syslog(LOG_ERR,
		       "stat(%s): %s",
		       schedule->config_fname, strerror(errno));
		return -1;
	}

	if (statbuf.st_mtim.tv_sec <= schedule->config_mtime)
		return 0;

	syslog(LOG_INFO, "updating schedule");

	/* failure leaves schedule unchanged */
	if (cfg_load(schedule->config_fname, &temp_sched) == -1)
		return -1;

	/* copy in new events */
	schedule->num_events = temp_sched.num_events;
	for (i = 0; i < schedule->num_events; i++)
		schedule->event[i] = temp_sched.event[i];
	schedule->config_mtime = temp_sched.config_mtime;

	schedule->units = temp_sched.units;

	return 0;
}

static void
update_sys(struct state_str *state, struct schedule_str *schedule)
{
	/* check controls (hold, advance, resume) */
	if (ctrls_check(schedule) == -1)
		syslog(LOG_ERR, "controls check failed!");

	if (schedule->hold_flag) {
		state->setpoint_degc = schedule->hold_temp_degc;
	} else {
		/*
		 * check for config file update
		 * soldier on if it fails
		 */
		if (update_schedule(schedule) == -1)
			syslog(LOG_ERR, "schedule update failed!");
		state->setpoint_degc = sched_get_setpoint(
			state->timestamp.tv_sec, schedule);
	}
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
control_temp(struct state_str *state, struct gpiod_line *line)
{
	/* wait for temperature average to settle */
	if (state->sequence < ARRAY_SIZE(state->temp_arr))
		;
	else if ((!state->heat_req)
		 && (state->temp_avg < state->setpoint_degc - HYST_DEGC)) {
		if (set_heat_request(state, line, true) == -1)
			return -1;
	} else if ((state->heat_req)
		   && (state->temp_avg > state->setpoint_degc)) {
		if (set_heat_request(state, line, false) == -1)
			return -1;
	}

	return 0;
}

static int
log_data(const struct state_str *state, const struct schedule_str *schedule,
	 const char *data_dir)
{
	FILE *out;
	struct tm bdt;		/* broken down time */
	char date_buf[20];
	double temp, temp_avg, setpoint;

	if (localtime_r(&state->timestamp.tv_sec, &bdt) == NULL) {
		syslog(LOG_ERR, "localtime_r: %s", strerror(errno));
		return -1;
	}

	//printf("%ld\n", bdt.tm_gmtoff);

	out = open_dayfile(data_dir, &bdt);
	if (out == NULL) {
		syslog(LOG_ERR, "open_dayfile: %s", strerror(errno));
		return -1;
	}

	if (strftime(date_buf, sizeof date_buf, "%w %Y%m%d%H%M%S", &bdt) == 0) {
		syslog(LOG_ERR, "strftime: buffer overflow");
		return -1;
	}

	if (schedule->units == UNITS_DEGF) {
		temp = degc_to_degf(state->temp_degc);
		temp_avg = degc_to_degf(state->temp_avg);
		setpoint = degc_to_degf(state->setpoint_degc);
	} else {
		temp = state->temp_degc;
		temp_avg = state->temp_avg;
		setpoint = state->setpoint_degc;
	}

	fprintf(out, "%7lu %10ld %9ld %s %7.4f %7.4f %4.1f %d %d %d\n",
		state->sequence,
		state->timestamp.tv_sec,
		state->timestamp.tv_nsec,
		date_buf,
		temp,
		temp_avg,
		setpoint,
		state->heat_req,
		schedule->hold_flag,
		schedule->advance_flag);

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
tstat_control(struct gpiod_line *line, int mcp9808_fd,
	      struct schedule_str *schedule,
	      const char *data_dir, int data_interval)
{
	struct state_str state = {
		.sequence = 0,
		.temp_sum = 0.0,
		.setpoint_degc = 0.0,
	};

	memset(&state.temp_arr, 0, sizeof state.temp_arr);

	/* start with heat off */
	if (set_heat_request(&state, line, false) == -1)
		return -1;

	/* initialize setpoint */
	state.setpoint_degc = sched_get_setpoint(time(NULL), schedule);

	for (;;) {
		/* 1 Hertz control loop */
		if (sync_to_second(&state) == -1)
			return -1;

		/* get new measurement, maintain 60-second average */
		if (get_temperature(&state, mcp9808_fd) == -1)
			return -1;

		/* perform system updates */
		update_sys(&state, schedule);

		/* bang-bang controller */
		if (control_temp(&state, line) == -1)
			return -1;

		/* log data (if requested) */
		if ((data_interval != 0)
		    && (state.timestamp.tv_sec % data_interval == 0))
			log_data(&state, schedule, data_dir);
	}

	return 0;
}
