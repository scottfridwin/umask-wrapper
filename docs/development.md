# Development

## Build and test locally

With `gcc` and static libc available (the dev container installs them):

```bash
make        # builds ./umask-wrapper
make test   # runs test.sh against it
```

`test.sh` checks the resulting permissions, inherited umask, rejection of invalid values, exit codes, and that arguments and environment pass through unchanged.

## Release binaries

```bash
make dist
```

This runs `Dockerfile.build` with Docker Buildx for `linux/amd64`, `linux/arm64`, `linux/arm/v7` and `linux/386`. For each platform it compiles a static binary with musl on Alpine and runs `test.sh` against it (under QEMU for foreign architectures). The binary is exported from the test stage, so a binary is only produced if its tests passed. Multi-platform builds need a Buildx builder with the `docker-container` driver and QEMU binfmt handlers installed.

## Continuous integration and releases

All changes reach `main` through a pull request; the **Test** check (the multi-platform build and tests above) must pass and merges are squashed.

- **Dependencies** — the Alpine base image (pinned by digest), the dev container and the GitHub Actions (pinned to commit SHAs) are updated by [Renovate](https://docs.renovatebot.com/). Updates wait 3 days after publication, then merge automatically once **Test** passes.
- **Releases** are created automatically when a merge to `main` changes `umask-wrapper.c` or `Dockerfile.build`. The version is derived from the commit subject: `feat:` bumps the minor version, `<type>!:` or a `BREAKING CHANGE` footer bumps the major version, anything else (including Renovate updates) bumps the patch version. To release manually, run the **Build, Test & Release** workflow with a `release_tag` such as `v1.2.0`.
- **Release contents** — the exact binaries that passed **Test**, `SHA256SUMS`, and a build provenance attestation.
- If an unattended run on `main` fails, an issue titled **CI failed on main** is opened.

> [!IMPORTANT]
> Consumers that download `releases/latest` pick up a new release on their next deploy. Keep the asset names (`umask-wrapper.<arch>`) stable.
