/*
 * umask-wrapper: set the umask from APP_UMASK, then exec a command.
 *
 * Usage: APP_UMASK=<octal> umask-wrapper <command> [args...]
 *
 * APP_UMASK unset   -> the inherited umask is kept.
 * APP_UMASK invalid -> error, exit 125, the command is not run.
 * Exit codes follow env(1): 125 wrapper error, 126 command not executable,
 * 127 command not found; otherwise the command's own exit code.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define EXIT_WRAPPER_ERROR 125
#define EXIT_CANNOT_EXECUTE 126
#define EXIT_NOT_FOUND 127

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

int main(int argc, char **argv) {
    if (argc < 2) {
        error("usage: APP_UMASK=<octal> umask-wrapper <command> [args...]", NULL, NULL);
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
