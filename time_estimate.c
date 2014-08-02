
#include "time_estimate.h"
#include "units.h"

static const unsigned long long Milestones[] = {
    1 * Megabytes,
    10 * Megabytes,
    50 * Megabytes,
    500 * Megabytes,
    1 * (long long) Gigabytes,
    10 * (long long) Gigabytes,
    1024 * (long long) Gigabytes,
};
static const int NumMilestones = sizeof(Milestones) / sizeof(Milestones[0]);


typedef enum TimeUnits {
    Seconds = 1,
    Minutes = 60,
    Hours = 60 * 60,
    Days = 60 * 60 * 24,
} TimeUnits;


void estimate_time(TimeEstimate* te, unsigned long long total_bytes, double bytes_per_sec) {
    unsigned long long next_milestone;
    unsigned long long remaining;
    double secs;
    int i;

    if (!te) {
        return;
    }

    next_milestone = Milestones[0];
    for (i=1; total_bytes >= next_milestone && i < NumMilestones; ++i) {
        next_milestone = Milestones[i];
    }

    while (total_bytes >= next_milestone) {
        next_milestone += Milestones[NumMilestones - 1];

        if (next_milestone < Milestones[NumMilestones - 1]) {
            // Woops, overflowed.
            next_milestone = total_bytes;
            break;
        }
    }

    remaining = next_milestone - total_bytes;
    secs = remaining / bytes_per_sec;

    te->milestone_bytes = next_milestone;
    te->bytes_remaining = remaining;
    te->secs_remaining = secs;

    if (secs >= Days) {
        te->time_remaining = secs / Days;
        te->time_unit = "days";
    } else if (secs >= Hours) {
        te->time_remaining = secs / Hours;
        te->time_unit = "hrs";
    } else if (secs >= Minutes) {
        te->time_remaining = secs / Minutes;
        te->time_unit = "mins";
    } else {
        te->time_remaining = secs;
        te->time_unit = "secs";
    }
}
