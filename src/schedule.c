/*
 * thermostat schedule module
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

#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "schedule.h"

/* calculate seconds-of-week, in local time */
static long
sse_to_sow(time_t sse)
{
	struct tm bdt;		/* broken-down time */

	localtime_r(&sse, &bdt); /* for tm_gmtoff */

	sse -= 3 * (24 * 60 * 60); /* January 1 1970 was a Thursday */
	sse += bdt.tm_gmtoff;	   /* adjust to local time */
	return sse % (7 * 24 * 60 * 60);
}

/*
 * public functions
 */

double
sched_get_setpoint(time_t now, struct schedule_str *schedule)
{
	long sow;		/* current second-of-week */
	size_t i;

	sow = sse_to_sow(now);

	/* find next (upcoming) event */
	/* FIXME: binary search more efficient... */
	for (i = 0; i < schedule->num_events; i++)
		if (schedule->event[i].sow > sow)
			break;
	/* i is now 0..num_events */

	/* back up one (0 goes to last) */
	if (i == 0)
		i = schedule->num_events - 1;
	else
		i--;

	if (schedule->curr_sow == -1) {
		schedule->curr_sow = schedule->event[i].sow;
	} else if (schedule->curr_sow != schedule->event[i].sow) {
		schedule->curr_sow = schedule->event[i].sow;
		/* advance only lasts until next event */
		schedule->advance_flag = false;
	}

	if (schedule->advance_flag)
		i = (i + 1) % schedule->num_events;

	return schedule->event[i].setpoint_degc;
}
