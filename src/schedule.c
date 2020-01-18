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

#define SEC_PER_DAY (24 * 60 * 60)
#define SEC_PER_WEEK (7 * SEC_PER_DAY)

/* calculate seconds-of-week, in local time */
static long
sse_to_sow(time_t sse)
{
	struct tm bdt;		/* broken-down time */

	localtime_r(&sse, &bdt); /* for tm_gmtoff */

	sse -= 3 * SEC_PER_DAY;	/* January 1 1970 was a Thursday */
	sse += bdt.tm_gmtoff;	/* adjust to local time */
	return sse % SEC_PER_WEEK;
}

/* find event containing current setpoint, set curr_idx */
static void
init_index(long now_sow, struct schedule_str *schedule)
{
	size_t i;

	/* find next (upcoming) event */
	for (i = 0; i < schedule->config.num_events; i++)
		if (schedule->config.event[i].sow > now_sow)
			break;
	/* i is now 0..num_events */

	/* back up one (0 wraps back to last) */
	if (i == 0)
		i = schedule->config.num_events - 1;
	else
		i--;

	schedule->curr_idx = i;
}

/*
 * public functions
 */

double
sched_get_setpoint(time_t now_sse, struct schedule_str *schedule)
{
	long now_sow;		/* current second-of-week */
	size_t idx;

	now_sow = sse_to_sow(now_sse);

	if (schedule->curr_idx == -1) {
		/* initialize index */
		init_index(now_sow, schedule);
		/* cancel advance mode for new schedule */
		schedule->advance_flag = false;
	} else {
		/* update index */
		long t_0;	/* current time */
		long t_1;	/* time of upcoming event */
		size_t next_idx;

		t_0 = now_sow;

		/* t_1 is the time of the upcoming event */
		next_idx = (schedule->curr_idx + 1)
			% schedule->config.num_events;
		t_1 = schedule->config.event[next_idx].sow;

		/*
		 * There's a chance that t_0, t_1 wrapped back
		 * at Sunday midnight since the previous schedule
		 * check. Unwrap these.
		 */
		if (t_0 < schedule->curr_sow)
			t_0 += SEC_PER_WEEK;

		if (t_1 < schedule->curr_sow)
			t_1 += SEC_PER_WEEK;

		/* detect event: current time >= time of next event */
		if (t_0 >= t_1) {
			schedule->curr_idx = next_idx;
			/* cancel advance mode at event boundary */
			schedule->advance_flag = false;
		}
	}

	schedule->curr_sow = now_sow; /* update the current time */

	/* implement advance mode */
	if (schedule->advance_flag)
		idx = (schedule->curr_idx + 1) % schedule->config.num_events;
	else
		idx = schedule->curr_idx;

	return schedule->config.event[idx].setpoint_degc;
}
