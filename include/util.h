#ifndef UTIL_H_
#define UTIL_H_

#include <unistd.h>

/*
 * public function prototypes
 */

ssize_t
writen(int fd, const void *buffer, size_t n);

ssize_t
readn(int fd, void *buffer, size_t n);

#endif
