# umask-wrapper

A tiny wrapper to set `umask` for any command. Designed for scratch/distroless containers.

## Usage

```bash
# With a specific umask
APP_UMASK=002 ./umask-wrapper <command> [args...]

# Use system default umask (APP_UMASK unset)
./umask-wrapper <command> [args...]
```

### Docker example

```yaml
services:
  myservice:
    image: yourimage:latest
    environment:
      - APP_UMASK=0002
    volumes:
      - ./umask-wrapper:/usr/local/bin/umask-wrapper:ro
    entrypoint: ["/usr/local/bin/umask-wrapper"]
    command: ["myservice_entrypoint.sh"]
```
