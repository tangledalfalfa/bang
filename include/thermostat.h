/*
 * Header file for thermostat module
 */

#ifndef THERMOSTAT_H_
#define THERMOSTAT_H_

#include <gpiod.h>

/*
 * public function prototypes
 */

int
tstat_control(struct gpiod_line *line, int mcp9808_fd, const char *data_dir);

#endif