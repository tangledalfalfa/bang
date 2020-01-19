/*
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>

#include "controls.h"
#include "schedule.h"
#include "util.h"

#ifndef CTRL_FNAME_HOLD
#define CTRL_FNAME_HOLD    "hold"
#endif

#ifndef CTRL_FNAME_OVERRIDE
#define CTRL_FNAME_OVERRIDE "override"
#endif

#ifndef CTRL_FNAME_ADVANCE
#define CTRL_FNAME_ADVANCE "advance"
#endif

#ifndef CTRL_FNAME_RESUME
#define CTRL_FNAME_RESUME  "resume"
#endif

/*
 * private functions
 */

static int
get_modtime(const char *ctrl_dir, const char *fname, time_t *mtime)
{
	char *path;
	struct stat statbuf;

	if (asprintf(&path, "%s/%s", ctrl_dir, fname) == -1) {
		syslog(LOG_ERR,
		       "asprintf(%s/%s): %s",
		       ctrl_dir, fname, strerror(errno));
		return -1;
	}

	if (stat(path, &statbuf) == -1) {
		/* file does not exist.  this is OK */
		*mtime = 0;
		free(path);
		return 0;
	}

	free(path);

	*mtime = statbuf.st_mtim.tv_sec;

	return 0;
}

static int
read_temp_degc(struct schedule_str *schedule, const char *fname,
	double *temp_degc)
{
	char *path;
	FILE *infile;
	int ret;
	float temp;

	if (asprintf(&path, "%s/%s",
		     schedule->ctrl_dir, fname) == -1) {
		syslog(LOG_ERR,
		       "asprintf(%s/%s): %s",
		       schedule->ctrl_dir, fname, strerror(errno));
		return -1;
	}

	infile = fopen(path, "r");
	if (infile == NULL) {
		syslog(LOG_ERR, "read temp, fopen(%s): %s",
		       path, strerror(errno));
		free(path);
		return -1;
	}

	ret = fscanf(infile, " %f", &temp);
	if (ret != 1) {
		syslog(LOG_INFO, "failed to read temp from %s",
		       path);
		free(path);
		return 0;
	}

	free(path);

	/* convert to deg C if needed */
	if (schedule->config.units == UNITS_DEGF)
		temp = degf_to_degc(temp);
	else if (schedule->config.units == UNITS_AUTO)
		temp = deg_to_degc_auto(temp);

	if (temp_degc)
		*temp_degc = temp;

	return 0;
}

static int
check_hold(struct schedule_str *schedule)
{
	time_t mtime;

	if (get_modtime(schedule->ctrl_dir, CTRL_FNAME_HOLD, &mtime) == -1)
		return -1;

	if (mtime <= schedule->hold_mtime)
		return 0;

	/* update mtime so we don't do it again */
	schedule->hold_mtime = mtime;

	/* input setpoint from file */
	if (read_temp_degc(schedule, CTRL_FNAME_HOLD,
			   &schedule->hold_temp_degc) == -1)
		return -1;

	schedule->hold_flag = true;

	syslog(LOG_INFO, "HOLD: %.2f", schedule->hold_temp_degc);

	/* this overrides any OVERRIDE, ADVANCE in effect */
	schedule->override_flag = false;
	schedule->advance_flag = false;

	return 0;
}

static int
check_override(struct schedule_str *schedule)
{
	time_t mtime;

	if (get_modtime(schedule->ctrl_dir, CTRL_FNAME_OVERRIDE, &mtime) == -1)
		return -1;

	if (mtime <= schedule->override_mtime)
		return 0;

	/* update mtime so we don't do it again */
	schedule->override_mtime = mtime;

	/* input setpoint from file */
	if (read_temp_degc(schedule, CTRL_FNAME_OVERRIDE,
			   &schedule->override_temp_degc) == -1)
		return -1;

	schedule->override_flag = true;

	syslog(LOG_INFO, "OVERRIDE: %.2f", schedule->override_temp_degc);

	/* this overrides any HOLD, ADVANCE in effect */
	schedule->hold_flag = false;
	schedule->advance_flag = false;

	return 0;
}

static int
check_advance(struct schedule_str *schedule)
{
	time_t mtime;

	if (get_modtime(schedule->ctrl_dir, CTRL_FNAME_ADVANCE, &mtime) == -1)
		return -1;

	if (mtime <= schedule->advance_mtime)
		return 0;

	schedule->advance_mtime = mtime;

	syslog(LOG_INFO, "ADVANCE");

	schedule->advance_flag = true;

	/* this overrides any HOLD, OVERRIDE in effect */
	schedule->hold_flag = false;
	schedule->override_flag = false;

	return 0;
}

static int
check_resume(struct schedule_str *schedule)
{
	time_t mtime;

	if (get_modtime(schedule->ctrl_dir, CTRL_FNAME_RESUME, &mtime) == -1)
		return -1;

	if (mtime <= schedule->resume_mtime)
		return 0;

	schedule->resume_mtime = mtime;

	syslog(LOG_INFO, "RESUME");

	schedule->hold_flag = false;
	schedule->override_flag = false;
	schedule->advance_flag = false;

	return 0;
}

/*
 * public functions
 */

int
ctrls_init(struct schedule_str *schedule)
{
	if (get_modtime(schedule->ctrl_dir, CTRL_FNAME_HOLD,
		    &schedule->hold_mtime) == -1)
		return -1;

	if (get_modtime(schedule->ctrl_dir, CTRL_FNAME_OVERRIDE,
		    &schedule->override_mtime) == -1)
		return -1;

	if (get_modtime(schedule->ctrl_dir, CTRL_FNAME_ADVANCE,
		    &schedule->advance_mtime) == -1)
		return -1;

	if (get_modtime(schedule->ctrl_dir, CTRL_FNAME_RESUME,
		    &schedule->resume_mtime) == -1)
		return -1;

	schedule->hold_flag = schedule->override_flag
		= schedule->advance_flag = false;

	return 0;
}

int
ctrls_check(struct schedule_str *schedule)
{
	if (check_hold(schedule) == -1)
		return -1;

	if (check_override(schedule) == -1)
		return -1;

	if (check_advance(schedule) == -1)
		return -1;

	if (check_resume(schedule) == -1)
		return -1;

	return 0;
}
