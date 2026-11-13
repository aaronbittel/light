#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    char*   buf;
    size_t  cap;
    size_t  len;
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

void ib_free(InputBuffer* ib) {
    free(ib->buf);
}

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

typedef struct {
    StatementType type;
} Statement;

bool prepare_statement(Statement* stmt, InputBuffer ib) {
    if (strncmp(ib.buf, "insert", 6) == 0) {
        stmt->type = STATEMENT_INSERT;
        return true;
    }
    if (strncmp(ib.buf, "select", 6) == 0){
        stmt->type = STATEMENT_SELECT;
        return true;
    }
    return false;
}

void execute_statement(Statement stmt) {
    switch (stmt.type) {
        case STATEMENT_INSERT:
            printf("handling 'insert' command...\n");
            break;
        case STATEMENT_SELECT:
            printf("handling 'select' command...\n");
            break;
    }
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

        if (ib.buf[0] == '.') {
            // meta command
            if (strcmp(ib.buf, ".exit") == 0) {
                break;
            } else {
                printf("error: invalid meta command '%s'\n", ib.buf);
            }
            continue;
        }

        Statement stmt = {0};
        if (prepare_statement(&stmt, ib)) {
            execute_statement(stmt);
            printf("Executed.\n");
        } else {
            printf("error: unknown command '%s'\n", ib.buf);
        }
    }

    ib_free(&ib);
    return 0;
}
