/*
 * Header file for thermostat schedule module
 */

#ifndef SCHEDULE_H_
#define SCHEDULE_H_

#include <time.h>

struct event_str {
    long sow;                   /* second of week */
    float setpoint_degc;        /* setpoint */
};

/*
 * public function prototypes
 */

float
sched_get_setpoint(time_t sse);

#endif
