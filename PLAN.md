# Plan: make nasactl writes reliable and visible

Status: software implementation and local verification complete on `fix/reliable-writes`, updated
2026-10-02. Hardware capture, deployment and soak are pending. Historical findings
below refer to `main` at a572c18; the incident trace is consistent with a lost mode
write but does not prove its cause. Primary ESPHome target: **2026.9.1**;
compatibility target: **2026.7.0**.

## Why

On 2026-09-29 the Home Assistant "start dry-out" script sent five AC units to
fan-only/high. Offices ended up in `cool`. Recorder trace for Offices, from the
moment the command was sent:

| time | Offices in HA | source |
|---|---|---|
| +0 s | fan_only / high | `control()` optimistic publish |
| +8 s | off | bus report - power-on not landed yet |
| +19 s | cool / auto | bus report - powered on in its remembered mode, mode frame lost |
| +30 s | cool / high | bus report - fan frame landed |

The HA script checked the unit at +6 s, read the optimistic echo and moved on.
Other units showed the same in-between states (Kids read `cool` at +7.8 s
before settling). Units report every ~40.6 s on their own, and 2-30 s after a
write.

The HA side is fixed (ha-config: `ac_dryout.yaml` v1.4.0, `ac_curfew.yaml`
v1.4.2, `bedroom_ac_night.yaml` v2.8.0 no longer judge a unit in the run that
commanded it, and wait 50-60 s for a real report). But the mode frame was still
lost, and nothing on the bridge said so. This plan is about that.

## Historical write behavior at a572c18

- `NasaController::write()` -> `NasaClient::send_write()` builds one packet per
  message code (`nasa_packet.cpp:22-34`) and appends it to `send_queue_`.
- The queue is head-of-line: only the front packet is transmitted
  (`nasa_client.cpp:92-135`). It is re-sent every `retry_interval` (500 ms)
  until an ACK removes it or `send_timeout` (4000 ms) drops it - about 8 sends.
  Everything behind it waits.
- Entities publish the commanded value immediately, before anything is sent:
  climate `nasactl_climate.cpp:134`, switch `nasactl_switch.h:24`, select
  `nasactl_select.h:31`, number `nasactl_number.h:28`. HA sees the truth only
  when the unit next reports that code.
- FSV entities are only read at boot and every `fsv_read.interval`. The
  samsung-nasa-bridge config uses 24 h.

## Findings

### F1. A NACK counts as delivered
`nasa_client.cpp:59-65`: an Ack and a Nack both call `ack_packet()`, so a
refused write leaves the queue exactly like an accepted one. A warning is
logged; there is no retry.

### F2. ACKs are matched by packet number only, across the whole queue
`ack_packet()` (`nasa_client.cpp:174-181`) takes any decoded Ack/Nack on the
bus. It does not check source or destination and erases the first queued
packet with the same 1-byte number (`nasa_command.h:36`). Two failure modes:
- An ACK exchanged between other devices (remotes, WiFi kits, outdoor unit)
  can clear our in-flight write.
- It can also erase a packet that is still waiting in the queue and was
  never sent. In a climate call, the mode write waits behind the power write,
  so it is exposed for as long as the power write is in flight.

### F3. Timeouts drop the packet with a thin log
`nasa_client.cpp:105-111` logs only the packet id. It does not name the
unit, message code or value, so a drop in the log cannot be tied to anything.

### F4. Reads may block the queue (needs a bus capture)
Reads are broadcast to `b2.00.20` (`nasa_client.cpp:21`) and queued with a
nonzero id, so they retry like writes. Replies arrive as `Response` packets
(`DataType::Response`, `nasa_command.h:23`), which never clear the queue.
Only Ack/Nack does. If the units do not also ACK a broadcast read, every
read batch is re-broadcast for 4 s and holds up any write queued behind it.
That covers the boot read of every code, the FSV poll batches, and the daily
re-poll.

