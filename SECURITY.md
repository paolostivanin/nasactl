# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in this project, please report it responsibly by emailing the maintainer directly rather than opening a public issue.

You can reach the maintainer via the contact information on their [GitHub profile](https://github.com/paolostivanin).

## Scope

This component runs on ESP32 microcontrollers and communicates over:

- **RS-485 bus** — physical serial connection to Samsung NASA equipment
- **Wi-Fi** — for Home Assistant API and OTA updates (managed by ESPHome, not this component)

### What this component controls

nasactl can read and write values on the Samsung NASA bus. This includes changing HVAC operating modes, target temperatures, DHW settings, and other device parameters. Anyone with physical access to the RS-485 bus can send arbitrary commands to the connected equipment.

### Security considerations

- **Physical access** — The RS-485 bus has no authentication. Protect physical access to the bus wiring and the ESP32 device.
- **Network access** — Secure your Wi-Fi network and use the ESPHome API encryption key and OTA password. Do not expose the ESP32's web server or API port to the internet.
- **Firmware** — Only flash firmware from sources you trust. Review YAML configurations before compiling.

## Supported Versions

Only the latest version on the `main` branch is supported with security fixes.
