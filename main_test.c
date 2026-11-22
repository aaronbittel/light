#include <assert.h>
#include <stdio.h>
#include <pty.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 256
#define TEST_DB "test_db.light"

#define CHECK(expr) do {                                                         \
    if (!(expr)) {                                                               \
        fprintf(stderr, "TEST FAILED: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        exit(EXIT_FAILURE);                                                      \
    }                                                                            \
} while (0)

#define SEND(t, fmt, ...)                                           \
    do {                                                            \
        char message[1024];                                         \
        snprintf(message, sizeof(message), fmt, ##__VA_ARGS__);     \
        if (!terminal_send(&(t), message)) {                        \
            fprintf(stderr, "  Failed to send: \"%s\"\n", message); \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while (0)

#define EXPECT(t, fmt, ...)                                       \
    do {                                                          \
        char expected[1024];                                      \
        snprintf(expected, sizeof(expected), fmt, ##__VA_ARGS__); \
        if (!terminal_expect(&(t), expected)) {                   \
            exit(EXIT_FAILURE);                                   \
        }                                                         \
    } while (0)

#define RUN_TEST(test) do {                                                 \
    unlink_test_db();                                                       \
    printf("Running %s... \033[31mFAILED\033[0m\n", #test);                 \
    fflush(stdout);                                                         \
                                                                            \
    Terminal terminal = {0};                                                \
    CHECK(terminal_start(&terminal, "./main", TEST_DB));                    \
                                                                            \
    test(terminal);                                                         \
                                                                            \
    SEND(terminal, ".exit");                                                \
    EXPECT(terminal, "db > ");                                              \
                                                                            \
    int status = terminal_finish(&terminal);                                \
    CHECK(WIFEXITED(status));                                               \
    CHECK(WEXITSTATUS(status) == 0);                                        \
                                                                            \
    printf("\033[1A\r\033[2KRunning %s... \033[32mPASSED\033[0m\n", #test); \
} while (0)

#define RUN_TEST_MANUAL(test) do {                                          \
    unlink_test_db();                                                       \
    printf("Running %s... \033[31mFAILED\033[0m\n", #test);                 \
    fflush(stdout);                                                         \
    test();                                                                 \
    printf("\033[1A\r\033[2KRunning %s... \033[32mPASSED\033[0m\n", #test); \
} while (0)

#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

typedef struct {
    char* line;
    size_t cap;
    size_t len;
} LineBuffer;

typedef struct {
    pid_t pid;
    int fd;
    FILE* out;
    FILE* in;
    LineBuffer line_buffer;
} Terminal;

bool terminal_start(Terminal* terminal, const char* program, const char* filename) {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(tcflag_t)ECHO;
    term.c_iflag &= ~(tcflag_t)(ICRNL | IGNCR);
    term.c_oflag &= ~(tcflag_t)(OPOST);

    int fd;
    pid_t pid = forkpty(&fd, NULL, &term, NULL);
    if (pid == -1) {
        perror("forkpty");
        return false;
    }

    if (pid == 0) {
        execl(program, program, filename, NULL);
        perror("execl");
        _exit(EXIT_FAILURE);
    } else {
        int read_fd = dup(fd);
        int write_fd = dup(fd);

        FILE *in = fdopen(read_fd, "r");
        if (in == NULL) {
            perror("fdopen");
            return false;
        }

        FILE *out = fdopen(write_fd, "w");
        if (out == NULL) {
            perror("fdopen");
            return false;
        }

        terminal->pid = pid;
        terminal->fd = fd;
        terminal->out = out;
        terminal->in = in;
    }

    return true;
}

int terminal_finish(Terminal* terminal) {
    int status;
    if (waitpid(terminal->pid, &status, 0) == -1) {
        fprintf(stderr, "ERROR: could not wait for pid(%d)\n", terminal->pid);
        return -1;
    }

    free(terminal->line_buffer.line);

    fclose(terminal->in);
    fclose(terminal->out);
    close(terminal->fd);

    return status;
}

bool terminal_send(Terminal *terminal, const char* text) {
    if (fprintf(terminal->out, "%s\n", text) < 0) {
        fprintf(stderr, "terminal_send: fprintf failed\n");
        return false;
    }

    if (fflush(terminal->out) == EOF) {
        perror("terminal_send: fflush");
        return false;
    }

    return true;
}

bool terminal_read(Terminal* terminal) {
    ssize_t read_bytes = getline(
            &(terminal->line_buffer.line),
            &(terminal->line_buffer.cap),
            terminal->in);
    if (read_bytes == -1) {
        perror("getline");
        free(terminal->line_buffer.line);
        return false;
    }

    terminal->line_buffer.len = (size_t)read_bytes;
    if (terminal->line_buffer.line[terminal->line_buffer.len-1] == '\n') {
        terminal->line_buffer.len -= 1;
    }

    return true;
}

bool terminal_expect(Terminal* terminal, const char* expected) {
    if (!terminal_read(terminal)) return false;

    size_t expected_len = strlen(expected);
    if (terminal->line_buffer.len != expected_len ||
            strncmp(expected, terminal->line_buffer.line, expected_len) != 0) {
        fprintf(stderr,
                "  Expected: \"%.*s\"\n"
                "  Received: \"%.*s\"\n",
                (int)expected_len,
                expected,
                (int)terminal->line_buffer.len,
                terminal->line_buffer.line);

        return false;
    }

    return true;
}

void unlink_test_db() {
    if (unlink(TEST_DB) == -1 && errno != ENOENT) {
        perror("unlink_test_db: unlink");
    }
}

void test_empty_table(Terminal terminal) {
    SEND(terminal, "select");
    EXPECT(terminal, "db > Empty table.");
    EXPECT(terminal, "Executed.");
}

void test_insert_and_select(Terminal terminal) {
    SEND(terminal, "insert 1 bob bob@example.com");
    EXPECT(terminal, "db > Executed.");

    SEND(terminal, "select");
    EXPECT(terminal, "db > Rows:");
    EXPECT(terminal, "  Row: id=1, username=bob, email=bob@example.com");
    EXPECT(terminal, "Executed.");
}

void test_full_table(Terminal terminal) {
    const int MAX_ROWS = 1301;

    for (int i = 0; i < MAX_ROWS; i++) {
        SEND(terminal, "insert 1 bob bob@example.com");

        if (i < MAX_ROWS - 1) {
            EXPECT(terminal, "db > Executed.");
        } else {
            EXPECT(terminal, "db > Error: Table full.");
        }
    }
}

void test_inserting_strings_with_maximum_length(Terminal terminal) {
    char username_maxlen[COLUMN_USERNAME_SIZE+1] = {0};
    memset(username_maxlen, 'a', COLUMN_USERNAME_SIZE);

    char email_maxlen[COLUMN_EMAIL_SIZE+1] = {0};
    memset(email_maxlen, 'b', COLUMN_EMAIL_SIZE);

    SEND(terminal, "insert 1 %s %s", username_maxlen, email_maxlen);
    EXPECT(terminal, "db > Executed.");

    SEND(terminal, "select");
    EXPECT(terminal, "db > Rows:");
    EXPECT(terminal, "  Row: id=1, username=%s, email=%s", username_maxlen, email_maxlen);
    EXPECT(terminal, "Executed.");
}

void test_error_string_too_long(Terminal terminal) {
    char username_toolong[COLUMN_USERNAME_SIZE+2] = {0};
    memset(username_toolong, 'a', COLUMN_USERNAME_SIZE+1);

    char email_toolong[COLUMN_EMAIL_SIZE+2] = {0};
    memset(email_toolong, 'b', COLUMN_EMAIL_SIZE+1);

    SEND(terminal, "insert 1 %s %s", username_toolong, email_toolong);
    EXPECT(terminal, "db > String is too long.");
}

void test_insert_invalid_syntax(Terminal terminal) {
    const char *inputs[] = {
        "insert",
        "insert 1",
        "insert 1 bob"
    };

    for (size_t i = 0; i < ARRAY_LEN(inputs); i++) {
        SEND(terminal, "%s", inputs[i]);
        EXPECT(terminal, "db > Syntax error. Could not parse statement.");
    }
}

void test_error_invalid_number(Terminal terminal) {
    const char *inputs[] = {
        "a",
        "125sdf",
        "4294967296",            // UINT32_MAX + 1 (storing as u32)
        "9223372036854775808",   // LONG_MAX + 1 (overflow)
        "-9223372036854775809"   // LONG_MIN - 1 (underflow)
    };

    for (size_t i = 0; i < ARRAY_LEN(inputs); i++) {
        SEND(terminal, "insert %s bob bob@example.com", inputs[i]);
        EXPECT(terminal, "db > Invalid number.");
    }

    SEND(terminal, "insert -1 bob bob@example.com");
    EXPECT(terminal, "db > ID must be positive.");
}

void test_id_max_u32(Terminal terminal) {
    SEND(terminal, "insert 4294967295 bob bob@example.com");
    EXPECT(terminal, "db > Executed.");
}

void test_insert_with_spaces(Terminal terminal) {
    SEND(terminal, "    insert   1     bob      bob@example.com");
    EXPECT(terminal, "db > Executed.");
}

void test_keeps_data_after_closing_connection(void) {
    Terminal terminal1 = {0};
    CHECK(terminal_start(&terminal1, "./main", TEST_DB));

    SEND(terminal1, "insert 1 bob bob@example.com");
    EXPECT(terminal1, "db > Executed.");

    SEND(terminal1, ".exit");
    EXPECT(terminal1, "db > ");

    int status1 = terminal_finish(&terminal1);
    CHECK(WIFEXITED(status1));
    CHECK(WEXITSTATUS(status1) == 0);

    Terminal terminal2 = {0};
    CHECK(terminal_start(&terminal2, "./main", TEST_DB));

    SEND(terminal2, "select");
    EXPECT(terminal2, "db > Rows:");
    EXPECT(terminal2, "  Row: id=1, username=bob, email=bob@example.com");
    EXPECT(terminal2, "Executed.");

    SEND(terminal2, ".exit");
    EXPECT(terminal2, "db > ");

    int status2 = terminal_finish(&terminal2);
    CHECK(WIFEXITED(status2));
    CHECK(WEXITSTATUS(status2) == 0);
}

int main() {
    RUN_TEST(test_empty_table);
    RUN_TEST(test_insert_and_select);
    RUN_TEST(test_full_table);
    RUN_TEST(test_inserting_strings_with_maximum_length);
    RUN_TEST(test_insert_invalid_syntax);
    RUN_TEST(test_error_string_too_long);
    RUN_TEST(test_error_invalid_number);
    RUN_TEST(test_id_max_u32);
    RUN_TEST(test_insert_with_spaces);
    RUN_TEST_MANUAL(test_keeps_data_after_closing_connection);
}
