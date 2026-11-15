# Changelog

All notable changes to this component are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

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
