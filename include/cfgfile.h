/*
 */
#ifndef CFGFILE_H_
#define CFGFILE_H_

#include <stddef.h>

#ifndef SCHED_MAX_EVENTS
#define SCHED_MAX_EVENTS 100
#endif

enum units_enum {
	UNITS_DEGC,
	UNITS_DEGF,
	UNITS_AUTO
};

struct event_str {
	long sow;                   /* second of week */
	double setpoint_degc;       /* setpoint */
};

struct cfg_data_str {
	enum units_enum units;
	size_t num_events;
	struct event_str event[SCHED_MAX_EVENTS];
	const char *fname;        /* need to check for config file updates */
	time_t mtime;	          /* config file time of last modification */
};

int
cfg_load(const char *fname, struct cfg_data_str *cfg_data);

#endif
