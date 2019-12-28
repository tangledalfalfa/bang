/*
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "util.h"

/*
 * writen, readn inspired by Michael Kerrisk, TLPI
 */

/* repeated writes until n bytes written, or error */
ssize_t
writen(int fd, const void *buffer, size_t n)
{
    const char *buf;
    size_t left;

    if (n == 0)
        return 0;

    buf = buffer;
    left = n;
    do {
        ssize_t m;

        m = write(fd, buf, left);
        if (m == -1) {
            if (errno == EINTR)
                continue;
            else
                return -1;
        }
        left -= m;
        buf += m;
    } while (left);

    return n;
}

/* repeated reads until n bytes written or error or EOF */
ssize_t
readn(int fd, void *buffer, size_t n)
{
    char *buf;
    size_t left;

    if (n == 0)
        return 0;

    buf = buffer;
    left = n;
    do {
        ssize_t m;

        m = read(fd, buf, left);
        if (m == 0)
            return n - left;    /* EOF */
        if (m == -1) {
            if (errno == EINTR)
                continue;
            else
                return -1;
        }

	left -= m;
        buf += m;
    } while (left);

    return n;
}

int
wait_for_next_second(void)
{
	int ret;
	struct timespec now, sleep_time, remain;

	ret = clock_gettime(CLOCK_REALTIME, &now);
	if (ret == -1)
		return -1;

	sleep_time.tv_sec = 0;
	/* intentionally 1 short to avoid setting nsec to 1 billion */
	sleep_time.tv_nsec = 999999999L - now.tv_nsec;

	for (;;) {
		ret = nanosleep(&sleep_time, &remain);
		if (ret == 0)
			break;
		if (errno != EINTR)
			return -1;

		sleep_time = remain;
	}

	return 0;
}

FILE *
open_dayfile(const char *data_dir, const struct tm *timestamp)
{
	char day_buf[20];
	char *path;
	FILE *result;

	if (data_dir == NULL)
		return stdout;

	if (strftime(day_buf, sizeof day_buf, "%Y%m%d.dat",
		     timestamp) == 0)
		return NULL;

	if (asprintf(&path, "%s/%s", data_dir, day_buf) == -1)
		return NULL;

	result = fopen(path, "a");
	free(path);

	return result;
}
