#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define u32 uint32_t

typedef struct {
    char*   buf;
    size_t  cap;
    size_t  len;
} InputBuffer;

bool ib_read_line(InputBuffer* ib) {
    ssize_t bytes_read = getline(&(ib->buf), &(ib->cap), stdin);
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

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 256

typedef struct {
    u32  id;
    char username[COLUMN_USERNAME_SIZE+1];
    char email[COLUMN_EMAIL_SIZE+1];
} Row;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

const u32 ID_SIZE = size_of_attribute(Row, id);
const u32 USERNAME_SIZE = size_of_attribute(Row, username);
const u32 EMAIL_SIZE = size_of_attribute(Row, email);
const u32 ID_OFFSET = 0;
const u32 USERNAME_OFFSET = ID_SIZE;
const u32 EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const u32 ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

typedef struct {
    StatementType type;
    Row row_to_insert;
} Statement;

const u32 PAGE_SIZE = 4096;
#define TABLE_MAX_PAGES 100
const u32 ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const u32 TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

typedef struct {
    void* pages[TABLE_MAX_PAGES];
} Pager;

typedef struct {
    u32 num_rows;
    Pager* pager;
} Table;

Table* table_init() {
    Table* table = calloc(1, sizeof(Table));
    if (table == NULL) {
        perror("calloc()");
        return NULL;
    }

    table->pager = calloc(1, sizeof(Pager));

    return table;
}

void table_free(Table* table) {
    for (size_t i = 0; i < TABLE_MAX_PAGES; i++) {
        free(table->pager->pages[i]);
    }
    free(table);
}

void serialize_row(Row* src, void* dst) {
    memcpy((char*)dst+ID_OFFSET,       &(src->id),       ID_SIZE);
    memcpy((char*)dst+USERNAME_OFFSET, &(src->username), USERNAME_SIZE);
    memcpy((char*)dst+EMAIL_OFFSET,    &(src->email),    EMAIL_SIZE);
}

void deserialize_row(Row* dst, void* src) {
    memcpy(&(dst->id),       (char*)src+ID_OFFSET,       ID_SIZE);
    memcpy(&(dst->username), (char*)src+USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(dst->email),    (char*)src+EMAIL_OFFSET,    EMAIL_SIZE);
}

typedef enum {
    PREPARE_SUCCESS,
    PREPARE_STRING_TOO_LONG,
    PREPARE_INVALID_NUMBER,
    PREPARE_NEGATIVE_ID,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNREGONIZED_COMMAND
} PrepareResult;

PrepareResult prepare_statement(Statement* stmt, InputBuffer ib) {
    while (ib.len > 0 && *ib.buf == ' ') {
        ib.buf += 1;
        ib.len -= 1;
    }

    if (strncmp(ib.buf, "insert", 6) == 0) {
        assert(strtok(ib.buf, " ") != NULL); // unreachable

        char* idtok = strtok(NULL, " ");
        if (idtok == NULL) {
            return PREPARE_SYNTAX_ERROR;
        }

        char* endptr;
        errno = 0;
        long id = strtol(idtok, &endptr, 10);
        if (*endptr != '\0' || errno == ERANGE) {
            return PREPARE_INVALID_NUMBER;
        }
        if (id < 0) {
            return PREPARE_NEGATIVE_ID;
        }
        if (id > UINT32_MAX) {
            return PREPARE_INVALID_NUMBER;
        }
        stmt->row_to_insert.id = (u32)id;

        char* usernametok = strtok(NULL, " ");
        if (usernametok == NULL) {
            return PREPARE_SYNTAX_ERROR;
        }

        if (strlen(usernametok) > COLUMN_USERNAME_SIZE) {
            return PREPARE_STRING_TOO_LONG;
        }
        strncpy(stmt->row_to_insert.username, usernametok, strlen(usernametok));

        char* emailtok = strtok(NULL, " ");
        if (emailtok == NULL) {
            return PREPARE_SYNTAX_ERROR;
        }

        if (strlen(emailtok) > COLUMN_EMAIL_SIZE) {
            return PREPARE_STRING_TOO_LONG;
        }
        strncpy(stmt->row_to_insert.email, emailtok, strlen(emailtok));

        return PREPARE_SUCCESS;
    }


    if (strncmp(ib.buf, "select", 6) == 0){
        stmt->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    return PREPARE_UNREGONIZED_COMMAND;
}

void* row_slot(Table* table, u32 row_num) {
    size_t page_num = row_num / ROWS_PER_PAGE;
    size_t page_index = row_num % ROWS_PER_PAGE;

    if (table->pager->pages[page_num] == NULL) {
        table->pager->pages[page_num] = malloc(PAGE_SIZE);
    }

    return (char*)table->pager->pages[page_num] + page_index * ROW_SIZE;
}

void row_print(Row row) {
    printf("Row: id=%d, username=%s, email=%s\n", row.id, row.username, row.email);
}

typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
} ExecuteResult;

ExecuteResult execute_insert(Statement stmt, Table* table) {
    if (table->num_rows == TABLE_MAX_ROWS) {
        return EXECUTE_TABLE_FULL;
    }

    serialize_row(&stmt.row_to_insert, row_slot(table, table->num_rows));
    table->num_rows++;
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Table* table) {
    if (table->num_rows == 0) {
        printf("Empty table.\n");
        return EXECUTE_SUCCESS;
    }

    printf("Rows:\n");
    Row row = {0};
    for (u32 i = 0; i < table->num_rows; ++i) {
        deserialize_row(&row, row_slot(table, i));
        printf("  "); row_print(row);
    }

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement stmt, Table* table) {
    switch (stmt.type) {
        case STATEMENT_INSERT: return execute_insert(stmt, table);
        case STATEMENT_SELECT: return execute_select(table);
    }

    assert(false && "unreachable");
}

int main() {
    Table* table = table_init();
    if (table == NULL) {
        fprintf(stderr, "ERROR: could not initialize table\n");
        return 1;
    }

    InputBuffer ib = {0};
    int exit_code = EXIT_SUCCESS;

    while (true) {
        printf("db > ");

        if (!ib_read_line(&ib)) {
            ib_free(&ib);
            fprintf(stderr, "ERROR: reading line\n");
            exit_code = EXIT_FAILURE;
            break;
        }

        if (ib.buf[0] == '.') {
            // meta command
            if (strcmp(ib.buf, ".exit") == 0) {
                break;
            } else {
                printf("Unrecognized command '%s'\n", ib.buf);
            }
            continue;
        }

        Statement stmt = {0};
        switch (prepare_statement(&stmt, ib)) {
            case PREPARE_SUCCESS:
                switch (execute_statement(stmt, table)) {
                    case EXECUTE_SUCCESS:
                        printf("Executed.\n");
                        break;
                    case EXECUTE_TABLE_FULL:
                        printf("Error: Table full.\n");
                        break;
                }
                break;
            case PREPARE_STRING_TOO_LONG:
                printf("String is too long.\n");
                break;
            case PREPARE_INVALID_NUMBER:
                printf("Invalid number.\n");
                break;
            case PREPARE_NEGATIVE_ID:
                printf("ID must be positive.\n");
                break;
            case PREPARE_SYNTAX_ERROR:
                printf("Syntax error. Could not parse statement.\n");
                break;
            case PREPARE_UNREGONIZED_COMMAND:
                printf("Unrecognized keyword at start of '%s'.\n", ib.buf);
                break;
        }
    }

    ib_free(&ib);
    table_free(table);
    return exit_code;
}
