#ifndef UTIL_H_
#define UTIL_H_

#include <unistd.h>

/*
 * public function prototypes
 */

/* repeated writes until n bytes written, or error */
ssize_t
writen(int fd, const void *buffer, size_t n);


/* repeated reads until n bytes written or error or EOF */
ssize_t
readn(int fd, void *buffer, size_t n);

#endif
