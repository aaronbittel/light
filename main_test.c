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

#define CHECK(expr) do {                                                         \
    if (!(expr)) {                                                               \
        fprintf(stderr, "TEST FAILED: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        exit(EXIT_FAILURE);                                                      \
    }                                                                            \
} while (0)

#define SEND(t, fmt, ...) \
    do { \
        char message[1024]; \
        snprintf(message, sizeof(message), fmt, ##__VA_ARGS__); \
        if (!terminal_send(&(t), message)) { \
            fprintf(stderr, "  Failed to send: \"%s\"\n", message); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define EXPECT(t, fmt, ...) \
    do { \
        char expected[1024]; \
        snprintf(expected, sizeof(expected), fmt, ##__VA_ARGS__); \
        if (!terminal_expect(&(t), expected)) { \
            fprintf(stderr, "  Expected: \"%s\"\n", expected); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define RUN_TEST(test) do {                                                 \
    printf("Running %s... \033[31mFAILED\033[0m\n", #test);                  \
    fflush(stdout);                                                         \
                                                                            \
    Terminal terminal = {0};                                                \
    CHECK(terminal_start(&terminal, "./main"));                             \
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

bool terminal_start(Terminal* terminal, const char* program) {
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
        execl(program, program, NULL);
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
                "Expected: \"%.*s\"\n"
                "Received: \"%.*s\"\n",
                (int)expected_len,
                expected,
                (int)terminal->line_buffer.len,
                terminal->line_buffer.line);

        return false;
    }

    return true;
}

int main() {
}
