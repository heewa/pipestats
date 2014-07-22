#include <stdio.h>
#include <stdlib.h>

#define BUFF_SIZE (8192)


#define PICK_CHAR(choices, num) ( choices[num % (sizeof(choices) / sizeof(choices[0]))] )


int main(int argc, char** argv) {
    int chunk_size;
    int num_chunks;
    int chunk_num;
    int num_nums_in_chunk;
    int leftover_in_chunk;
    static const char prefixes[] = {'<', '{', '('};
    static const char suffixes[] = {'>', '}', ')'};
    static const char extras[] = {'-', '=', '~', '#', '$'};

    if (argc != 3) {
        fprintf(stderr, "Run with 2 arguments: [chunk size] [number of chunks]\n");
        return 1;
    }

    chunk_size = atoi(argv[1]);
    if (chunk_size < sizeof(int) + 2 || chunk_size > BUFF_SIZE) {
        fprintf(stderr, "Chunk size must be between %d and %d.\n",
                (int) sizeof(int) + 2, BUFF_SIZE);
        return 1;
    }
    num_nums_in_chunk = (int) ((chunk_size - 2) / sizeof(int));
    leftover_in_chunk = (chunk_size - 2) % sizeof(int);

    num_chunks = atoi(argv[2]);
    if (num_chunks <= 0) {
        fprintf(stderr, "Number of chunks must be at least 1.\n");
        return 1;
    }

    for (chunk_num=0; chunk_num < num_chunks; ++chunk_num) {
        char chunk[BUFF_SIZE];
        int i;

        // A chunk is bookended by <>, and in the middle, as many chunk_nums
        // as can fit, as integers, and leftover space is filled with different
        // things every chunk (so you can see if the same chunk's repeated on
        // partial writes).
        chunk[0] = PICK_CHAR(prefixes, chunk_num);
        for (i=0; i < num_nums_in_chunk; ++i) {
            *((int*) (chunk + 1 + sizeof(int) * i)) = chunk_num;
        }
        for (i=1 + sizeof(int) * num_nums_in_chunk; i < chunk_size - 1; ++i) {
            chunk[i] = PICK_CHAR(extras, chunk_num);
        }
        chunk[chunk_size - 1] = PICK_CHAR(suffixes, chunk_num);

        fwrite(chunk, 1, chunk_size, stdout);
    }

    return 0;
}
