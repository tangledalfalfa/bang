/*
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
	sleep_time.tv_nsec = 1000000000L - now.tv_nsec;

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
