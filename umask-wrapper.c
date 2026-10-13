/*
 * umask-wrapper: load variables from files, set the umask from APP_UMASK, then exec a command.
 *
 * Usage: [FILE__<NAME>=<path>...] APP_UMASK=<octal> umask-wrapper <command> [args...]
 *
 * FILE__<NAME>=<path> -> NAME is set to the file's contents (trailing CR/LF removed) and
 *                        FILE__<NAME> is removed; an unreadable, oversized or binary file,
 *                        or an invalid NAME, is an error.
 * APP_UMASK unset   -> the inherited umask is kept.
 * APP_UMASK invalid -> error, exit 125, the command is not run.
 * Exit codes follow env(1): 125 wrapper error, 126 command not executable,
 * 127 command not found; otherwise the command's own exit code.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define EXIT_WRAPPER_ERROR 125
#define EXIT_CANNOT_EXECUTE 126
#define EXIT_NOT_FOUND 127

#define FILE_PREFIX "FILE__"
#define FILE_PREFIX_LEN (sizeof(FILE_PREFIX) - 1)
#define MAX_FILE_SIZE 65536

extern char **environ;

static void put(const char *text) {
    ssize_t written = write(STDERR_FILENO, text, strlen(text));
    (void)written; /* nothing useful to do if stderr is unavailable */
}

/* Prints "umask-wrapper: <a><b><c>\n"; b and c may be NULL. */
static void error(const char *a, const char *b, const char *c) {
    put("umask-wrapper: ");
    put(a);
    if (b) {
        put(b);
    }
    if (c) {
        put(c);
    }
    put("\n");
}

/* Accepts 1-4 octal digits with a value of at most 0777. */
static int parse_umask(const char *text, mode_t *out) {
    size_t len = strlen(text);
    mode_t value = 0;

    if (len == 0 || len > 4) {
        return -1;
    }
    for (size_t i = 0; i < len; i++) {
        if (text[i] < '0' || text[i] > '7') {
            return -1;
        }
        value = (mode_t)(value * 8 + (mode_t)(text[i] - '0'));
    }
    if (value > 0777) {
        return -1;
    }
    *out = value;
    return 0;
}

static int valid_name(const char *name) {
    if (!(*name == '_' || (*name >= 'A' && *name <= 'Z') || (*name >= 'a' && *name <= 'z'))) {
        return 0;
    }
    for (const char *p = name + 1; *p; p++) {
        if (!(*p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9'))) {
            return 0;
        }
    }
    return 1;
}

/* Reads path into buf (NUL-terminated, trailing CR/LF removed). Returns 0 or an error message. */
static const char *read_value(const char *path, char *buf) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return strerror(errno);
    }
    size_t len = 0;
    for (;;) {
        ssize_t n = read(fd, buf + len, MAX_FILE_SIZE + 1 - len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            const char *msg = strerror(errno);
            close(fd);
            return msg;
        }
        if (n == 0) {
            break;
        }
        len += (size_t)n;
        if (len > MAX_FILE_SIZE) {
            close(fd);
            return "file is larger than 64 KiB";
        }
    }
    close(fd);
    if (memchr(buf, '\0', len)) {
        return "file contains a NUL byte";
    }
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        len--;
    }
    buf[len] = '\0';
    return NULL;
}

/* For each FILE__<NAME>=<path>: set NAME from the file, then unset FILE__<NAME>. */
static int load_file_vars(void) {
    static char buf[MAX_FILE_SIZE + 2];
    size_t count = 0;

    for (char **e = environ; *e; e++) {
        if (strncmp(*e, FILE_PREFIX, FILE_PREFIX_LEN) == 0) {
            count++;
        }
    }
    if (count == 0) {
        return 0;
    }
    /* Copy first: setenv/unsetenv change environ while we work through it */
    char **entries = calloc(count, sizeof(*entries));
    if (!entries) {
        error("out of memory", NULL, NULL);
        return -1;
    }
    size_t n = 0;
    for (char **e = environ; *e && n < count; e++) {
        if (strncmp(*e, FILE_PREFIX, FILE_PREFIX_LEN) == 0) {
            entries[n] = strdup(*e);
            if (!entries[n]) {
                error("out of memory", NULL, NULL);
                return -1;
            }
            n++;
        }
    }

    for (size_t i = 0; i < n; i++) {
        char *eq = strchr(entries[i], '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        const char *name = entries[i] + FILE_PREFIX_LEN;
        const char *path = eq + 1;
        if (!valid_name(name)) {
            error("invalid variable name: ", entries[i], NULL);
            return -1;
        }
        const char *msg = read_value(path, buf);
        if (msg) {
            error(entries[i], ": ", msg);
            return -1;
        }
        if (setenv(name, buf, 1) != 0 || unsetenv(entries[i]) != 0) {
            error("cannot set ", name, NULL);
            return -1;
        }
    }
    for (size_t i = 0; i < n; i++) {
        free(entries[i]);
    }
    free(entries);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        error("usage: [FILE__<NAME>=<path>] APP_UMASK=<octal> umask-wrapper <command> [args...]", NULL, NULL);
        return EXIT_WRAPPER_ERROR;
    }

    if (load_file_vars() != 0) {
        return EXIT_WRAPPER_ERROR;
    }

    const char *env_umask = getenv("APP_UMASK");
    if (env_umask) {
        mode_t mask;
        if (parse_umask(env_umask, &mask) != 0) {
            error("invalid APP_UMASK (expected octal 0-0777): ", env_umask, NULL);
            return EXIT_WRAPPER_ERROR;
        }
        umask(mask);
    }

    execvp(argv[1], &argv[1]);

    int err = errno;
    put("umask-wrapper: cannot run ");
    put(argv[1]);
    put(": ");
    put(strerror(err));
    put("\n");
    return err == ENOENT ? EXIT_NOT_FOUND : EXIT_CANNOT_EXECUTE;
}
