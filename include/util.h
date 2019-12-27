#ifndef UTIL_H_
#define UTIL_H_

#include <unistd.h>
#include <time.h>

/* useful macros */

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define MAX(a, b) (((b) > (a)) ? (b) : (a))
#define MIN(a, b) (((b) < (a)) ? (b) : (a))

/*
 * public function prototypes
 */

/* repeated writes until n bytes written, or error */
ssize_t
writen(int fd, const void *buffer, size_t n);


/* repeated reads until n bytes written or error or EOF */
ssize_t
readn(int fd, void *buffer, size_t n);

/* wait until top of next second */
int
wait_for_next_second(void);

/*
 * return file handle for dayfile, or stdout if data_dir is NULL
 * returns NULL on error.
 */
FILE *
open_dayfile(const char *data_dir, const struct tm *timestamp);

#endif
