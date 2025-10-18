/*
 * umask-wrapper.c
 * A tiny wrapper to optionally set umask before executing any command.
 *
 * Usage:
 *   APP_UMASK=<octal> ./umask-wrapper <command> [args...]
 *   If APP_UMASK is unset, the system default is used.
 */

#include <sys/types.h>   // mode_t
#include <sys/stat.h>    // umask()
#include <unistd.h>      // execvp(), write()
#include <stdlib.h>      // getenv(), strtol()
#include <errno.h>       // errno
#include <string.h>      // strlen(), strerror()

int main(int argc, char **argv) {
    if (argc < 2) {
        const char msg[] = "Usage: umask-wrapper <command> [args...]\n";
        write(2, msg, sizeof(msg)-1);
        return 1;
    }

    // Only set umask if APP_UMASK is defined
    const char *env_umask = getenv("APP_UMASK");
    if (env_umask) {
        mode_t mask = strtol(env_umask, NULL, 8);
        umask(mask);
    }

    // Execute the target command with arguments
    execvp(argv[1], &argv[1]);

    // If execvp returns, an error occurred
    const char prefix[] = "exec failed: ";
    write(2, prefix, sizeof(prefix)-1);
    const char *err = strerror(errno);
    write(2, err, strlen(err));
    write(2, "\n", 1);

    return 1;
}