### F5. Power and mode go out as separate packets
`nasactl_climate.cpp:84-98`: power (0x4000) and mode (0x4001) are two writes.
Target temperature (0x4201, `:107`) and fan (0x4006, `:121`) are two more. If
the mode frame is lost while power lands, the unit comes up in its
remembered mode. That is the Offices pattern. The packet format already
carries a message list (`Packet::messages`); `create_write` just never puts
more than one entry in it.

### F6. A lost write stays invisible until the unit reports that code
The optimistic publish (see above) stands until the next report. For the
climate codes, which report periodically, that is at most ~40 s. For FSV
codes it is up to the 24 h re-poll, unless the unit broadcasts FSV changes on
its own, which a capture can show. ha-config's `pv_surplus.yaml` writes four
FSV codes on the hydro unit:
- `dhw_disinfection_start_time` (0x4269)
- `dhw_disinfection_target_temp` (0x426A)
- `dhw_disinfection_day` (0x409A)
- `dhw_booster_heater_overshoot` (0x4267)

### F7. Power-on publishes a guessed mode
`update_power(true)` (`nasactl_climate.cpp:137-149`) restores
`last_active_mode_` when the unit reports power on, before any mode report
arrives. HA can briefly show a mode the unit never reported. This is minor:
the mode report corrects it within seconds. It does make in-between states
harder to read.

### F8. Nobody records the failures
The F1/F3 warnings are emitted. The bridge's `logger: logs: component: ERROR`
only affects ESPHome's core `component` tag. But HA's ESPHome entry for the
bridge does not subscribe to device logs, so nothing keeps them. No counters
exist.

### F9. Deployment exposure
ha-config's `esphome/samsung-nasa-bridge.yaml` builds from
`github://paolostivanin/nasactl@main` with `refresh: 0s`. The same bridge runs
the hydro unit (heating and DHW) and the five ACs. Any push to `main` goes
into the next bridge build.

## Constraints from ha-config

- **Keep the optimistic publish**, at least for climate. ac_curfew's
  `attribute_turn_on` attributes a turn-on to a person when
  `to_state.context.user_id` is set. That works only because the echo lands
  within milliseconds, carrying the HA call's context. Without it, app
  turn-ons lose attribution and the office complaint fixed in ac_curfew
  v1.4.0 comes back. bedroom_ac_night does not depend on the echo.
- **The HA packages assume nothing better than today.** Every change here is
  an improvement they do not rely on. Their 50-60 s waits could shrink once
  read-after-write is measured, but that is a separate ha-config change.
- **Units report in-between states while powering on** (off at +8 s, old
  mode at +7.8 s). Even with read-after-write, HA must not judge within a few
  seconds of a power-on.

## Revised implementation

### Transport and visibility

- Match ACK/NACK only against the transmitted front write, its destination, bridge address and packet number. Preserve packet numbers across retries and avoid collisions with queued packets.
- Treat NACK as a terminal refusal; blind retries could repeat an unsupported or unsafe request. Retain bounded timeout retries for unanswered writes.
- Send reads once using nonzero packet numbers, without waiting for ACK. Fix the partial batch dispatcher so trailing codes are eventually sent.
- Retain packet metadata for actionable logs and outcome callbacks. Add optional timeout, NACK and queue-drop diagnostic sensors, initialized at boot.
- Capture raw TX/RX and header fields before address/state filtering, including foreign ACKs. Capture is opt-in.
- Validate message boundaries and exact decoded payload length; reject malformed frames safely.

### Compatibility gates

The legacy transmitted header is the default until captured hardware evidence supports switching it. Decode standard NASA version/retry bit positions; make corrected transmission opt-in with `standard_command_header`. Keep `packet_information` false by default.

Per-device `control_data_type` defaults to Write; Request is opt-in for ordinary control. FSV writes always retain Write semantics, including when a caller supplies mixed categories. Per-device `targeted_reads` defaults false, preserving broadcast reads.

