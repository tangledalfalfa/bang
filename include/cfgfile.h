#ifndef CFGFILE_H_
#define CFGFILE_H_

#include <stddef.h>

#include "schedule.h"

int
cfg_load(const char *fname, struct schedule_str *schedule);

#endif
