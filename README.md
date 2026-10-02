# nasactl

ESPHome external component for Samsung systems using the NASA (Network Attached Samsung Appliance) protocol. Supports air conditioning, heat pumps, DHW (domestic hot water) tanks, underfloor heating, and other equipment connected via the RS-485 bus.

## Features

- **NASA protocol only** — clean, focused implementation
- **Easy to extend** — add new entities by editing one line in `const.py`, no C++ needed
- **FSV auto-read** — Field Setting Values are polled on startup in batches with configurable delay and periodic re-poll
- **Proper device types** — hydro units get individual entities, ACs get a climate entity with modes and fan speeds
- **AC climate** — Cool, Heat, Dry, Fan Only, Auto modes with Auto, Low, Medium, High, Turbo fan speeds
- **Custom sensors** — read any NASA message code without modifying the component

## Supported Hardware

Tested with:
- Samsung EHS TDM Plus heat pump (hydro unit) with underfloor heating
- Samsung ducted AC indoor units
- DHW (Domestic Hot Water) tank
- 3-way valve

Should work with any Samsung equipment using the NASA protocol via RS-485 bus.

## Hardware Setup

See the [Hardware Installation wiki](https://docs.samsung-hvac.aran.net.tr/wiki/Hardware-Installation/) for wiring instructions, recommended boards, and RS-485 transceiver options.

## Quick Start

```yaml
external_components:
  - source: github://paolostivanin/nasactl@main
    components: [nasactl]

uart:
  tx_pin: GPIO19
  rx_pin: GPIO22
  baud_rate: 9600
  parity: EVEN

nasactl:
  devices:
    - address: "20.00.00"
      type: hydro
      water_temperature:
        name: "DHW Temperature"
      water_heater_power:
        name: "DHW Enabled"

    - address: "20.00.01"
      type: ac
      climate:
        name: "AC Bedroom"

    - address: "10.00.00"
      type: outdoor
      outdoor_temperature:
        name: "Outdoor Temperature"
```

See [`example.yaml`](example.yaml) for a complete configuration.

For heating/DHW deployments, replace `@main` with a reviewed immutable commit or release. See [Reliable writes and diagnostics](#reliable-writes-and-diagnostics) for default behavior, experimental options and deployment requirements.

## Configuration

### Main Component

```yaml
nasactl:
  # Optional RS-485 flow control pin (active-high = TX mode)
  flow_control_pin: GPIO23

  # Communication tuning
  silence_interval: 100    # ms silence after RX before TX (50-1000)
  retry_interval: 500      # ms between retries (200-5000)
  min_retries: 1           # minimum total transmission attempts (1-10)
  send_timeout: 4000       # ms since first transmission (1000-10000)

  # FSV auto-read configuration
  fsv_read:
    startup_delay: 5s      # wait after boot before first poll
    batch_size: 10          # codes per NASA read packet (1-50)
    batch_delay: 200ms      # delay between batches
    interval: 24h           # periodic re-poll (0s = startup only)

  # Debug
  debug_log_messages: false    # log all received messages
  debug_log_undefined: false   # log messages with no registered entity

  devices: [...]
```

### Device Types

Each device requires an `address` (format `XX.XX.XX`) and a `type`:

| Type | Description | Climate Entity |
|------|-------------|----------------|
| `hydro` | Heat pump / hydro unit | No (individual entities only) |
| `ac` | Air conditioning indoor unit | Yes (with modes + fan speeds) |
| `outdoor` | Outdoor unit | No (sensors only) |

Entity-to-device-type compatibility is validated at compile time: indoor entities (`0x4xxx` codes) cannot be used on `outdoor` devices, and outdoor entities (`0x8xxx` codes) cannot be used on `hydro`/`ac` devices. System codes (`0x0xxx`/`0x2xxx`) are allowed on any device type.

### Climate Entity (AC only)

AC devices support a climate entity with modes (Cool, Heat, Dry, Fan Only, Auto) and fan speeds (Auto, Low, Medium, High, Turbo).

### Custom Sensors

Read any NASA message code without modifying the component. Useful for codes not yet defined in `const.py`:

```yaml
- address: "10.00.00"
  type: outdoor
  custom_sensor:
    - name: "Condenser Mid Temperature"
      message: 0x8206
      device_class: temperature
      state_class: measurement
      unit_of_measurement: "°C"
      accuracy_decimals: 1
      divisor: 10        # optional: raw value / 10
      signed: true       # optional: treat as int16
```

## Adding New Entities

Edit `components/nasactl/const.py`. No C++ changes needed.

### Add a sensor (read-only)

```python
"my_new_sensor": {
    "type": "sensor",
    "code": 0x4XXX,         # NASA message hex code
    "unit": "°C",
    "device_class": "temperature",
    "state_class": "measurement",
    "accuracy": 1,
    "icon": "mdi:thermometer",
    "divisor": 10,           # raw / 10
    "signed": True,          # treat as int16
},
```

### Add a number (read-write)

```python
"my_new_number": {
    "type": "number",
    "code": 0x4XXX,
    "min": 0, "max": 100, "step": 1,
    "unit": "°C",
    "device_class": "temperature",
    "icon": "mdi:thermometer",
    "divisor": 10,
    "signed": True,
    "fsv": True,             # polled on startup (not broadcast)
    "entity_category": "config",
},
```

### Add a select (read-write enum)

```python
"my_new_select": {
    "type": "select",
    "code": 0x4XXX,
    "options": ["Option A", "Option B", "Option C"],
    "icon": "mdi:format-list-bulleted",
    "offset": 0,             # NASA value = option_index + offset
    "fsv": True,
    "entity_category": "config",
},
```

### Add a switch (read-write boolean)

```python
"my_new_switch": {
    "type": "switch",
    "code": 0x4XXX,
    "icon": "mdi:toggle-switch",
    "fsv": True,
    "entity_category": "config",
},
```

### Add a binary sensor (read-only boolean)

```python
"my_new_binary_sensor": {
    "type": "binary_sensor",
    "code": 0x4XXX,
    "icon": "mdi:check-circle",
},
```

### Add a text sensor (read-only with mapping)

```python
"my_new_text_sensor": {
    "type": "text_sensor",
    "code": 0x4XXX,
    "icon": "mdi:information",
    "mapping": {
        0: "Off",
        1: "Running",
        2: "Error",
    },
},
```

Then use it in your YAML:

```yaml
- address: "20.00.00"
  type: hydro
  my_new_sensor:
    name: "My New Sensor"
```

## Value Transformations

Most entities need no transformation (1:1 mapping). For those that do:

| Field | Read (device → HA) | Write (HA → device) | Example |
|-------|-------------------|---------------------|---------|
| `divisor: 10` | `x / 10` | `x * 10` | Temperatures: raw 250 → 25.0°C |
| `multiplier: 10` | `x * 10` | `x / 10` | FSV 3052: raw 18 → 180 min |
| Neither | `x` | `x` | Direct 1:1 mapping |

The `signed: True` flag treats the raw value as a signed 16-bit integer before applying transformations (important for negative temperatures).

## FSV Auto-Read

Field Setting Values (FSVs) are persistent device settings that are not broadcast automatically. The component polls them:

1. **On startup** — after `startup_delay`, reads all FSV-flagged codes
2. **In batches** — packs up to `batch_size` codes into a single NASA read packet
3. **With delay** — waits `batch_delay` between batches to avoid flooding the bus
4. **Periodically** — re-reads all FSVs every `interval` (if > 0)

This is useful because FSVs can be changed externally (e.g., from a Samsung wired remote), and periodic polling keeps HA in sync.

## Project Structure

```
components/nasactl/
├── __init__.py              # Schema generation + code generation
├── const.py                 # Entity definitions (EDIT THIS FILE)
├── nasa.py                  # C++ class references
│
├── nasa_address.h           # Address parsing
├── nasa_command.h           # Command structure
├── nasa_message.h           # MessageSet structure
├── nasa_crc.h               # CRC-16 CCITT
├── nasa_packet.h/.cpp       # Packet encode/decode
├── nasa_queue.h             # BatchDispatcher + LimitedQueue
├── nasa_device.h            # Device abstraction
├── nasa_base.h              # Base entity classes
├── nasa_client.h/.cpp       # UART communication
├── nasa_controller.h/.cpp   # Message routing + FSV polling
│
├── sensor/                  # NasactlSensor (read-only numeric)
├── number/                  # NasactlNumber (read-write numeric)
├── select/                  # NasactlSelect (read-write enum)
├── switch_/                 # NasactlSwitch (read-write boolean)
├── binary_sensor/           # NasactlBinarySensor (read-only boolean)
├── text_sensor/             # NasactlTextSensor (read-only mapped)
└── climate/                 # NasactlClimate (AC climate entity)
```

## Credits

This project combines ideas from:
- [Beormund/esphome-samsung-nasa](https://github.com/Beormund/esphome-samsung-nasa) — Python dictionary approach, FSV auto-read, clean config style
- [paolostivanin/esphome_samsung_hvac_bus](https://github.com/paolostivanin/esphome_samsung_hvac_bus) — C++ NASA protocol implementation, production reliability

## License

MIT

## Reliable writes and diagnostics

The tested ESPHome target is **2026.9.1**, with **2026.7.0** checked for compatibility. Automated checks and their limits are described in [tests/README.md](tests/README.md).

Only an ACK from the intended destination, addressed to this bridge and matching the transmitted front packet, completes a write. A NACK is a terminal refusal. Unanswered writes retain the configured retry and timeout policy. Reads transmit once and do not wait for an ACK. Failures log the destination, codes, values and send count.

After an ACK, NACK, timeout or queue rejection, nasactl schedules reads of the affected codes at approximately 3 and 15 seconds after that outcome. Climate devices include power, mode, fan and target temperature. Readback windows expire after 20 seconds; queue pressure and bus silence can delay or prevent delivery. Genuine Response/Notification messages update entities; ACKs do not confirm applied state. The existing optimistic publish remains immediate. A mismatch updates the entity from the reported value; it does not cause another write. Startup reads and periodic FSV polling remain enabled.

These transport fixes and automatic readback apply with the default configuration. The experimental switches below do not gate them. `send_timeout` starts with the first transmission, and expiry requires at least `min_retries` total attempts; it does not bound time waiting behind other packets or waiting for bus silence.

Readbacks are coalesced by device and code. A newer outcome restarts that code's window. The scheduler tracks at most 256 device/code pairs, queues at most 10 codes per reconciliation read, retries rejected admission after 200 ms, and limits each queued read to 5 seconds or the remaining window. If the first attempt cannot be admitted before the second window, it is skipped. At expiry, `No fresh report after write` means no genuine report of that code arrived during the window. A report can arrive unsolicited; freshness alone does not prove the requested value was accepted. Readback timing is fixed in code and has no YAML tuning options.

Optional diagnostics reset to zero at boot and use `total_increasing` state class:

```yaml
nasactl:
  tx_timeouts:
    name: "NASA TX Timeouts"
  tx_nacks:
    name: "NASA TX NACKs"
  tx_queue_drops:
    name: "NASA Queue Drops"
```

`tx_timeouts` counts unanswered writes and expired queued readbacks. `tx_nacks` counts matched write refusals. `tx_queue_drops` counts rejected send/read queue admissions and exhausted readback capacity. A rejected readback admission may be counted repeatedly while the scheduler retries admission.

For a bounded capture window, enable `debug_log_packets: true` and DEBUG logging. It records raw TX/RX bytes and command fields, including foreign and empty ACK packets. `debug_log_messages` continues to log routed values separately.

Wire changes requiring acceptance tests are **off by default**:

| Option | Where it belongs | Default | Effect when enabled |
|---|---|---|---|
| `standard_command_header` | `nasactl` | `false` | Transmit protocol version/retry fields at corrected bit positions. Decoding always uses those positions. |
| `packet_information` | `nasactl` | `false` | Set the packet-information header bit. Its effect on this installation still needs a capture. |
| `control_data_type` | Each device | `write` | Setting `request` uses Request for ordinary controls. Registered FSV writes retain Write semantics. |
| `targeted_reads` | Each device | `false` | Address reconciliation reads to this unit instead of broadcasting. Startup and periodic FSV reads remain broadcast. |
| `batch_writes` | Each AC's `climate` | `false` | Send the fields requested by one climate call in a single packet instead of separate packets. Acceptance does not establish atomic application by the unit. |

These options are independent. For example, `standard_command_header: true` does not also set `packet_information`. With the default version/retry values, the header byte is `0x20` by default, `0x40` with only the standard layout, or `0xC0` with both switches enabled. Simulator tests cover the default and experimental profiles; physical-device acceptance remains pending.

```yaml
nasactl:
  standard_command_header: false  # true uses version bits 5..6 and retry bits 3..4
  packet_information: false
  devices:
    - address: "20.00.02"
      type: ac
      control_data_type: write    # request is experimental; FSV always remains write
      targeted_reads: false      # true sends reconciliation reads to this address
      climate:
        name: "Offices"
        batch_writes: false      # true groups requested fields into one packet
```

Temperature/fan-only climate calls never add a power-on command. An OFF call can also contain explicitly requested temperature/fan fields. Default settings preserve the previous transmitted command header and separate climate writes. Enable experimental settings individually after capturing the baseline and verifying acceptance on one unit.

For heating/DHW installations, pin `external_components` to a reviewed immutable commit or release rather than a moving branch. Hardware capture, Home Assistant attribution checks and a multi-day soak remain release requirements; successful software tests alone do not establish device acceptance.
