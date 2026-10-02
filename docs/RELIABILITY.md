# Reliability and release acceptance

This component controls heating and domestic hot water as well as AC units.
Changes require regression coverage and recorded hardware acceptance before
deployment. Software verification completed on 2026-10-02; physical bus capture,
Home Assistant attribution checks and a multi-day soak remain pending.

Configuration, defaults and current behavior are documented in the
[README](../README.md#reliable-writes-and-diagnostics). Test commands and coverage
are documented in [tests/README.md](../tests/README.md).

## Incident evidence

On 2026-09-29, the Home Assistant dry-out script requested fan-only/high on five
AC units. Offices ended up in cool mode. Its recorder trace showed:

| Time after command | State shown in Home Assistant | Interpretation |
|---|---|---|
| 0 s | fan_only / high | Immediate optimistic publish |
| 8 s | off | Device report during startup |
| 19 s | cool / auto | Device report; requested mode had not been established |
| 30 s | cool / high | Device report; fan setting had arrived |

The script checked at 6 seconds and accepted the optimistic state. Other units
also reported intermediate states, including Kids showing cool at about 7.8
seconds. In this installation, periodic reports were observed about every 40.6
seconds, with reports following writes after 2–30 seconds. These observations
are not protocol guarantees.

The trace is consistent with power arriving while a mode write was lost. It
does not establish the cause without a bus capture. The incident record reports
that `ac_dryout.yaml` v1.4.0, `ac_curfew.yaml` v1.4.2 and
`bedroom_ac_night.yaml` v2.8.0 were adjusted to avoid judging an optimistic echo
and to wait 50–60 seconds for genuine state. Those external configurations need
checking against the deployed versions during acceptance.

## Findings and design decisions

The original transport findings refer to revision `a572c18`.

| Finding | Resulting behavior or remaining acceptance requirement |
|---|---|
| ACKs were matched by number across the queue; foreign or premature ACKs could clear writes. | Match only the transmitted front write, intended source, bridge destination and number. Avoid queued number collisions. |
| NACKs removed writes through the same completion path as ACKs. | Record a terminal refusal; do not blindly repeat a rejected command. |
| Timeout logs identified only a packet number, and no diagnostic counters existed. | Log destination, codes, values and send count; expose optional failure counters and raw packet capture. |
| Reads retried while waiting for ACKs that Response packets did not satisfy. Partial batches could remain stranded. | Transmit reads once and dispatch partial batches. Actual read ACK/Response behavior still needs capture. |
| Separate climate power/mode/fan/temperature packets could leave only some settings applied. | Offer opt-in grouped writes. Device acceptance and application order remain unverified. |
| Optimistic state could persist until a later report; FSV polling could be 24 hours apart. | Request bounded readback after every write outcome. Genuine reports correct state; mismatches do not trigger another write. |
| Power-on reports restore the remembered climate mode before a mode report arrives. | This behavior remains. Read all four climate state codes after writes, and allow intermediate startup states during acceptance. |
| The production configuration used a moving `@main` source with `refresh: 0s`. | Pin deployments to reviewed immutable revisions and preserve rollback firmware. Verify the actual production pin before deployment. |

Corrected transmit-header positions, the packet-information flag, Request
controls, targeted reconciliation reads and grouped climate writes remain
independent opt-in settings. Preserve the existing wire defaults until measured
acceptance supports changing them. ACK receipt and a fresh report alone do not
prove that the requested value was applied.

FSV readback matters particularly for the hydro settings used by
`pv_surplus.yaml` in the incident record:

| Entity | Code |
|---|---|
| `dhw_disinfection_start_time` | `0x4269` |
| `dhw_disinfection_target_temp` | `0x426A` |
| `dhw_disinfection_day` | `0x409A` |
| `dhw_booster_heater_overshoot` | `0x4267` |

## Home Assistant constraints

Keep immediate optimistic publishing. In the incident configuration,
`ac_curfew` uses `to_state.context.user_id` to attribute a turn-on to a person.
The immediate echo preserves that context; a delayed bus report may not. Verify
attribution with the candidate firmware and current Home Assistant packages.

Preserve the existing 50–60 second waits until actual readback latency has been
measured per unit. A power-on can produce intermediate off or remembered-mode
reports. Automations must allow settling time and use genuine reported state
as acceptance evidence.

## Software verification record

The implementation was verified locally on 2026-10-02:

| Check | Result |
|---|---|
| GCC and Clang production-source regressions with ASan/UBSan | Passed |
| Five deliberately reintroduced defects under both compilers | All detected by regression assertions |
| Configuration/catalog tests on ESPHome 2026.9.1 and 2026.7.0 | Passed |
| Real ESPHome runtime, default and experimental profiles, both versions | Passed |
| ESP32 firmware compilation, both versions | Passed |

ESPHome 2026.9.1 is the primary target; 2026.7.0 is the compatibility target.
The CI workflow runs the checks described in the test documentation. Require
passing checks before merging and add a reproducing regression for every bug.
Review test changes together with production changes. Required branch checks
must be configured separately in repository settings.

## Hardware acceptance checklist

Record the candidate commit, ESPHome version, enabled options, unit addresses,
capture dates and test results. Keep logs and counter history with the release
record. The following items remain pending:

- [ ] Verify that production uses an immutable known-good component revision.
  Preserve its firmware and configuration for rollback.
- [ ] Capture baseline raw frames. Establish ACK source/destination, read ACK
  and Response behavior, response numbering, foreign traffic, header fields,
  Request/Write acceptance and unsolicited FSV reports.
- [ ] Test the candidate with default wire settings. Verify initial reads and
  FSV polling, heating enable, DHW target and an appropriate FSV write/readback.
- [ ] On one AC, enable experimental settings individually. Verify grouped
  writes and targeted reads; exercise off-to-cool, off-to-fan-only, fan,
  temperature, Turbo, off and temperature/fan changes while off. Measure actual
  readback latency and allow intermediate startup reports.
- [ ] Verify Home Assistant turn-on attribution, dry-out, curfew and bedroom
  night behavior against the deployed package versions.
- [ ] Soak the pinned candidate for several days with heating in use. Review
  every failure and preserve logs and counter history. Roll back on a
  heating/DHW regression or unexplained transport/state failure.
- [ ] Record passing CI and hardware results before release, then update the
  production pin to the accepted immutable revision.

Software tests reduce risk; they do not establish physical RS-485 timing or
device acceptance. Keep unverified wire behavior opt-in.
