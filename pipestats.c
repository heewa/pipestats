
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <getopt.h>

#define BUF_SIZE (1024)

typedef struct Stats {
    unsigned long int total_bytes;
    unsigned int bytes_since;

    struct timeval last_report;
    struct timeval start;

    unsigned int num_errors;
    unsigned int underwrites;
    unsigned int underwritten_bytes;
} Stats;
Stats stats;


typedef enum Unit {
    Human = 0,
    Bytes = 1,
    Kilobytes = 1024,
    Megabytes = 1024 * 1024,
    Gigabytes = 1024 * 1024 * 1024,
} Unit;


typedef struct Options {
    double freq;
    Unit unit;
} Options;
Options options;


static volatile sig_atomic_t should_report = 0;


double elapsed_sec(struct timeval* end, struct timeval* start);
double adjust_unit(double bytes, Unit target_unit);
const char* unit_name(double bytes, Unit target_unit);
Unit find_unit(double bytes);


int read_options();
int setup();


void print_report();
void print_final_report();


void interval(int signal);
void cleanup(int signal);


int main(int argc, char** argv) {
    char buff[BUF_SIZE];
    size_t buff_offset = 0;
    int r;
    int done = 0;
    int bytes_read = 0;

    if ((r = read_options(argc, argv)) != 0) {
        return r;
    }

    if ((r = setup()) != 0) {
        return r;
    }

    while (!done) {
        if (should_report) {
            // Report first, then clear flag, so we don't bother reporting
            // back-to-back in overlap scenarios.
            print_report();
            should_report = 0;
        }

        // Only read more if we've already written everything we already had.
        if (bytes_read == 0) {
            bytes_read = fread(buff, 1, BUF_SIZE, stdin);
        }

        if (bytes_read > 0) {
            int bytes_written;
            int err;

            // But first update counters, cuz we might report white this is
            // going out to the buffer? Is that sound logic? woof!
            bytes_written = fwrite(buff + buff_offset, 1, bytes_read, stdout);
            stats.total_bytes += bytes_written;
            stats.bytes_since += bytes_written;

            // Sigh, check some stupid scenarios.
            if (bytes_read < 0) {
                fprintf(stderr, "Read negative bytes? %d\n", bytes_read);
                abort();
            } else if (bytes_written < 0) {
                fprintf(stderr, "Wrote negative bytes? %d\n", bytes_written);
                abort();
            } else if (bytes_written > bytes_read) {
                fprintf(stderr, "Wrote more (%d b) that read (%d b)?\n",
                        bytes_written, bytes_read);
                abort();
            }

            if (bytes_written == bytes_read) {
                bytes_read = 0;
                buff_offset = 0;
            } else {
                if ((err = ferror(stdout)) != 0) {
                    ++stats.num_errors;
                }

                // Write the rest next time around.
                if (bytes_written > 0) {
                    bytes_read -= bytes_written;
                    buff_offset += bytes_written;

                    // Only pay attention to partial writes. When nothing was
                    // written, that could be cuz of an error.
                    ++stats.underwrites;
                    stats.underwritten_bytes += (bytes_read - bytes_written);
                }
            }
        } else {
            // Only bother checking if no more data was read.
            done = feof(stdin);
        }
    }

    // Just for timing niceness, flush buffers before printing report.
    fflush(stdout);
    fclose(stdout);

    print_final_report();

    return 0;
}


int read_options(int argc, char** argv) {
    int opt = 0;
    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"human", no_argument, NULL, 'H'},
        {"freq", required_argument, NULL, 'f'},
        {0, 0, 0, 0}
    };

    // Init with defaults.
    options.freq = 2.0;
    options.unit = Human;

    while (opt != -1) {
        int option_index = 0;

        opt = getopt_long(argc, argv, "hHBKMGf:", long_options, &option_index);
        switch (opt) {
        case -1:
            break;

        case 'h':
            printf("usage: %s [options]\n"
                   "\n"
                   "    -f/--freq SECONDS    Report frequency, 0 to disable.\n"
                   "\n"
                   "pipestats reads from stdin, writes that input to stdout, "
                   "and reports stats about data transfered to stderr.\n",
                   argv[0]);
            return -1;
            break;

        case 'H':
            options.unit = Human;
            break;

        case 'B':
            options.unit = Bytes;
            break;

        case 'K':
            options.unit = Kilobytes;
            break;

        case 'M':
            options.unit = Megabytes;
            break;

        case 'G':
            options.unit = Gigabytes;
            break;

        case 'f':
            options.freq = strtod(optarg, NULL);
            if (options.freq < 0) {
                fprintf(stderr, "ERROR: frequency must be >= 0\n");
                return -1;
            }
            break;

        case '?':
            return -1;
            break;

        default:
            return -1;
            break;
        }
    }

    return 0;
}


