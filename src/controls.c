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
get_mtime(const char *ctrl_dir, const char *fname, time_t *mtime)
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
check_hold(struct schedule_str *schedule)
{
	time_t mtime;
	char *path;
	FILE *hold_file;
	float hold_temp;
	int ret;

	if (get_mtime(schedule->ctrl_dir, CTRL_FNAME_HOLD, &mtime) == -1)
		return -1;

	if (mtime <= schedule->hold_mtime)
		return 0;

	/* update mtime so we don't do it again */
	schedule->hold_mtime = mtime;

	if (asprintf(&path, "%s/%s",
		     schedule->ctrl_dir, CTRL_FNAME_HOLD) == -1) {
		syslog(LOG_ERR,
		       "asprintf(%s/%s): %s",
		       schedule->ctrl_dir, CTRL_FNAME_HOLD, strerror(errno));
		return -1;
	}

	hold_file = fopen(path, "r");
	if (hold_file == NULL) {
		syslog(LOG_ERR, "check hold, fopen(%s): %s",
		       path, strerror(errno));
		free(path);
		return -1;
	}

	ret = fscanf(hold_file, " %f", &hold_temp);
	if (ret != 1) {
		syslog(LOG_INFO, "failed to read hold temp from %s",
		       path);
		free(path);
		return 0;
	}

	syslog(LOG_INFO, "HOLD: %.2f", hold_temp);

	/* convert to deg C if needed */
	if (schedule->units == UNITS_DEGF)
		hold_temp = degf_to_degc(hold_temp);
	else if (schedule->units == UNITS_AUTO)
		hold_temp = deg_to_degc_auto(hold_temp);

	schedule->hold_temp_degc = hold_temp; /* FIXME: units */
	schedule->hold_flag = true;

	free(path);

	return 0;
}

static int
check_advance(struct schedule_str *schedule)
{
	time_t mtime;

	if (get_mtime(schedule->ctrl_dir, CTRL_FNAME_ADVANCE, &mtime) == -1)
		return -1;

	if (mtime <= schedule->advance_mtime)
		return 0;

	schedule->advance_mtime = mtime;

	syslog(LOG_INFO, "ADVANCE");

	schedule->advance_flag = true;

	return 0;
}

static int
check_resume(struct schedule_str *schedule)
{
	time_t mtime;

	if (get_mtime(schedule->ctrl_dir, CTRL_FNAME_RESUME, &mtime) == -1)
		return -1;

	if (mtime <= schedule->resume_mtime)
		return 0;

	schedule->resume_mtime = mtime;

	syslog(LOG_INFO, "RESUME");

	schedule->hold_flag = false;
	schedule->advance_flag = false;

	return 0;
}

/*
 * public functions
 */

int
ctrls_init(struct schedule_str *schedule)
{
	if (get_mtime(schedule->ctrl_dir, CTRL_FNAME_HOLD,
		    &schedule->hold_mtime) == -1)
		return -1;

	if (get_mtime(schedule->ctrl_dir, CTRL_FNAME_ADVANCE,
		    &schedule->advance_mtime) == -1)
		return -1;

	if (get_mtime(schedule->ctrl_dir, CTRL_FNAME_RESUME,
		    &schedule->resume_mtime) == -1)
		return -1;

	schedule->curr_sow = -1;

	return 0;
}

int
ctrls_check(struct schedule_str *schedule)
{
	if (check_hold(schedule) == -1)
		return -1;

	if (check_advance(schedule) == -1)
		return -1;

	if (check_resume(schedule) == -1)
		return -1;

	return 0;
}
