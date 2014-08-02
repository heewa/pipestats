
#ifndef __TIME_ESTIMATE_H__
#define __TIME_ESTIMATE_H__

typedef struct TimeEstimate {
    unsigned long long milestone_bytes;
    unsigned long long bytes_remaining;

    double secs_remaining;
    double time_remaining;
    const char* time_unit;
} TimeEstimate;

void estimate_time(TimeEstimate* te, unsigned long long total_bytes, double bytes_per_sec);

#endif

