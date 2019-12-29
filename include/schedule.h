/*
 * Header file for thermostat schedule module
 */

#ifndef SCHEDULE_H_
#define SCHEDULE_H_

#include <stdbool.h>
#include <time.h>

#ifndef SCHED_MAX_EVENTS
#define SCHED_MAX_EVENTS 100
#endif

struct event_str {
	long sow;                   /* second of week */
	float setpoint_degc;        /* setpoint */
};

struct schedule_str {
	int num_events;
	struct event_str event[SCHED_MAX_EVENTS];
	bool hold_flag;
	float hold_temp_degc;
	bool advance_flag;
};

/*
 * public function prototypes
 */

float
sched_get_setpoint(time_t sse);

#endif
