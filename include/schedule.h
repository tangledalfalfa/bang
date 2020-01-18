/*
 * Header file for thermostat schedule module
 */

#ifndef SCHEDULE_H_
#define SCHEDULE_H_

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#include "cfgfile.h"

struct schedule_str {
	struct cfg_data_str config;

	bool hold_flag;
	double hold_temp_degc;
	bool advance_flag;
	/* control files: hold, advance, and resume */
	const char *ctrl_dir;
	time_t hold_mtime;
	time_t advance_mtime;
	time_t resume_mtime;

	ssize_t curr_idx;	/* index of current scheduled event */
	long curr_sow;		/* time of last setpoint calculation */
	                        /* (valid when curr_idx != -1) */
};

/*
 * public function prototypes
 */

double
sched_get_setpoint(time_t sse, struct schedule_str *schedule);

#endif
