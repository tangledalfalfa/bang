#ifndef UTIL_H_
#define UTIL_H_

#include <unistd.h>
#include <time.h>

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
