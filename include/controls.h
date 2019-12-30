/*
 */

#ifndef CONTROLS_H_
#define CONTROLS_H_

#include "schedule.h"

/*
 * public function prototypes
 */

int
ctrls_init(struct schedule_str *schedule);

int
ctrls_check(struct schedule_str *schedule);

#endif
