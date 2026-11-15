# idf-knx-device

An ETS-programmable **KNX IP device** stack for ESP-IDF, without an Arduino
layer.

It is a native ESP-IDF packaging of the device stack from
[thelsing/knx](https://github.com/thelsing/knx). Your ESP32 appears in ETS as
an ordinary KNX IP device (mask `0x57B0`). ETS assigns its individual address,
downloads group addresses and parameters into it, and the device then sends
and receives group telegrams over KNXnet/IP routing (multicast
`224.0.23.12:3671`).

Use this when you want an ESP32 to be a first-class KNX device that an
installer commissions with ETS. If you only want to read or write a few group
addresses from code, without ETS, a routing-only library is simpler.

## Features

- KNX IP device profile `0x57B0` (Bau57B0): device object, address,
  association and group object tables, application parameters, programming
  mode.
- ESP-IDF platform layer: `esp_netif` and lwIP sockets for KNXnet/IP, NVS for
  the ETS configuration, `esp_log` for library tracing.
- Optional FreeRTOS runtime task (`knx_runtime`) that drives the stack and
  polls a debounced programming button.
- Builds for ESP32, ESP32-S3 and ESP32-C6 on ESP-IDF 6.0 and 6.1. Runs on
  real hardware in hw2knx, a sensor-to-KNX bridge (ESP32-C6, ESP-IDF 6.1).

## Installation

```bash
idf.py add-dependency "erikmeinders/idf-knx-device^0.2.0"
```

or in your `main/idf_component.yml`:

```yaml
dependencies:
  erikmeinders/idf-knx-device: "^0.2.0"
```

Add `erikmeinders__idf-knx-device` to your component's `REQUIRES`.

## Usage

```cpp
#include "esp32_idf_platform.h"
#include "knx/bau57B0.h"
#include "knx/dpt.h"
#include "knx_facade.h"
#include "knx_runtime.h"

static Esp32IdfPlatform knxPlatform;
static Bau57B0 knxBau(knxPlatform);
static KnxFacade<Esp32IdfPlatform, Bau57B0> knx(knxBau);

extern "C" void app_main(void)
{
    // 1. nvs_flash_init(), start Wi-Fi or Ethernet, and on IP_EVENT_*_GOT_IP:
    //        knxPlatform.setNetif(netif);
    //    Wait for that before knx.start(), which joins the multicast group.

    // 2. Load what ETS downloaded. Group objects only exist once configured.
    knx.readMemory();
    if (knx.configured()) {
        knx.getGroupObject(1).dataPointType(DPT_Value_Temp);
    }

    // 3. Start the stack and hand it to a task.
    knx.start();
    KnxRuntimeConfig runtime;
    runtime.loopFn = [] {
        knx.loop();
        // application code that reads/writes group objects goes here
    };
    runtime.buttonGpio = 9;  // active-low prog button, or -1
    runtime.onButtonPress = [] { knx.toggleProgMode(); };
    knx_runtime_start(runtime);
}
```

Things to know:

- **Call `setNetif()` before `start()`.** The platform reads the IP address,
  netmask and gateway from that netif, and `start()` joins the KNX multicast
  group on it.
- **Check `knx.configured()` before touching group objects.** Before the
  first ETS download they have no backing storage.
- **Touch group objects from one task only.** The stack is not thread-safe.
  Put application logic in `loopFn`, or hand values to it through a queue.
- **Programming LED:** use `knx.ledPin()` and `knx.ledPinActiveOn()` for a
  plain GPIO LED. For an addressable LED, use
  `setProgLedOnCallback()` / `setProgLedOffCallback()`.
- **Unset objects are not readable.** Until a group object has been given a
  value, a GroupValueRead for it gets no answer (not a zero). Readers receive
  the value when the application first sets it and it is transmitted.

### Group activity callback

To react to bus traffic, for example to blink an LED, register a callback on
the BAU:

```cpp
knxBau.groupActivityCallback([](GroupActivity activity, uint16_t asap) {
    switch (activity) {
    case GroupActivity::ReadAnswered:  /* someone read object `asap` */ break;
    case GroupActivity::ReadIgnored:   /* ... before it had a value */  break;
    case GroupActivity::WriteSent:     /* we transmitted `asap` */      break;
    case GroupActivity::WriteReceived: /* the bus wrote to `asap` */    break;
    }
});
```

It runs in the task that calls `knx.loop()`. Keep it short, and don't call
back into the stack from it.

### Configuration (`idf.py menuconfig` → KNX device)

| Option | Default | Meaning |
|---|---|---|
| `KNX_MASK` | `0x57B0` | Device profile reported to ETS. It must match the `MaskVersion` of your ETS product. |
| `KNX_FLASH_SIZE` | 4096 | Bytes reserved for the ETS configuration. Stored as one NVS blob. |

### ETS product database

ETS can only commission a device it has a product entry (`.knxprod`) for. Its
group objects, parameter layout, application number and version, and mask
must match your firmware. The example ships thelsing's demo product. For your
own device, create one with
[Kaenx-Creator](https://github.com/OpenKNX/Kaenx-Creator) or a similar tool.

## Example

[examples/ip_device_demo](examples/ip_device_demo) is a simulated temperature
sensor that you can commission with the bundled
[ets/knx-demo-ip.knxprod](examples/ip_device_demo/ets/knx-demo-ip.knxprod):

```bash
cd examples/ip_device_demo
idf.py set-target esp32c6
idf.py menuconfig          # KNX IP device demo -> Wi-Fi SSID/password, GPIOs
idf.py build flash monitor
```

In ETS, import the `.knxprod` and add the "Temperatursensor" device. Press
the programming button and run *Program individual address*, then
*Program application*. After that the device sends a temperature on group
object 1 every 10 seconds.

## Credits and license

This component is derived from **[thelsing/knx](https://github.com/thelsing/knx)**
by Thomas Kunze and contributors, at commit
[`980c047`](https://github.com/thelsing/knx/commit/980c047ad7fc5e27bf2fae95e48acde5d5e0b4fd)
(2025-11-04). Almost all of the code in `src/knx/` is theirs, and so is the
demo ETS product. Thank you.

Like upstream, it is licensed under the **GNU General Public License v3.0**
(see [LICENSE](LICENSE)). Firmware that you distribute with this component
must therefore also be available under the GPLv3.

KNX is a registered trademark of the KNX Association. This project is not
affiliated with or certified by the KNX Association.

### Changes from upstream

- Removed the Arduino, ESP8266, SAMD, RP2040, STM32, LibreTiny, Linux and
  CC1310 platforms. Only `esp32_idf_platform` remains.
- `esp32_idf_platform.cpp`:
  - Convert IP address, netmask and gateway from network to host byte order
    (`ntohl`). Without this, the multicast join and the IP addresses reported
    to ETS were wrong.
  - Give the multicast socket a 20 ms receive timeout so `loop()` no longer
    blocks while waiting for a telegram.
  - Set `SO_REUSEADDR` and `IP_MULTICAST_LOOP`.
- `knx_facade.h`: check `_progLedOnCallback`, not `_progLedOffCallback`,
  before calling the "LED on" callback.
- `knx/bau_systemB_device.{h,cpp}`:
  - Don't answer a GroupValueRead for a group object that has never had a
    value.
  - Added the optional group activity callback.
- `knx/bits.cpp`: buffer `print()`/`println()` fragments into whole lines and
  emit them through `esp_log` at DEBUG level, instead of `printf`.
- Added `knx_runtime.{h,cpp}`, a FreeRTOS task helper with a polled,
  debounced programming button.
- Build: Kconfig options replace the global `MASK_VERSION`/`KNX_FLASH_SIZE`
  defines, and only `src/` is a public include directory.