Climate `batch_writes` defaults false. Opt-in grouping includes only requested fields, with power only when mode was requested. Preserve Turbo, OFF with temperature/fan, fan-only and temperature-only calls, scaling and immediate optimistic publishing. No fallback corrective writes without measured hardware evidence.

### Reconciliation

After every outcome (ACK, NACK, timeout, queue rejection), schedule written codes at +3 s and +15 s; climate devices include power/mode/fan/target. Coalesce by address and code, cap memory, throttle failed queue admission and expire windows after 20 s. Queued reconciliation reads expire within 5 s or the remaining window.

Only genuine Response/Notification messages update state. Log absence of fresh reports. A fresh report is evidence of current state, not proof the requested value was accepted. Do not rewrite mismatches automatically. Preserve startup reads, FSV polling and optimistic updates needed for Home Assistant attribution.

## Automated verification and release policy

Every change must pass the repository CI before merging:

- Production C++ regression/behavior tests under GCC and Clang with AddressSanitizer and UndefinedBehaviorSanitizer.
- Independent golden wire fixtures, malformed packets and 20,000 deterministic fuzz inputs.
- Mutation tests that must detect five deliberately reintroduced ACK, NACK, read-queue and DHW-scaling defects in temporary copies.
- Fault injection for foreign/premature/duplicate ACKs, NACKs, packet loss/timeouts, bus traffic, queue pressure, wraparound and fragmented UART frames.
- Heating, DHW, FSV, select and sensor regressions; climate full/partial/OFF/Turbo calls; optimism and genuine-state correction.
- Real ESPHome configuration validation of all catalog entities, defaults and invalid combinations.
- Real ESPHome host runtime driven through a pseudo-terminal by an independent NASA simulator, for both default and experimental profiles.
- ESP32 firmware compilation on 2026.9.1 and 2026.7.0.

Commands and coverage are documented in `tests/README.md`. Add a reproducing regression for every discovered bug. Test changes must be reviewed with production changes; CI checks must be required by repository branch protection before release. Configuring remote branch protection is a separate administrative step.

## Local verification — 2026-10-02

| Check | Result |
|---|---|
| GCC and Clang host regressions with ASan/UBSan | Passed |
| Five mutation defects, GCC and Clang | All detected by assertions |
| ESPHome schema/catalog tests, 2026.9.1 and 2026.7.0 | Passed |
| Real runtime, default and experimental profiles, both versions | Passed |
| ESP32 firmware compilation, both versions | Passed |
| Python syntax and tracked diff whitespace | Passed |
| GitHub Actions workflow | Added; remote execution pending |
| Physical bus, HA attribution, production pin and multi-day soak | Pending |

## Hardware acceptance — pending

1. Pin the production bridge to an immutable known-good revision; preserve previous firmware and configuration for rollback. Do not deploy a floating `main` candidate.
2. Capture the baseline with raw logging. Establish ACK addresses, read ACK/Response behavior, response numbering, foreign traffic, packet-information/version/retry fields, Request versus Write acceptance, and unsolicited FSV reports.
3. Test transport/reconciliation changes using default wire settings. Verify initial reads and FSV polls, heating enable, DHW target and an appropriate FSV write/readback.
4. On one AC, enable experimental settings individually and verify grouped-write and targeted-read acceptance. Exercise off-to-cool/fan-only, fan, temperature, Turbo and off. Measure genuine readback latency. Intermediate power-on reports are allowed; optimistic state cannot be used as acceptance evidence.
5. Verify Home Assistant turn-on context attribution, dry-out, curfew and bedroom night behavior. Keep existing 50–60 s waits until measured evidence supports changing them.
6. Preserve logs/counters and soak the pinned candidate for several days with heating in use. Review every failure. Roll back on a heating/DHW regression or unexplained transport/state failure.
7. Release and update the production pin only after recorded hardware acceptance. No hardware flashing or Home Assistant changes are part of the local implementation.

Software tests cannot guarantee perfect operation or replace physical-bus and device acceptance testing. All unverified wire behavior remains opt-in.
