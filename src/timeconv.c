#include "define.h"



char* pst_fileTimeToAscii(const FILETIME* filetime, char* result) {
    time_t t;
    char *r;
    t = pst_fileTimeToUnixTime(filetime);
    // ctime_r returns NULL for out-of-range times (a crafted FILETIME can force
    // this); hand back an empty string rather than a NULL the callers deref.
    r = ctime_r(&t, result);
    if (!r && result) { result[0] = '\0'; r = result; }
    return r;
}

size_t pst_fileTimeToString(const FILETIME* filetime, const char* date_format, char* result) {
    time_t t;
    struct tm *tm;
    t = pst_fileTimeToUnixTime(filetime);
    // localtime() can return NULL for out-of-range times; passing that to
    // strftime is undefined. Emit an empty result instead.
    tm = localtime(&t);
    if (!tm) {
        if (result) result[0] = '\0';
        return 0;
    }
    return strftime(result, MAXDATEFMTLEN-1, date_format, tm);
}

void pst_fileTimeToStructTM (const FILETIME *filetime, struct tm *result) {
    time_t t1;
    t1 = pst_fileTimeToUnixTime(filetime);
    // gmtime_r can fail (NULL) for out-of-range times; leave a zeroed struct
    // rather than an uninitialized one.
    if (!gmtime_r(&t1, result)) {
        memset(result, 0, sizeof(*result));
    }
}


time_t pst_fileTimeToUnixTime(const FILETIME *filetime)
{
    uint64_t t = filetime->dwHighDateTime;
    const uint64_t bias = 11644473600LL;
    t <<= 32;
    t += filetime->dwLowDateTime;
    t /= 10000000;
    t -= bias;
    return ((t > (uint64_t)0x000000007fffffff) && (sizeof(time_t) <= 4)) ? 0 : (time_t)t;
}

