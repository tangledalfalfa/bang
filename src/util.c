#include <stddef.h>
#include <unistd.h>
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
