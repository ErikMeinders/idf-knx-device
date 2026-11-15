# CLAUDE.md

Guidance for Claude Code when working in this repository.

## What this is

An ESP-IDF component, published as `erikmeinders/idf-knx-device`. It packages
the thelsing/knx device stack (GPLv3, vendored from upstream commit `980c047`)
for native ESP-IDF, with no Arduino layer. The repo root *is* the component
(`CMakeLists.txt`, `Kconfig`, `idf_component.yml`).
`examples/ip_device_demo` is a flashable project that consumes it through
`override_path: "../../../"`, so it builds this repo's `src/`. The configure
step prints `NOTICE: Using component placed at ...` when that works.

It was split out of hw2knx (`../hw2knx`), which is still its main consumer.

## Layout

- `src/knx/`: upstream core. Keep it byte-identical to upstream except for the
  files listed under "Changes from upstream" in README.md. That keeps a future
  sync a plain diff against `980c047`.
- `src/esp32_idf_platform.*`, `src/knx_facade.*`, `src/knx/bits.cpp`,
  `src/knx/bau_systemB_device.*`: upstream files carrying our changes. The
  last pair holds the read guard (no GroupValueRead answer before a value is
  set) and the `GroupActivity` callback, both added in 0.2.0. Each modified upstream file starts with a "Modified 2026 by ..."
  notice, which GPLv3 §5(a) requires. Add one to any upstream file you change,
  and list the change in README.md and CHANGELOG.md.
- `src/knx_runtime.*`: ours.
- Only `src/` is a public include directory. Core files include each other by
  bare name (`"config.h"`), which resolves relative to the including file. Do
  **not** add `src/knx` to `INCLUDE_DIRS`: it would put `config.h`,
  `memory.h` and similar names on every consumer's include path.
- `MASK_VERSION`, `KNX_FLASH_SIZE` and `KNX_NO_AUTOMATIC_GLOBAL_INSTANCE` are
  PUBLIC compile definitions set from Kconfig in `CMakeLists.txt`. Only mask
  `0x57B0` is offered, because it is the only one tested with this platform.
  Upstream's TP (`0x07B0`) and coupler masks need UART/TP-UART work and
  testing before they are added to the Kconfig choice.

## Build

```bash
cd examples/ip_device_demo
source ~/.espressif/tools/activate_idf_v6.1.sh   # esp32c6 only locally; use v6.0.2 for esp32/esp32s3
idf.py set-target esp32c6
idf.py build
```

CI ([build.yml](.github/workflows/build.yml)) builds the example for IDF
v6.0 and v6.1 on esp32, esp32s3 and esp32c6, plus an allowed-to-fail v5.5
job. That job relaxes the manifest's `idf >= 6.0` in its CI copy, so it tests
the code rather than the constraint. It has passed since 0.1.0, so the
manifest could be widened to `>=5.5` in a future release.

## Testing on hardware

- hw2knx (`../hw2knx`) on an ESP32-C6 is the real-world test. Its
  `knx_activity` log tag shows every `GroupActivity` event per group object.
- Bus traffic can't be observed from the Mac: the home Wi-Fi AP doesn't
  forward KNXnet/IP multicast between wireless clients. Use the Raspberry Pi
  with knxd: `knxtool groupsocketlisten ip:localhost` and
  `knxtool groupread ip:localhost <GA>`.
- 0.2.0 was verified that way: a read is answered after boot, reads in the
  first seconds after boot get no answer, and writes and cyclic resends
  appear on the bus.

## Publishing

This follows the same flow as `../webdav`. See its CLAUDE.md for the
pitfalls, which are summarized here:

1. Bump `version:` in `idf_component.yml`. A version can be uploaded only
   once and can never be replaced.
2. Move the CHANGELOG `Unreleased` entries under the new version.
3. Bump the version ranges in README.md (two places) and in
   `examples/ip_device_demo/main/idf_component.yml`. A caret range on 0.x
   does not widen past the next minor version.
4. `compote component pack --name idf-knx-device --version X.Y.Z` and
   `tar tzf dist/...`. compote packs the working tree, not git.
5. Push to `main`. [release.yml](.github/workflows/release.yml) uploads via
   OIDC. Re-uploading an existing version is a no-op. A red run that says
   "wasn't processed in 300 seconds" may still have published, so check
   `https://components.espressif.com/api/components/erikmeinders/idf-knx-device`
   before bumping again.
6. Tag the release commit `vX.Y.Z` (annotated) and push the tag.

One-time setup before the first publish: on components.espressif.com, add a
trusted uploader for `ErikMeinders/idf-knx-device`. The comparison is
case-sensitive, so use exactly that casing. Then dry-run with
`gh workflow run release.yml -f dry_run=true`.
