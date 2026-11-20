# Changelog

All notable changes to this component are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.3.0] - 2026-11-20

### Added

- `knx_runtime_post()`: run a function in the runtime task before its next
  `loopFn`, so other tasks can safely touch group objects and parameters.
  Queue size is `KnxRuntimeConfig::postQueueLength` (default 16, 0 disables).
- `KnxRuntimeConfig::coreId`: pin the runtime task to a core (default: no
  affinity).
- `Esp32IdfPlatform::multicastPending()`: non-blocking check for a waiting
  datagram.
- Multicast membership renewal: every `CONFIG_KNX_IGMP_REFRESH_S` seconds
  (default 20, 0 = off) the platform sends an unsolicited IGMP membership
  report while the socket stays open (lwIP reports every group the interface
  has joined, mDNS included, one packet each). Wi-Fi access points
  stop forwarding multicast to a station at random moments, for up to
  minutes (measured on a TP-Link Deco mesh: 77 gaps of 40-100 s in 7.4 h on
  one device); a report ends that within a second. The renewal deliberately
  sends no IGMP Leave: on the Deco, a Leave from one ESP32 made other ESP32s
  on the same access point deaf (16 of 18 outages started 1.3-1.9 s after
  another device's leave-and-rejoin). `Esp32IdfPlatform::refreshMembership()` renews on demand,
  for example after an expected telegram stayed away.
- `Esp32IdfPlatform::linkStats()`: reception counters readable from any task.
  Datagrams per sender (8 addresses), own looped-back telegrams, time since
  the last datagram from another device, renewals, renewal failures and
  failed sends.

### Fixed

- KNX/IP bursts were dropped: `knx.loop()` read one datagram per call, and
  lwIP's per-socket queue (6 by default) overflowed silently. `loop()` now
  handles every waiting datagram (up to 32 per call). Measured on an
  ESP32-C6 with 30-telegram bursts at 1 ms spacing: 37% arrived before, 91%
  after, 100% with `CONFIG_LWIP_UDP_RECVMBOX_SIZE=16` (now recommended in the
  README). No API change: `readBytesMultiCast()` overrides still see every
  datagram, and replies keep their pace.

### Known issues

- `refreshMembership()` called from a task other than the KNX task reads the
  socket state without synchronisation. If the call coincides with the stack
  opening or closing the multicast socket (ETS restart, Wi-Fi stop/start),
  its fallback join can leave an extra IGMP membership that no socket owns,
  so the group is never left again. Reception is not affected. The periodic
  renewal runs in the KNX task and is not affected. Planned fix for 0.3.1:
  the public call only requests a renewal, and the KNX task performs it.

## [0.2.0]

### Added

- `BauSystemBDevice::groupActivityCallback()`: optional callback for group
  communication events (read answered, read ignored, write sent, write
  received), for example to blink a status LED.

### Changed

- A GroupValueRead for a group object that has never been given a value is no
  longer answered. Before, the device replied with the zero-filled buffer, so
  a reader asking right after boot got for example 0.0 °C.

## [0.1.0]

### Added

- First release: ESP-IDF native packaging of the thelsing/knx device stack at
  upstream commit `980c047`, with the KNX IP device profile (`0x57B0`).
- `knx_runtime` FreeRTOS task helper with a debounced programming button.
- Kconfig options for the mask version and the configuration storage size.
- `ip_device_demo` example plus thelsing's demo ETS product.

### Fixed (relative to upstream)

- `esp32_idf_platform`: IP address, netmask and gateway byte order, which
  broke the multicast join and the IP reported to ETS.
- `esp32_idf_platform`: `loop()` blocked until the next telegram arrived.
- `knx_facade.h`: the programming LED "on" callback was gated on the "off"
  callback being set.
