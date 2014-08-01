
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

int generate_data(long long num_nums) {
    uint32_t num;

    for (num=0; num < num_nums; ++num) {
        while (fwrite(&num, sizeof(num), 1, stdout) != 1) {
            switch (errno) {
            case EINTR:
            case EBUSY:
            case EDEADLK:
            case EAGAIN:
            case ETXTBSY:
                break;

            default:
                fprintf(stderr, "Failed to write number %d, err %d: %s\n",
                        num, errno, strerror(errno));
                return -1;
                break;
            }
        }
    }

    fprintf(stderr, "Wrote %d sequential numbers.\n", num);
    return 0;
}

int verify_data(long long num_nums) {
    uint32_t num;

    for (num=0; num < num_nums; ++num) {
        uint32_t input;

        while (fread(&input, sizeof(input), 1, stdin) != 1) {
            if (feof(stdin) != 0) {
                fprintf(stderr, "Ran out of input prematurely, after %d numbers.\n", num);
                return -1;
            }

            switch (errno) {
            case EINTR:
            case EBUSY:
            case EDEADLK:
            case EAGAIN:
            case ETXTBSY:
                break;

            default:
                fprintf(stderr, "Failed to read byte %d, err %d: %s\n",
                        num, errno, strerror(errno));
                return -1;
                break;
            }
        }

        if (input != num) {
            fprintf(stderr, "Expected byte %d, read %d.\n",
                    num, input);
            return -1;
        }
    }

    fprintf(stderr, "Read %d sequentail numbers.\n", num);

    return 0;
}

void print_usage(char** argv) {
    fprintf(stderr,
            "usage: %s [read|write] [num]\n"
            "\n"
            "    read  - read [num] 4-byte ints and verify they're sequential\n"
            "    write - write [num] sequential 4-byte ints, starting from 0\n"
            "    num   - how many to read/write, between 1 & %d\n",
            argv[0],
            INT32_MAX);
}

int main(int argc, char** argv) {
    long long num_nums;

    if (argc != 3) {
        print_usage(argv);
        return -1;
    }

    num_nums = strtol(argv[2], NULL, 10);
    if (num_nums <= 0) {
        print_usage(argv);
        return -1;
    }

    if (strcmp(argv[1], "read") == 0) {
        return verify_data(num_nums);
    } else if (strcmp(argv[1], "write") == 0) {
        return generate_data(num_nums);
    } else {
        print_usage(argv);
    }

    return -1;
}
