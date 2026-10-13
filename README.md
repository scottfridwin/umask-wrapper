# umask-wrapper

[![Build](https://img.shields.io/github/actions/workflow/status/scottfridwin/umask-wrapper/build.yml?branch=main&label=build)](https://github.com/scottfridwin/umask-wrapper/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/scottfridwin/umask-wrapper)](https://github.com/scottfridwin/umask-wrapper/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/scottfridwin/umask-wrapper/total)](https://github.com/scottfridwin/umask-wrapper/releases)
[![License](https://img.shields.io/github/license/scottfridwin/umask-wrapper)](LICENSE)

A tiny static binary that sets the [umask](https://man7.org/linux/man-pages/man2/umask.2.html) from the `APP_UMASK` environment variable and then runs your command. It is meant for containers whose image has no shell or no built-in umask setting (scratch, distroless and many hardened images), so you can control the permissions of the files they create on shared volumes.

It can also load environment variables from files (`FILE__<NAME>=<path>`), so images that only read secrets from environment variables can use [Docker secrets](https://docs.docker.com/compose/how-tos/use-secrets/) instead of having the value in the compose file or `docker inspect`.

> [!NOTE]
> **AI disclosure:** This project is built and maintained with substantial help from AI coding assistants (GitHub Copilot). AI is used to write and modify the code, tests, documentation and CI configuration, and to manage the repository. Dependency updates are merged and released automatically, without human review, when the automated tests pass. Review the code and test it in your own environment before relying on it.

## Features

- **No dependencies** — one statically linked file (~10–70 KB); works in `FROM scratch` images
- **Transparent** — replaces itself with your command (`exec`), so signals, PID 1, arguments, environment and exit codes are untouched
- **Fails safe** — an invalid `APP_UMASK` stops the container instead of silently creating world-writable files
- **Multi-architecture** — `x86_64`, `arm64`, `armv7` and `i386`
- **Verifiable** — every release has SHA-256 checksums and a signed build provenance attestation

## Download

Pick the file for your architecture from the [latest release](https://github.com/scottfridwin/umask-wrapper/releases/latest):

| Architecture | `uname -m` | File |
| --- | --- | --- |
| 64-bit Intel/AMD | `x86_64` | `umask-wrapper.x86_64` |
| 64-bit ARM (e.g. Raspberry Pi 4/5) | `aarch64` | `umask-wrapper.arm64` |
| 32-bit ARM | `armv7l` | `umask-wrapper.armv7` |
| 32-bit Intel | `i386` / `i686` | `umask-wrapper.i386` |

```bash
curl -fsSLo umask-wrapper https://github.com/scottfridwin/umask-wrapper/releases/latest/download/umask-wrapper.x86_64
chmod +x umask-wrapper
```

To verify the download, compare it with `SHA256SUMS` from the same release, or check its provenance with the GitHub CLI:

```bash
gh attestation verify umask-wrapper --repo scottfridwin/umask-wrapper
```

## Usage

```bash
APP_UMASK=0002 ./umask-wrapper <command> [args...]
```

| `APP_UMASK` | Result |
| --- | --- |
| not set | The command runs with the umask it inherited |
| 1–4 octal digits, up to `0777` (e.g. `0002`, `022`, `0`) | The umask is set, then the command runs |
| anything else, including an empty value | Error message, exit code 125; the command is **not** run |

Exit codes follow the same convention as `env`:

| Exit code | Meaning |
| --- | --- |
| 125 | umask-wrapper error (no command given, invalid `APP_UMASK`, a `FILE__` variable that can't be loaded) |
| 126 | The command was found but could not be run (e.g. not executable) |
| 127 | The command was not found |
| anything else | The command's own exit code |

### Docker Compose

Mount the binary into the container, make it the entrypoint, and pass the image's original entrypoint and command as the `command`:

```yaml
services:
  app:
    image: example/app:1.2.3
    entrypoint: ["/umask-wrapper"]
    command: ["/usr/local/bin/entrypoint.sh"]  # the image's original ENTRYPOINT + CMD
    environment:
      - APP_UMASK=0002  # files 664, directories 775
    volumes:
      - ./umask-wrapper:/umask-wrapper:ro
```

Find an image's original entrypoint and command with:

```bash
docker image inspect example/app:1.2.3 --format '{{json .Config.Entrypoint}} {{json .Config.Cmd}}'
```

Common values: `0022` (default on most systems; files `644`), `0002` (group-writable; files `664`), `0077` (private; files `600`).

### Variables from files

For every environment variable `FILE__<NAME>=<path>`, umask-wrapper sets `NAME` to the contents of the file and removes `FILE__<NAME>` before running the command:

```yaml
services:
  app:
    image: example/app:1.2.3
    entrypoint: ["/umask-wrapper"]
    command: ["/usr/local/bin/entrypoint.sh"]
    environment:
      - FILE__DB_PASS=/run/secrets/db_password  # the app sees DB_PASS
    secrets:
      - db_password
    volumes:
      - ./umask-wrapper:/umask-wrapper:ro
```

- Trailing newlines (`\n`, `\r\n`) are removed; everything else, including inner newlines, is kept. An empty file sets an empty value. An existing `NAME` is replaced.
- `NAME` must be a valid variable name (`[A-Za-z_][A-Za-z0-9_]*`).
- A file that can't be read, is larger than 64 KiB or contains a NUL byte stops the container with exit code 125; the error names the variable, never the file's contents.
- `APP_UMASK` is optional; use either feature on its own.
- The value is only in the command's process environment (`/proc/<pid>/environ`, readable by the same user), not in the container's configuration.

## Further reading

- [Development](docs/development.md)

## License

[MIT](LICENSE)
