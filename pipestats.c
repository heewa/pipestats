
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

#define BUF_SIZE (1024)
#define MAX_ERR_CODE (256)

typedef struct Stats {
    unsigned long int total_bytes;
    unsigned int bytes_since;

    // In case they're different, track both.
    unsigned long int bytes_read;

    struct timeval last_report;
    struct timeval start;

    unsigned int num_errors;

    unsigned int underwrites;
    unsigned int underwritten_bytes;
    unsigned int interrupted_writes;

    unsigned int interrupted_reads;

    int num_write_errors_by_code[MAX_ERR_CODE];
    int num_read_errors_by_code[MAX_ERR_CODE];
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
    int blocking;
    int ignore_errors;
    int verbose;
} Options;
Options options;


static volatile sig_atomic_t should_report = 0;
static volatile sig_atomic_t done = 0;


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
    int bytes_read = 0;

    done = 0;

    if ((r = read_options(argc, argv)) != 0) {
        return r;
    }

    if ((r = setup()) != 0) {
        return r;
    }

    while (bytes_read > 0 || !done) {
        if (should_report) {
            // Report first, then clear flag, so we don't bother reporting
            // back-to-back in overlap scenarios.
            print_report();
            should_report = 0;
        }

        // Only read more if we've already written everything we already had.
        if (bytes_read == 0) {
            /*
            */
            bytes_read = fread(buff, 1, BUF_SIZE, stdin);
            stats.bytes_read += bytes_read;

            if (ferror(stdin) == 0) {
                // cool

            /*
            int data;
            while (bytes_read <= BUF_SIZE && (data = getc(stdin)) != EOF) {
                buff[bytes_read++] = data;
            }
            stats.bytes_read += bytes_read;

            // Check if we just read enough, or it's the end of input, or
            // there was an error.
            if (data != EOF || (errno == 0 && ferror(stdin) == 0)) {
                done = (data == EOF);
            */
            } else {
                ++stats.num_read_errors_by_code[(errno <= MAX_ERR_CODE ? errno : MAX_ERR_CODE)];

                // Use a switch statement so it doesn't check each of many
                // conditions that we can safely ignore.
                switch (errno) {
                case EINTR:
                case EBUSY:
                case EDEADLK:
                case EAGAIN:
                case ETXTBSY:
                    ++stats.interrupted_reads;
                    break;

                case EPIPE:
                    // Inform user, but nobody's writing anymore, so there's
                    // nothing left to read. Don't abort right away, finish
                    // writing the rest.
                    if (!options.ignore_errors) {
                        fprintf(stderr,
                                "Broken pipe on stdin, will finish writing "
                                "and exit.\n");
                    }
                    done = 1;
                    break;

                default:
                    ++stats.num_errors;
                    if (!options.ignore_errors) {
                        fprintf(stderr, "Got err %d during a read: %s\n",
                                errno, strerror(errno));
                        abort();
                    }
                    break;
                }
            }
        } else if (bytes_read < 0) {
            if (!options.ignore_errors) {
                fprintf(stderr, "Somehow got to negative bytes read: %d\n",
                        bytes_read);
                abort();
            }
            bytes_read = 0;
        }

        if (bytes_read > 0) {
            int bytes_written;

            // But first update counters, cuz we might report white this is
            // going out to the buffer? Is that sound logic? woof!
            bytes_written = fwrite(buff + buff_offset, 1, bytes_read, stdout);

            // Sigh, check some stupid scenarios.
            if (bytes_read < 0) {
                fprintf(stderr, "Read negative bytes? %d\n", bytes_read);
                if (!options.ignore_errors) {
                    abort();
                }
            } else if (bytes_written < 0) {
                fprintf(stderr, "Wrote negative bytes? %d\n", bytes_written);
                if (!options.ignore_errors) {
                    abort();
                }
            } else if (bytes_written > bytes_read) {
                fprintf(stderr, "Wrote more (%d b) that read (%d b)?\n",
                        bytes_written, bytes_read);
                if (!options.ignore_errors) {
                    abort();
                }
            }

            // Regardless of write result, check for errors.
            if (ferror(stdout) != 0) {
                ++stats.num_write_errors_by_code[(errno <= MAX_ERR_CODE ? errno : MAX_ERR_CODE)];

                // Use a switch statement so it doesn't check each of many
                // conditions that we can safely ignore.
                switch (errno) {
                case EINTR:
                case EBUSY:
                case EDEADLK:
                case EAGAIN:
                case ETXTBSY:
                    ++stats.interrupted_writes;
                    break;

                case EPIPE:
                    // Shit! Nobody's reading, but we already took the data
                    // from stdin. Now it's lost, I guess. Oh well.
                    bytes_read = 0;
                    done = 1;
                    break;

                default:
                    ++stats.num_errors;
                    if (!options.ignore_errors) {
                        fprintf(stderr, "Got err %d during a write: %s\n",
                                errno, strerror(errno));
                        abort();
                    }
                }
            }

            // Cool, error checking out of the way, do some accounting.

            stats.total_bytes += bytes_written;
            stats.bytes_since += bytes_written;

            if (bytes_written == bytes_read) {
                bytes_read = 0;
                buff_offset = 0;
            } else if (bytes_written > 0) {
                bytes_read -= bytes_written;
                buff_offset += bytes_written;

                // Only pay attention to partial writes. When nothing was
                // written, that could be cuz of an error.
                ++stats.underwrites;
                stats.underwritten_bytes += (bytes_read - bytes_written);
            }
        } else {
            // Make sure not to clear a done flag from some other reason.
            done = done || feof(stdin);
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
        {"verbose", no_argument, NULL, 'v'},
        {"human", no_argument, NULL, 'H'},
        {"freq", required_argument, NULL, 'f'},
        {"blocking-io", no_argument, NULL, 'b'},
        {"ignore-errors", no_argument, NULL, 'i'},
        {0, 0, 0, 0}
    };

    // Init with defaults.
    options.freq = 2.0;
    options.unit = Human;
    options.blocking = 0;
    options.ignore_errors = 0;
    options.verbose = 0;

    while (opt != -1) {
        int option_index = 0;

        opt = getopt_long(argc, argv, "hvHBKMGf:b", long_options, &option_index);
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

        case 'i':
            options.ignore_errors = 1;
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
    int i;

    double data_amount = adjust_unit(stats.total_bytes, options.unit);
    const char* data_amount_unit = unit_name(stats.total_bytes, options.unit);

    gettimeofday(&now, NULL);
    elapsed = elapsed_sec(&now, &stats.start);

    fprintf(stderr, "%3.2f %s (%lu bytes) total over %.2f sec, avg %.2f %s/s\n",
            data_amount, data_amount_unit,
            stats.total_bytes,
            elapsed,
            data_amount / elapsed, data_amount_unit);

    if (stats.bytes_read < stats.total_bytes) {
        fprintf(stderr, "\tFailed to write %lu bytes.\n",
                stats.total_bytes - stats.bytes_read);
    } else if (stats.bytes_read < stats.total_bytes) {
        fprintf(stderr, "\tSomehow wrote %lu extra bytes.\n",
                stats.bytes_read - stats.total_bytes);
    }

    if (stats.num_errors > 0) {
        fprintf(stderr, "\tGot %d write errors.\n",
                stats.num_errors);
    }

    if (options.verbose) {
        if (stats.underwrites > 0) {
            fprintf(stderr, "\tUnderwrote %d times by a total of %d bytes.\n",
                    stats.underwrites, stats.underwritten_bytes);
        }

        if (stats.interrupted_writes > 0) {
            fprintf(stderr, "\tGot interrupted during a write %d times.\n",
                    stats.interrupted_writes);
        }
        if (stats.interrupted_reads > 0) {
            fprintf(stderr, "\tGot interrupted during a read %d times.\n",
                    stats.interrupted_reads);
        }

        for (i=0; i < MAX_ERR_CODE; ++i) {
            if (stats.num_write_errors_by_code[i] > 0) {
                fprintf(stderr, "\tGot err %d on a write %d times.\n",
                        i, stats.num_write_errors_by_code[i]);
            }
            if (stats.num_read_errors_by_code[i] > 0) {
                fprintf(stderr, "\tGot err %d on a read %d times.\n",
                        i, stats.num_read_errors_by_code[i]);
            }
        }
    }
}


void interval(int signal) {
    should_report = 1;
}


void cleanup(int signal) {
    fprintf(stderr, "\nGot signal %s, aborting early.\n",
            strsignal(signal));
    done = 1;
}
