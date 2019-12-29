/*
 * Header file for thermostat schedule module
 */

#ifndef SCHEDULE_H_
#define SCHEDULE_H_

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifndef SCHED_MAX_EVENTS
#define SCHED_MAX_EVENTS 100
#endif

struct event_str {
	long sow;                   /* second of week */
	double setpoint_degc;       /* setpoint */
};

struct schedule_str {
	size_t num_events;
	struct event_str event[SCHED_MAX_EVENTS];
	bool hold_flag;
	double hold_temp_degc;
	bool advance_flag;
};

/*
 * public function prototypes
 */

double
sched_get_setpoint(time_t sse);

#endif
