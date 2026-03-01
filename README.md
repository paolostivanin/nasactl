# nasactl

ESPHome component for Samsung HVAC systems using the NASA (Network Attached Samsung Appliance) protocol.

## Features

- **NASA protocol only** — clean, focused implementation
- **Easy to extend** — add new entities by editing one line in `const.py`, no C++ needed
- **FSV auto-read** — Field Setting Values are polled on startup in batches with configurable delay and periodic re-poll
- **Proper device types** — hydro units get individual entities (no useless climate card), ACs get a real climate entity
- **AC climate modes** — Cool, Heat, Dry, Fan Only, Auto
- **AC fan speeds** — Auto, Low, Medium, High, Turbo
- **Custom sensors** — read any NASA message code without modifying the component

## Supported Hardware

Tested with:
- Samsung EHS TDM Plus heat pump (hydro unit)
- Samsung ducted AC indoor units
- DHW (Domestic Hot Water) tank
- 3-way valve

Should work with any Samsung HVAC equipment using the NASA protocol via RS-485 bus.

## Hardware Setup

You need an ESP32 board (e.g., M5Stack Atom Lite) connected to the Samsung NASA bus via an RS-485 transceiver (e.g., MAX485). Connect the transceiver to the F1/F2 terminals on your Samsung unit.

```
ESP32               MAX485              Samsung Unit
──────              ──────              ────────────
GPIO19 (TX) ──────► DI
GPIO22 (RX) ◄────── RO
GPIO23 (opt) ─────► DE/RE               F1 ◄────── A
                                         F2 ◄────── B
3.3V ─────────────► VCC
GND ──────────────► GND
```

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

## Configuration

### Main Component

```yaml
nasactl:
  # Optional RS-485 flow control pin (active-high = TX mode)
  flow_control_pin: GPIO23

  # Communication tuning
  silence_interval: 100    # ms silence after RX before TX (50-1000)
  retry_interval: 500      # ms between retries (200-5000)
  min_retries: 1           # min retry attempts (1-10)
  send_timeout: 4000       # ms total timeout per packet (1000-10000)

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

### Climate Entity (AC only)

AC devices support a climate entity with:

| Modes | Fan Speeds |
|-------|-----------|
| Cool | Auto |
| Heat | Low |
| Dry (dehumidify) | Medium |
| Fan Only (ventilator) | High |
| Auto (heat/cool) | Turbo |

### Custom Sensors

Read any NASA message code without modifying the component:

```yaml
- address: "10.00.00"
  type: outdoor
  custom_sensor:
    - name: "Compressor Frequency"
      message: 0x8238
      device_class: frequency
      state_class: measurement
      unit_of_measurement: Hz
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
