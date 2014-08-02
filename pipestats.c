
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <stdint.h>


#include "units.h"


#define BUF_SIZE (4092)


typedef struct Stats {
    unsigned long int total_bytes;
    unsigned int bytes_since;

    struct timeval last_report;
    struct timeval start;
} Stats;
Stats stats;


typedef struct Options {
    double freq;
    Unit unit;
    int blocking;
    int verbose;
} Options;
Options options;


static volatile sig_atomic_t done = 0;


double elapsed_sec(struct timeval* end, struct timeval* start);


int read_options();
int setup();


void check_read_errors();
int check_write_errors();


void print_report();
void print_final_report();


void cleanup(int signal);


int main(int argc, char** argv) {
    size_t buff_offset = 0;
    int err = 0;
    int bytes_read = 0;
    struct timeval report_interval;
    char buff[BUF_SIZE];

    done = 0;

    if ((err = read_options(argc, argv)) != 0) {
        return err;
    }

    if ((err = setup(&report_interval)) != 0) {
        return err;
    }

    while (bytes_read > 0 || !done) {
        print_report();

        // Only read more if we've already written everything we already had.
        if (bytes_read == 0) {
            fd_set set;

            // Use select so we don't wait for longer than report interval, but
            // we do wait for some input at least that long (as opposed to just
            // plain non-blocking io).
            FD_ZERO(&set);
            FD_SET(STDIN_FILENO, &set);
            if (select(FD_SETSIZE, &set, NULL, NULL, &report_interval) > 0) {
                bytes_read = fread(buff, 1, BUF_SIZE, stdin);

                if (bytes_read == 0 && ferror(stdin) != 0) {
                    switch (errno) {
                    case EINTR:
                    case EBUSY:
                    case EDEADLK:
                    case EAGAIN:
                    case ETXTBSY:
                        // That's fine, keep going.
                        break;

                    default:
                        fprintf(stderr, "Got err %d during a read: %s\n",
                                errno, strerror(errno));
                        done = 1;
                        err = errno;
                        break;
                    }
                }
            }
        }

        if (bytes_read > 0) {
            fd_set set;

            FD_ZERO(&set);
            FD_SET(STDOUT_FILENO, &set);
            if (select(FD_SETSIZE, NULL, &set, NULL, &report_interval) > 0) {
                int bytes_written = fwrite(buff + buff_offset, 1, bytes_read, stdout);

                stats.total_bytes += bytes_written;
                stats.bytes_since += bytes_written;

                if (bytes_written == 0 && ferror(stdout) != 0) {
                    switch (errno) {
                    case EINTR:
                    case EBUSY:
                    case EDEADLK:
                    case EAGAIN:
                    case ETXTBSY:
                        break;

                    default:
                        // Can't write, so there's no point in continuing.
                        fprintf(stderr,
                                "Got err %d during a write: %s\n"
                                "Exiting with %d bytes still in buffer.\n",
                                errno, strerror(errno), bytes_read);
                        bytes_read = 0;
                        done = 1;
                        err = errno;
                        break;
                    }
                }

                if (bytes_written == bytes_read) {
                    bytes_read = 0;
                    buff_offset = 0;
                } else if (bytes_written > 0) {
                    bytes_read -= bytes_written;
                    buff_offset += bytes_written;
                }
            }
        } else {
            // Make sure not to clear a done flag from some other reason.
            done = done || feof(stdin);

            // Good time to proactively ask for a flush, cuz maybe there isn't
            // much happening (since we haven't read anything). But don't
            // bother trying multiple times if it fails, it's just kinda a
            // nice time.
            fflush(stdout);
        }
    }

    // Just for timing niceness, flush buffers before printing report.
    done = 0;
    while (!done && fflush(stdout) != 0) {
        switch (errno) {
        case EINTR:
        case EBUSY:
        case EDEADLK:
        case EAGAIN:
        case ETXTBSY:
            break;

        default:
            fprintf(stderr, "Failed to flush stdout, err %d: %s\n",
                    errno, strerror(errno));
            err = errno;
            done = 1;
            break;
        }
    }

    print_final_report();

    return err;
}


int read_options(int argc, char** argv) {
    int opt = 0;
    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
        {"human", no_argument, NULL, 'H'},
        {"freq", required_argument, NULL, 'f'},
        {"blocking-io", no_argument, NULL, 'b'},
        {0, 0, 0, 0}
    };

    // Init with defaults.
    options.freq = 2.0;
    options.unit = Human;
    options.blocking = 0;
    options.verbose = 0;

    while (opt != -1) {
        int option_index = 0;

        opt = getopt_long(argc, argv, "hvHBKMGf:bV", long_options, &option_index);
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

        case 'v':
            options.verbose = 1;
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

        case 'b':
            options.blocking = 1;
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


int setup(struct timeval* report_interval) {
    struct sigaction cleanup_action;
    double half_freq;
    int abort_signals[] = {SIGHUP, SIGINT, SIGQUIT, SIGABRT, SIGPIPE, SIGTERM};
    int i;
    int err;

    // Put stdin/stdout into non-blocking mode, so even if there's less than
    // buffer size of data, we clean that out and report stats on it.
    if (!options.blocking && fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1) {
        fprintf(stderr,
                "Warning: failed to put stdin in nonblocking mode. "
                "Reporting might not be consistently on time.\n");
    }
    if (!options.blocking && fcntl(STDOUT_FILENO, F_SETFL, O_NONBLOCK) == -1) {
        fprintf(stderr,
                "Warning: failed to put stdout in nonblocking mode. "
                "Reporting might not be consistently on time.\n");
    }

    if ((err = ferror(stdin)) != 0) {
        fprintf(stderr,
                "stdin is in error state %d: %s\nclearing and continuing\n",
                err, strerror(err));
        clearerr(stdin);
    }
    if ((err = ferror(stdout)) != 0) {
        fprintf(stderr,
                "stdout is in error state %d: %s\nclearing and continuing\n",
                err, strerror(err));
        clearerr(stdout);
    }

    // Timing for report.
    half_freq = options.freq / 2.0;
    report_interval->tv_sec = (int) half_freq;
    report_interval->tv_usec =
        (half_freq - ((int) half_freq)) * (1000 * 1000);

    // Init stats.
    memset(&stats, 0, sizeof(Stats));
    gettimeofday(&stats.start, NULL);
    stats.last_report = stats.start;

    // Set up handler for exiting, to print a final report, even if aborted.
    memset(&cleanup_action, 0, sizeof(struct sigaction));
    cleanup_action.sa_handler = &cleanup;
    cleanup_action.sa_flags = SA_RESTART;
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


void print_report() {
    struct timeval now;
    double elapsed;

    if (options.freq <= 0) {
        return;
    }

    gettimeofday(&now, NULL);
    elapsed = elapsed_sec(&now, &stats.last_report);

    if (elapsed >= options.freq) {
        double data_amount_since = adjust_unit(stats.bytes_since, options.unit);
        const char* data_amount_since_unit = unit_name(stats.bytes_since, options.unit);

        double data_amount_total = adjust_unit(stats.total_bytes, options.unit);
        const char* data_amount_total_unit = unit_name(stats.total_bytes, options.unit);

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

}


void cleanup(int signal) {
    fprintf(stderr, "\nGot signal %s, aborting early.\n",
            strsignal(signal));

    if (done) {
        // Already supposed to be done, just abort.
        abort();
    }

    done = 1;
}
