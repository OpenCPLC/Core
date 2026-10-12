// hal/host/inc/posix.h

#ifndef POSIX_H_
#define POSIX_H_

// POSIX calls of a Linux host, its `sleep` renamed out of the way of the `sleep` of VRTS
#define sleep posix_sleep
#include <unistd.h>
#undef sleep

#endif
