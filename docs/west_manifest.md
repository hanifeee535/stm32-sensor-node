# West Manifest (`west.yml`)

**Repository:** `stm32-sensor-node`

---

## 1. Purpose

This document records why `west.yml` exists at the root of this repository, what topology it implements, and how it's used both locally and in CI.

Before this file existed, building this repository meant manually running `west init` against Zephyr's upstream repository directly, with no revision pinned — whatever commit happened to be at the tip of Zephyr's default branch on the day of the build. `west.yml` replaces that with a single, version-controlled pin: given only this repository, `west` can resolve a specific, reproducible Zephyr tree without any other input or manual step.

---

## 2. Why a Manifest Was Added

The immediate driver was [`.github/workflows/build.yml`](../.github/workflows/build.yml) (see [`ci_pipeline.md`](ci_pipeline.md)). A GitHub Actions runner starts from a clean VM with no pre-existing Zephyr checkout anywhere — every run has to resolve one from scratch. Two options existed for that:

1. Point CI at Zephyr's own upstream repository directly (`west init -m https://github.com/zephyrproject-rtos/zephyr.git`), tracking whichever revision that repository's default branch happens to be at when the job runs.
2. Give this repository its own manifest, pinning an explicit Zephyr revision, and have CI initialize *from this repository*.

Option 1 was the initial approach and is unpinned by construction — a passing build today gives no guarantee the same commit still builds tomorrow, since "upstream default branch" is a moving target that can introduce breaking changes (renamed Kconfig symbols, changed devicetree bindings, updated HAL APIs) with no warning. Option 2 was adopted instead: `west.yml` pins Zephyr to `v4.3.0`, a release confirmed to build this board correctly, so a CI result — and a local build — means something reproducible, not "whatever Zephyr happened to be today."

---

## 3. Manifest Topology: T2 (Application-with-Manifest)

West recognizes a few standard manifest topologies. This repository uses **T2 — application repository *is* the manifest repository**: the same repository that holds `src/`, `boards/`, `CMakeLists.txt`, and `prj.conf` also holds `west.yml` at its root and doubles as the `self` project.

```
west init -m <this-repo-url> workspace
        |
        v
workspace/
├── .west/                  (created by west init)
├── app/                    (this repository — the manifest's `self.path`)
│   ├── west.yml
│   ├── CMakeLists.txt
│   ├── prj.conf
│   ├── src/
│   └── boards/
└── zephyr/                 (pulled in by the `zephyr` project + import: true)
    └── ...
```

This is the topology Zephyr's own "Getting Started" guide assumes for a standalone application repository — no separate, external manifest repository is needed.

---

## 4. Contents of `west.yml`

```yaml
manifest:
  version: "0.13"

  remotes:
    - name: zephyrproject-rtos
      url-base: https://github.com/zephyrproject-rtos

  projects:
    - name: zephyr
      remote: zephyrproject-rtos
      revision: v4.3.0
      import: true

  self:
    path: app
```

| Key | Role |
|---|---|
| `version` | The manifest schema version this file is written against — required by `west`, checked against the installed `west` version. |
| `remotes` | A named shorthand (`zephyrproject-rtos`) for a URL prefix, so `projects` entries don't repeat the full GitHub org URL. |
| `projects[0].revision` | Pins Zephyr to tag `v4.3.0` — not a branch, not `main`. This is the single line that makes the build reproducible; bumping the Zephyr version means changing this one value (see [Section 6](#6-updating-the-pinned-zephyr-revision)). |
| `projects[0].import: true` | Tells `west` to also read `zephyr/west.yml` (Zephyr's own manifest) once it's fetched, and resolve every module Zephyr itself depends on — `hal_stm32`, `cmsis`, and the rest of the HAL/module set this board's STM32F407 support needs. Without `import: true`, only the bare `zephyr` repository would be fetched, with none of its own dependencies — the build would fail at the CMake configure step, unable to find the STM32 HAL. |
| `self.path` | Declares that when this manifest repository is checked out into a workspace, it should land at `<workspace>/app` — not at a name derived from the repository's URL (which would be `stm32-sensor-node`). This matches the checkout path used in CI ([`build.yml`](../.github/workflows/build.yml), the `Checkout application` step) and keeps `west build -b sensor_node1 app` consistent everywhere this manifest is used. |

---

## 5. Building With This Manifest

From a fresh clone (this is what CI does, and what any local build now does too):

```bash
west init -m https://github.com/hanifeee535/stm32-sensor-node.git --mr main workspace
cd workspace
west update
west build -b sensor_node1 app -d build
```

`west update` resolves `zephyr` at `v4.3.0` plus every module it imports, into `workspace/zephyr/`, `workspace/modules/`, etc., alongside `workspace/app/` (this repository, per `self.path: app`).

If the repository is already cloned locally (as in CI, where `actions/checkout` has already placed it at `app/`), the equivalent is:

```bash
west init -l app
west update
```

`-l app` tells `west` "the manifest is already present locally at this path" — it skips cloning the manifest repository itself and only fetches the projects it declares.

---

## 6. Updating the Pinned Zephyr Revision

To move to a newer Zephyr release:

1. Edit `revision: v4.3.0` in this file to the target tag (e.g. `v4.4.0`).
2. Run `west update` in a workspace built from this manifest and confirm `west build -b sensor_node1 app` still succeeds — a new Zephyr release can rename or remove Kconfig symbols, devicetree bindings, or HAL APIs this board's files depend on.
3. Commit the `west.yml` change. CI's `build.yml` workspace cache is keyed on this file's contents ([`ci_pipeline.md`, Section 4](ci_pipeline.md#4-caching-strategy)), so a revision bump automatically invalidates the stale cached Zephyr checkout on the next run.

---

## 7. References

* Zephyr West Manifest reference: https://docs.zephyrproject.org/latest/develop/west/manifest.html
* Zephyr West Basics — workspace topologies (T1/T2/T3): https://docs.zephyrproject.org/latest/develop/west/basics.html
* Zephyr Getting Started Guide (standalone application workflow): https://docs.zephyrproject.org/latest/develop/getting_started/index.html
* [`ci_pipeline.md`](ci_pipeline.md) — how this manifest is consumed in CI
* [`board_customization.md`](board_customization.md) — the board this manifest builds
