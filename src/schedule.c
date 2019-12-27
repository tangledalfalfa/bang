/*
 * thermostat schedule module
 */

#include <time.h>

#include "schedule.h"

struct event_str {
	long sow;		/* second of week */
	float setpoint_degc;
};

/*
 * public functions
 */

float
sched_get_setpoint(time_t sse)
{
	return 20.0f;
}
