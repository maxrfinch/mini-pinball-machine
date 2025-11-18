#include "util.h"
#include <sys/time.h>
#include <stddef.h>

long long millis(void) {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;
    return milliseconds;
}
