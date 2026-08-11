# CI Pipeline (`.github/workflows/build.yml`)

**Repository:** `stm32-sensor-node`

---

## 1. Purpose

This document records what the GitHub Actions workflow at [`.github/workflows/build.yml`](../.github/workflows/build.yml) actually does.

The workflow's scope, in one line: **confirm this repository still builds for `sensor_node1`, confirm its C code is formatted per `.clang-format`, and, on a version tag, publish the resulting firmware as a GitHub Release.** It does not run any test suite, and it does not flash real hardware — see [Section 6](#6-known-gaps).

---

## 2. Triggers

```yaml
on:
  push:
    branches: [ main ]
    tags: [ 'v*' ]
  pull_request:
    branches: [ main ]
```

| Event | Behavior |
|---|---|
| Push to `main` | Runs `lint` + `build`. `release` does not run (tag condition not met). |
| Push of a tag matching `v*` (e.g. `v1.0.0`) | Runs `lint` + `build`, then `release` (see [Section 3.3](#33-release)). |
| Pull request targeting `main` from a `Feature/*` or `Bug/*` branch | Runs `lint` + `build`. `release` never runs on a PR — it requires a tag ref. |
| Pull request targeting `main` from any other branch name | Both jobs are skipped (see below) — the run shows up as skipped, not failed. |

`pull_request.branches: [ main ]` restricts the trigger to PRs targeting `main`, but GitHub Actions has no equivalent filter for the *source* branch name, so that half is enforced with a job-level `if:`:

```yaml
if: github.event_name != 'pull_request' || startsWith(github.head_ref, 'Feature/') || startsWith(github.head_ref, 'Bug/')
```

`github.head_ref` is only set on `pull_request` events (it's empty on `push`), so the `github.event_name != 'pull_request'` clause lets push-triggered runs (to `main`, or a `v*` tag) through unconditionally, while a PR only proceeds if its source branch starts with `Feature/` or `Bug/`. Both `lint` and `build` carry this condition; a PR from a branch like `gpio_interrupt` or `chore/x` will show the run as **skipped**, not failed. Branch names are case-sensitive here — `feature/x` (lowercase) does **not** match.

---

## 3. Jobs

The workflow has three jobs: `lint` and `build` run independently (no dependency between them — a lint failure does not block or wait on the build), and `release` runs only after `build` succeeds, and only on a tag push.

### 3.1 `lint`

```yaml
lint:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - name: Install clang-format
      run: sudo apt-get update && sudo apt-get install -y clang-format
    - name: Check code formatting (.clang-format)
      run: |
        find src -regex '.*\.\(c\|h\)$' -print0 \
          | xargs -0 clang-format --dry-run --Werror
```

Runs `clang-format` in `--dry-run --Werror` mode over every `.c`/`.h` file under `src/`, against the repository's existing [`.clang-format`](../.clang-format) (itself based on Zephyr's own style, per that file's header comment). `--dry-run` means no file is rewritten; `--Werror` means any formatting deviation fails the job rather than merely printing a diff. This job does not need the Zephyr SDK or a west workspace at all, which is why it's a separate, lighter job — it fails fast without paying the cost of the `build` job below.

Deliberately scoped to `src/` only — `boards/st/sensor_node1/` contains no `.c`/`.h` files (devicetree, Kconfig, and YAML, none of which `clang-format` applies to).

### 3.2 `build`

Runs on `ubuntu-latest`, with the SDK version and install path pinned via `env`:

```yaml
env:
  ZEPHYR_SDK_VERSION: 0.17.4
  ZEPHYR_SDK_INSTALL_DIR: ${{ github.workspace }}/zephyr-sdk
  ZEPHYR_TOOLCHAIN_VARIANT: zephyr
```

Steps, in order:

| # | Step | What it does |
|---|---|---|
| 1 | Checkout application | Checks out this repository to `app/` — **first**, because this repository is now the west manifest itself (`self.path: app` in [`west.yml`](west_manifest.md)); `west init -l app` in step 4 needs it present locally already. |
| 2 | Install build dependencies / Install west | Same apt package list and `pip3 install west` as before — `ninja`, `cmake`, `gperf`, `ccache`, `dfu-util`, the device-tree compiler, and the Python toolchain `west` itself needs. |
| 3 | Cache Zephyr SDK / Download Zephyr SDK / Register Zephyr SDK toolchain | See [Section 4](#4-caching-strategy). The download+extract is skipped on a cache hit; the toolchain *registration* (`setup.sh -t arm-zephyr-eabi -c`) always runs, since it writes to `~/.cmake/packages`, which is outside the cached path. |
| 4 | Cache Zephyr workspace (west projects) / Init Zephyr workspace from manifest | `west init -l app` reads `app/west.yml` locally (no clone needed — the manifest repo is already checked out); `west update` then resolves `zephyr` at `v4.3.0` plus every module it imports, into `zephyr/`, `modules/`, `bootloader/`, `tools/` at the workspace root. |
| — | Install Zephyr Python dependencies | `pip3 install -r zephyr/scripts/requirements.txt`, run only after step 4 populates `zephyr/` — that file is what pulls in `pyelftools`, `pyyaml`, and the other packages Zephyr's own build-time scripts (e.g. `gen_kobject_list.py`) import at ninja time. `pip3 install west` earlier only installs the `west` tool itself, not these; skipping this step fails the build with `ModuleNotFoundError: No module named 'elftools'`. |
| 5 | Cache ccache | See [Section 4](#4-caching-strategy). |
| 6 | Build stm32-sensor-node | `west build -b sensor_node1 app -d build`, with `ZEPHYR_BASE` pointed at `${{ github.workspace }}/zephyr` (populated by step 4). |
| 7 | Upload firmware | Publishes `build/zephyr/zephyr.hex` and `build/zephyr/zephyr.bin` as a downloadable workflow artifact named `stm32-sensor-node-sensor_node1`, retrievable from the run's summary page under **Actions**. |

### 3.3 `release`

```yaml
release:
  needs: build
  if: startsWith(github.ref, 'refs/tags/v')
  runs-on: ubuntu-latest
  permissions:
    contents: write
  steps:
    - uses: actions/download-artifact@v4
      with:
        name: stm32-sensor-node-sensor_node1
        path: firmware
    - name: Create GitHub Release
      env:
        GH_TOKEN: ${{ github.token }}
      run: |
        gh release create "${{ github.ref_name }}" \
          firmware/zephyr.hex firmware/zephyr.bin \
          --title "${{ github.ref_name }}" \
          --generate-notes
```

`needs: build` means this job only starts after `build` finishes successfully — it downloads the exact artifact `build` just produced rather than rebuilding. The `if:` condition restricts it to tag pushes matching `v*` (e.g. pushing `git tag v1.0.0 && git push origin v1.0.0`). It uses GitHub's own `gh` CLI (preinstalled on `ubuntu-latest` runners) with the workflow's ephemeral `${{ github.token }}`, rather than a third-party release action, to create a GitHub Release named after the tag, attach both firmware files, and auto-generate release notes from the commits since the previous tag.

**One-time setup this job depends on**, not yet confirmed done: the repository's default `GITHUB_TOKEN` permissions must allow write access to Releases. Under **Settings → Actions → General → Workflow permissions**, "Read and write permissions" must be selected — the default for new repositories is read-only, which would make `gh release create` fail with a permissions error (HTTP 403) the first time a tag is pushed.

---

## 4. Caching Strategy

Three independent `actions/cache@v4` layers, each targeting a different expensive step:

| Cache | Path | Key | What it avoids |
|---|---|---|---|
| Zephyr SDK | `zephyr-sdk/` | `zephyr-sdk-<version>-<os>` | Re-downloading and extracting the ~1 GB SDK tarball on every run. Invalidates only when `ZEPHYR_SDK_VERSION` changes. |
| Zephyr workspace | `zephyr/`, `modules/`, `bootloader/`, `tools/` | `west-workspace-<hash of app/west.yml>` | Re-cloning Zephyr and every HAL/module `import: true` pulls in. Invalidates automatically whenever `west.yml`'s contents change (e.g. a revision bump, [`west_manifest.md`, Section 7](west_manifest.md#7-updating-the-pinned-zephyr-revision)) — the hash changes, so the old cache simply isn't matched, and `west update` fetches fresh. |
| ccache | `~/.cache/ccache` | `ccache-<os>-<run id>`, falling back to `ccache-<os>-` | Full recompilation of unchanged object files between runs. Every run saves a new cache under its own `run_id` (required — `actions/cache` only *restores* a key that already exists, never overwrites one), but `restore-keys` lets each run start from the most recent prior run's cache, so the hit rate builds up over time rather than resetting per-run. |

Net effect: the first run on a given SDK version and `west.yml` pays the full cost (SDK download + full Zephyr/module clone), roughly 10–15 minutes. Subsequent runs restore both caches and only pay for `west update`'s incremental fetch (fast — the local checkout already matches most of what's on the remote) and the actual compile, which `ccache` then also shrinks for any file unchanged since the last run.

---

## 5. Requirements / One-Time Setup

For this workflow to run successfully end-to-end on a fresh repository:

1. **Actions enabled** — Settings → Actions → General → "Allow all actions and reusable workflows" (default for a repository you own).
2. **Workflow permissions set to read-write** — required only for the `release` job ([Section 3.3](#33-release)); `lint` and `build` need no elevated permissions.
3. **`west.yml` present at the repository root** — the `build` job's step 4 fails immediately without it. See [`west_manifest.md`](west_manifest.md).

No repository secrets are required — every URL fetched (Zephyr SDK release, Zephyr source, module sources) is public, and `GH_TOKEN` in the `release` job uses the workflow's own ephemeral `github.token`, not a stored secret.

---


## 7. References

* GitHub Actions workflow syntax: https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions
* `actions/cache` documentation: https://github.com/actions/cache
* GitHub CLI `gh release create`: https://cli.github.com/manual/gh_release_create
* Zephyr Twister test runner: https://docs.zephyrproject.org/latest/develop/test/twister.html
* [`west_manifest.md`](west_manifest.md) — the manifest this workflow initializes from
* [`board_customization.md`](board_customization.md) — the board this workflow builds
