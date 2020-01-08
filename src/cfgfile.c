/*
 */
#include "config.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libconfig.h>
#include <errno.h>

#include "cfgfile.h"
#include "schedule.h"
#include "util.h"

/*
 * finite state machine for parsing day specs in the form:
 *     tue, thu, fri - sun
 */

/* states */
#define ST_START 0
#define ST_FROM  1
#define ST_DASH  2
#define ST_TO    3
#define ST_END   5
#define ST_ERROR 6

/* char types */
#define CT_ALPHA  0		/* a-z */
#define CT_SPACE  1		/* space or tab */
#define CT_DASH   2
#define CT_COMMA  3
#define CT_ERROR  4		/* unrecognized */

static int
get_char_type(char c)
{
	return isalpha(c) ? CT_ALPHA
		: (c == ' ') ? CT_SPACE
		: (c == ',') ? CT_COMMA
		: (c == '-') ? CT_DASH : CT_ERROR;
}

static int
get_day_index(const char *day)
{
	const char *day_table[] =
		{ "sun", "mon", "tue", "wed", "thu", "fri", "sat" };
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(day_table); i++)
		if (memcmp(day, day_table[i], 3) == 0)
			return i;
	return -1;

}

static int
update_mask(const char *from_buf, const char *to_buf, uint8_t *mask)
{
	int from_idx, to_idx, idx;

	/* special handling for "all" */
	if (memcmp(from_buf, "all", 3) == 0) {
		*mask = 0x7F;
		return 0;
	}

	from_idx = get_day_index(from_buf);
	to_idx = get_day_index(to_buf);

	if ((from_idx == -1) || (to_idx == -1))
		return -1;

	/* support wrapping, e.g. fri-mon */
	if (to_idx < from_idx)
		to_idx += 7;

	for (idx = from_idx; idx <= to_idx; idx++)
		*mask |= (1 << (idx % 7)); /* modulo for "wrapped" days */

	return 0;
}

/*
 * day_spec can be:
 *    tue
 *    wed-fri
 *    mon,wed,fri
 *    mon-wed,sat
 *    fri-mon
 *    all
 *    Tuesday, Thursday
 * etc.
 */

static int
parse_day(const char *day_spec, uint8_t *mask)
{
	int state;
	char from_buf[3];
	char to_buf[3];
	unsigned idx;		/* index into from_buf, to_buf */

	*mask = 0;
	state = ST_START;
	idx = 0;

	while (*day_spec && (state != ST_ERROR)) {
		int ct;

		/* get char type: alpha, space, dash, comma or other */
		ct = get_char_type(*day_spec);

		switch (state) {
		case ST_START:

			if (ct == CT_ALPHA) {
				from_buf[idx++] = tolower(*day_spec);
				state = ST_FROM;
			} else if (ct != CT_SPACE) {
				state = ST_ERROR;
			}
			break;

		case ST_FROM:

			if (ct == CT_ALPHA) {
				/* ignore additional char ("tuesday" is OK) */
				if (idx < sizeof from_buf)
					from_buf[idx++] = tolower(*day_spec);
			} else if (idx < sizeof from_buf) {
				state = ST_ERROR;
			} else if (ct == CT_DASH) {
				idx = 0;
				state = ST_DASH;
			} else if (ct == CT_COMMA) {
				/* single day: copy from -> to */
				memcpy(to_buf, from_buf, sizeof to_buf);
				if (update_mask(from_buf, to_buf, mask) == -1) {
					state = ST_ERROR;
				} else {
					idx = 0;
					state = ST_START;
				}
			} else if (ct != CT_SPACE){
				state = ST_ERROR;
			}
			break;

		case ST_DASH:

			if (ct == CT_ALPHA) {
				to_buf[idx++] = tolower(*day_spec);
				state = ST_TO;
			} else if (ct != CT_SPACE) {
				state = ST_ERROR;
			}
			break;

		case ST_TO:

			if (ct == CT_ALPHA) {
				/* ignore additional chars */
				if (idx < sizeof to_buf)
					to_buf[idx++] = tolower(*day_spec);
			} else if (idx < sizeof to_buf) {
				state = ST_ERROR;
			} else if (ct == CT_COMMA) {
				if (update_mask(from_buf, to_buf, mask) == -1) {
					state = ST_ERROR;
				} else {
					idx = 0;
					state = ST_START;
				}
			} else if (ct != CT_SPACE) {
				state = ST_ERROR;
			}
			break;

		default:
			syslog(LOG_ERR, "state machine error");
			return -1;
		}

		day_spec++;
	}

	if (((state == ST_FROM) || (state == ST_TO))
	    && (idx == sizeof from_buf)) {
		if (state == ST_FROM)
			memcpy(to_buf, from_buf, sizeof to_buf);
		return update_mask(from_buf, to_buf, mask);
	} else {
		return -1;
	}
}

static int
load_time(config_setting_t *time_setting,
	  uint8_t *day_mask, int *hour, int *minute)
{
	const char *day;

	if (!config_setting_lookup_string(time_setting, "day", &day)) {
		syslog(LOG_ERR, "failed to find day for event, line %d",
			config_setting_source_line(time_setting));
		return -1;
	}

	if (parse_day(day, day_mask) == -1) {
		syslog(LOG_ERR, "syntax error, day spec %s, line %d",
			day,
			config_setting_source_line(time_setting));
		return -1;
	}

	if (!config_setting_lookup_int(time_setting, "hour", hour)) {
		syslog(LOG_ERR, "failed to find hour for event, line %d",
			config_setting_source_line(time_setting));
		return -1;
	}

	if (!config_setting_lookup_int(time_setting, "min", minute))
		*minute = 0; 	/* default to top of hour */

	if ((*hour < 0) || (*hour >= 24)) {
		syslog(LOG_ERR, "hour %d invalid, line %d", *hour,
			config_setting_source_line(time_setting));
		return -1;
	}

	if ((*minute < 0) || (*minute >= 60)) {
		syslog(LOG_ERR, "minute %d invalid, line %d", *minute,
			config_setting_source_line(time_setting));
		return -1;
	}

	//printf("%s 0x%02X %02d:%02d\n", day, *day_mask, *hour, *minute);

	return 0;
}

