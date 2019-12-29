/*
 */
#include "config.h"

#include <stddef.h>
#include <libconfig.h>

#include "cfgfile.h"
#include "schedule.h"

static void
print_schedule(const struct schedule_str *schedule)
{
	int i;

	for (i = 0; i < schedule->num_events; i++) {
		printf("%f\n",
		       schedule->event[i].setpoint_degc);
	}
}

static int
load_event(config_setting_t *event_setting, struct event_str *event)
{
	config_setting_t *time_setting;
	int hour, minute;
	double value;

	time_setting = config_setting_get_member(event_setting, "time");
	if (time_setting == NULL) {
		fprintf(stderr, "failed to find time for event, line %d\n",
			config_setting_source_line(event_setting));
		return -1;
	}

	/*
	 * day can be:
	 *    tue
	 *    wed-fri
	 *    mon,wed,fri
	 *    mon-wed,sat
	 * etc.
	 */

	if (!config_setting_lookup_int(time_setting, "hour", &hour)) {
		fprintf(stderr, "failed to find hour for event, line %d\n",
			config_setting_source_line(time_setting));
		return -1;
	}

	if (!config_setting_lookup_int(time_setting, "min", &minute)) {
		fprintf(stderr, "failed to find minute for event, line %d\n",
			config_setting_source_line(time_setting));
		return -1;
	}

	if (config_setting_lookup_float(event_setting, "setpoint", &value)) {
		event->setpoint_degc = value;
	} else {
		fprintf(stderr, "failed fo find setpoint for event, line %d\n",
			config_setting_source_line(event_setting));
		return -1;
	}
	return 0;
}

int
cfg_load(const char *fname, struct schedule_str *schedule)
{
	config_t cfg;
	config_setting_t *schedule_setting;
	int i;

	config_init(&cfg);

	if (!config_read_file(&cfg, fname)) {
		fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
			config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return -1;
	}

	schedule_setting = config_lookup(&cfg, "schedule");
	if (schedule_setting == NULL) {
		fprintf(stderr, "no schedule setting found in %s\n",
			fname);
		config_destroy(&cfg);
		return -1;
	}

        schedule->num_events = config_setting_length(schedule_setting);
	for (i = 0; i < schedule->num_events; i++) {
		config_setting_t *event_setting;

		event_setting = config_setting_get_elem(schedule_setting, i);
		if (load_event(event_setting, schedule->event + i) == -1) {
			config_destroy(&cfg);
			return -1;
		}
	}

	print_schedule(schedule);

	config_destroy(&cfg);

	return 0;
}
