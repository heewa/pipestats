
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <getopt.h>

#define BUF_SIZE (1024)

double elapsed_sec(struct timeval* end, struct timeval* start);

int read_options();
int setup();

void print_report();
void print_final_report();

void interval(int signal);
void cleanup(int signal);


typedef struct Stats {
    unsigned long int total_bytes;
    unsigned int bytes_since;
    struct timeval last_report;
    struct timeval start;
} Stats;
Stats stats;


typedef struct Options {
    double freq;
} Options;
Options options;


int main(int argc, char** argv) {
    char buff[BUF_SIZE];
    int r;
    int done = 0;

    if ((r = read_options(argc, argv)) != 0) {
        return r;
    }

    if ((r = setup()) != 0) {
        return r;
    }

    // Read until there's nothing left.
    while (!done) {
        int bytes_read = 0;

        // Read and write out as close to each other as possible.
        bytes_read = fread(buff, 1, BUF_SIZE, stdin);
        if (bytes_read > 0) {
            // But first update counters, cuz we might report white this is
            // going out to the buffer? Is that sound logic? woof!
            stats.total_bytes += bytes_read;
            stats.bytes_since += bytes_read;
            fwrite(buff, 1, bytes_read, stdout);
        } else if (ferror(stdin) != 0) {
            fprintf(stderr, "Error reading from stdin: %d\n", ferror(stdin));
            done = 1;
        } else {
            done = feof(stdin);
        }
    }

    // Make sure it all went out.
    fflush(stdout);

    print_final_report();

    return 0;
}


int read_options(int argc, char** argv) {
    int opt = 0;
    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"freq", required_argument, NULL, 'f'},
        {0, 0, 0, 0}
    };

    // Init with defaults.
    options.freq = 2.0;

    while (opt != -1) {
        int option_index = 0;

        opt = getopt_long(argc, argv, "hf:", long_options, &option_index);
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


void print_report() {
    struct timeval now;
    double elapsed;

    gettimeofday(&now, NULL);
    elapsed = elapsed_sec(&now, &stats.last_report);

    fprintf(stderr,
            "%3.2f bytes per sec, "
            "%lu bytes total, "
            "%d since last report\n",
            stats.bytes_since / elapsed,
            stats.total_bytes,
            stats.bytes_since);

    stats.bytes_since = 0;
    stats.last_report = now;
}


void print_final_report() {
    struct timeval now;
    double elapsed;

    gettimeofday(&now, NULL);
    elapsed = elapsed_sec(&now, &stats.start);

    fprintf(stderr, "%lu bytes total over %.2f sec, avg %.2f bytes per sec\n",
            stats.total_bytes,
            elapsed,
            stats.total_bytes / elapsed);
}


void interval(int signal) {
    print_report();
}


void cleanup(int signal) {
    fprintf(stderr, "\n");
    print_final_report();
    exit(signal);
}