int setup() {
    struct itimerval interval_timer;
    struct sigaction interval_action;
    struct sigaction cleanup_action;
    int abort_signals[] = {SIGHUP, SIGINT, SIGQUIT, SIGABRT, SIGPIPE, SIGTERM};
    int i;

    // Init stats.
    memset(&stats, 0, sizeof(Stats));
    gettimeofday(&stats.start, NULL);
    stats.last_report = stats.start;

    // Init timer. Use a timer instead of reporting in the main loop so we
    // can use blocking reads (while reporting consistently), instead of
    // a wasteful tight loop.
    if (options.freq > 0) {
        interval_timer.it_interval.tv_sec = (int) options.freq;
        interval_timer.it_interval.tv_usec =
            (options.freq- ((int) options.freq)) * (1000 * 1000);
        interval_timer.it_value = interval_timer.it_interval;

        memset(&interval_action, 0, sizeof(struct sigaction));
        interval_action.sa_handler = &interval;
        if (sigaction(SIGALRM, &interval_action, NULL) != 0) {
            fprintf(stderr, "Failed to set up timer handler.\n");
            return -1;
        }
        if (setitimer(ITIMER_REAL, &interval_timer, NULL) != 0) {
            fprintf(stderr, "Failed to start timer.\n");
            return -1;
        }
    }

    // Set up handler for exiting, to print a final report, even if aborted.
    memset(&cleanup_action, 0, sizeof(struct sigaction));
    cleanup_action.sa_handler = &cleanup;
    for (i=sizeof(abort_signals) / sizeof(abort_signals[0]) - 1; i >= 0; --i) {
        if (sigaction(abort_signals[i], &cleanup_action, NULL) != 0) {
            fprintf(stderr, "Failed to set cleanup handler for sig %d.\n",
                    i);
            return -1;
        }
    }

    return 0;
}


double elapsed_sec(struct timeval* end, struct timeval* start) {
    double sec = end->tv_sec - start->tv_sec;
    double usec = end->tv_usec - start->tv_usec;
    return sec + usec / (1000 * 1000);
}


double adjust_unit(double bytes, Unit target_unit) {
    if (target_unit != Human) {
        return bytes / (double) target_unit;
    }
    return bytes / (double) find_unit(bytes);
}


Unit find_unit(double bytes) {
    if (bytes >= Gigabytes) {
        return Gigabytes;
    } else if (bytes >= Megabytes) {
        return Megabytes;
    } else if (bytes >= Kilobytes) {
        return Kilobytes;
    }
    return Bytes;
}


const char* unit_name(double bytes, Unit target_unit) {
    if (target_unit == Human) {
        target_unit = find_unit(bytes);
    }

    switch (target_unit) {
    case Gigabytes:
        return "GB";
        break;

    case Megabytes:
        return "MB";
        break;

    case Kilobytes:
        return "KB";
        break;

    case Bytes:
        return "B";
        break;

    default:
        break;
    }

    return "??";
}


void print_report() {
    struct timeval now;
    double elapsed;

    double data_amount_since = adjust_unit(stats.bytes_since, options.unit);
    const char* data_amount_since_unit = unit_name(stats.bytes_since, options.unit);

    double data_amount_total = adjust_unit(stats.total_bytes, options.unit);
    const char* data_amount_total_unit = unit_name(stats.total_bytes, options.unit);

    gettimeofday(&now, NULL);
    elapsed = elapsed_sec(&now, &stats.last_report);

    fprintf(stderr,
            "%3.2f %s/s, "
            "%3.2f %s total, "
            "%3.2f %s since last report\n",
            data_amount_since / elapsed, data_amount_since_unit,
            data_amount_total, data_amount_total_unit,
            data_amount_since, data_amount_since_unit);

    stats.bytes_since = 0;
    stats.last_report = now;
}


void print_final_report() {
    struct timeval now;
    double elapsed;

    double data_amount = adjust_unit(stats.total_bytes, options.unit);
    const char* data_amount_unit = unit_name(stats.total_bytes, options.unit);

    gettimeofday(&now, NULL);
    elapsed = elapsed_sec(&now, &stats.start);

    fprintf(stderr, "%3.2f %s (%lu bytes) total over %.2f sec, avg %.2f %s/s\n",
            data_amount, data_amount_unit,
            stats.total_bytes,
            elapsed,
            data_amount / elapsed, data_amount_unit);

    if (stats.num_errors > 0) {
        fprintf(stderr, "Got %d write errors.\n",
                stats.num_errors);
    }

    if (stats.underwrites > 0) {
        fprintf(stderr, "Underwrote %d times by a total of %d bytes.\n",
                stats.underwrites, stats.underwritten_bytes);
    }
}


void interval(int signal) {
    should_report = 1;
}


void cleanup(int signal) {
    fprintf(stderr, "\nGot signal %d, aborting early.\n", signal);
    print_final_report();
    exit(signal);
}
