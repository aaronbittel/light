#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    char*   buf;
    size_t  cap;
    size_t len;
} InputBuffer;

bool ib_read_line(InputBuffer* ib) {
    size_t bytes_read = getline(&(ib->buf), &(ib->cap), stdin);
    if (bytes_read <= 0) {
        perror("getline()");
        return false;
    }

    ib->len = (size_t)bytes_read - 1;
    assert(ib->buf[ib->len] == '\n');
    ib->buf[ib->len] = '\0';

    return true;
}

bool ib_equals(InputBuffer ib, char* text) {
    return strcmp(ib.buf, text) == 0;
}

void ib_free(InputBuffer* ib) {
    free(ib->buf);
}

int main() {
    InputBuffer ib = {0};

    while (true) {
        printf("db > ");

        if (!ib_read_line(&ib)) {
            ib_free(&ib);
            fprintf(stderr, "ERROR: reading line\n");
            return 1;
        }

        if (ib_equals(ib, ".exit")) {
            break;
        } else {
            printf("error: invalid command '%s'\n", ib.buf);
        }
    }

    ib_free(&ib);
    return 0;
}