static int
update_events(struct schedule_str *schedule, uint8_t day_mask,
	      int hour, int minute, double setpoint)
{
	uint8_t mask;
	long day;

	for (day = 0, mask = 0x01; mask != 0x80; day++, mask <<= 1) {
		if (mask & day_mask) {
			if (schedule->num_events == ARRAY_SIZE(schedule->event))
				return -1;

			schedule->event[schedule->num_events].sow
				= (((day * 24) + hour) * 60 + minute) * 60;
			schedule->event[schedule->num_events].setpoint_degc
				= setpoint;
			schedule->num_events++;
		}
	}

	return 0;
}

static double
to_degc(double temp, enum units_enum units)
{
	if (units == UNITS_DEGC)
		return temp;
	else if (units == UNITS_DEGF)
		return degf_to_degc(temp);
	else
		return deg_to_degc_auto(temp);
}

static int
load_event(config_setting_t *event_setting, struct schedule_str *schedule)
{
	config_setting_t *time_setting;
	uint8_t day_mask;
	int hour, minute;
	double setpoint;
	int setpt_int;

	time_setting = config_setting_get_member(event_setting, "time");
	if (time_setting == NULL) {
		syslog(LOG_ERR, "failed to find time for event, line %d",
			config_setting_source_line(event_setting));
		return -1;
	}

	if (load_time(time_setting, &day_mask, &hour, &minute) == -1)
		return -1;

	if (config_setting_lookup_float(event_setting, "setpoint",
		    &setpoint)) {
	} else if (config_setting_lookup_int(event_setting, "setpoint",
			   &setpt_int)) {
		/* libconfig won't parse a float without a decimal pt */
		setpoint = setpt_int;
	} else {
		syslog(LOG_ERR, "failed fo find setpoint for event, line %d",
		       config_setting_source_line(event_setting));
		return -1;
	}
	setpoint = to_degc(setpoint, schedule->units);

	if (update_events(schedule, day_mask, hour, minute, setpoint) == -1)
		return -1;

	//printf("%f deg\n", setpoint);

	return 0;
}

/* callback for qsort */
static int
compare_events(const struct event_str *event1,
	const struct event_str *event2)
{
	if (event1->sow < event2->sow)
		return -1;
	else if (event1->sow > event2->sow)
		return 1;
	else
		return 0;
}

/* record schedule to syslog */
static void
log_schedule(const struct schedule_str *schedule)
{
	size_t i;
	char *units;

	units = (schedule->units == UNITS_DEGC) ? "deg C"
		: (schedule->units == UNITS_DEGF) ? "deg F" : "auto";
	syslog(LOG_INFO, "units: %s", units);

	for (i = 0; i < schedule->num_events; i++) {
		syslog(LOG_INFO,
		       "%3u %6ld %4.1f",
		       i, schedule->event[i].sow,
		       schedule->event[i].setpoint_degc);
	}
}

/*
 * public functions
 */

int
cfg_load(const char *fname, struct schedule_str *schedule)
{
	struct stat statbuf;
	config_t cfg;
	config_setting_t *schedule_setting;
	int i, count;
	const char *units;

	/* initialize config file modification time */
	if (stat(fname, &statbuf) == -1) {
		syslog(LOG_ERR, "stat(%s): %s",
		       fname, strerror(errno));
		exit(EXIT_FAILURE);
	}
	schedule->config_mtime = statbuf.st_mtim.tv_sec;

	config_init(&cfg);

	if (!config_read_file(&cfg, fname)) {
		syslog(LOG_ERR, "%s:%d - %s", config_error_file(&cfg),
			config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return -1;
	}

	/* get temperature units: deg C, deg F, or auto */
	if (config_lookup_string(&cfg, "units", &units)) {
		char c;

		/* just look at the first char */
		c = tolower(*units);
		schedule->units = (c == 'c') ? UNITS_DEGC
			: (c == 'f') ? UNITS_DEGF : UNITS_AUTO;
	} else {
		schedule->units = UNITS_AUTO; /* defaults to AUTO */
	}

	schedule_setting = config_lookup(&cfg, "schedule");
	if (schedule_setting == NULL) {
		syslog(LOG_ERR, "no schedule setting found in %s",
			fname);
		config_destroy(&cfg);
		return -1;
	}

	schedule->num_events = 0;
        count = config_setting_length(schedule_setting);
	if (count == 0) {
		syslog(LOG_ERR, "file %s: schedule is empty",
			fname);
		return -1;
	}

	for (i = 0; i < count; i++) {
		config_setting_t *event_setting;

		event_setting = config_setting_get_elem(schedule_setting, i);
		if (load_event(event_setting, schedule) == -1) {
			config_destroy(&cfg);
			return -1;
		}
	}

	config_destroy(&cfg);

	qsort(schedule->event, schedule->num_events,
	      sizeof schedule->event[0],
	      (int (*)(const void *, const void *))compare_events);

	log_schedule(schedule);

	return 0;
}
