
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#define BUF_SIZE (1024)
#define REPORT_FREQ (2.0)

double elapsed_sec(struct timeval* end, struct timeval* start);
void print_report(double elapsed, unsigned int bytes_since, unsigned long int total_bytes);

int main(int argc, char** argv) {
    char buff[BUF_SIZE];
    unsigned long int total_bytes = 0;
    unsigned int bytes_since = 0;
    struct timeval last_report;
    double elapsed;

    // Go into non-blocking mode, so even if there's less than buffer size of
    // data, we clean that out and report stats on it.
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    gettimeofday(&last_report, NULL);

    // Read until there's nothing left.
    while (!feof(stdin)) {
        int bytes_read = 0;
        struct timeval now;

        // Read and write out as close to each other as possible.
        bytes_read = fread(buff, 1, BUF_SIZE, stdin);
        if (bytes_read > 0) {
            total_bytes += bytes_read;
            bytes_since += bytes_read;
            fwrite(buff, 1, bytes_read, stdout);
        }

        gettimeofday(&now, NULL);
        elapsed = elapsed_sec(&now, &last_report);
        if (elapsed >= REPORT_FREQ) {
            print_report(elapsed, bytes_since, total_bytes);
            bytes_since = 0;
            last_report = now;
        }
    }

    if (bytes_since > 0) {
        print_report(elapsed, bytes_since, total_bytes);
    }

    return 0;
}


double elapsed_sec(struct timeval* end, struct timeval* start) {
    double sec = end->tv_sec - start->tv_sec;
    double usec = end->tv_usec - start->tv_usec;
    return sec + usec / (1000 * 1000);
}


void print_report(double elapsed, unsigned int bytes_since, unsigned long int total_bytes) {
    fprintf(stderr,
            "%3.2f bytes per sec, "
            "%lu bytes total, "
            "%d since last report\n",
            bytes_since / elapsed,
            total_bytes,
            bytes_since);
}
