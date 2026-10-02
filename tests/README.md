# Reliability checks

Primary target: ESPHome **2026.9.1**. Compatibility target: **2026.7.0**.
CI runs every check below for changes to this repository; no test connects to a heat pump.

```sh
python3 tests/run_host_tests.py
CXX=clang++ python3 tests/run_mutation_tests.py
python tests/test_config.py
python3 tests/run_behavioral_tests.py --esphome esphome
esphome compile tests/compile.yaml
```

The Python schema and runtime checks require the selected ESPHome installed in the active environment. The host tests require GCC or Clang with C++17 and AddressSanitizer/UndefinedBehaviorSanitizer. Use `CXX=clang++` to check the second compiler. Runtime simulation uses Linux pseudo-terminals, an independent Python encoder and CRC implementation, and real ESPHome entities and UART code. Firmware compilation downloads ESP32 build tools on its first run.

The deterministic host suite compiles production C++ against clock and UART doubles. It covers independent packet fixtures, malformed frames, 20,000 deterministic fuzz inputs, field widths, partial batches, foreign and premature ACKs, terminal NACKs, retries, timeouts under bus activity, clock and packet-number wraparound, queue saturation, diagnostics, readback after every write outcome, readback deadlines and queue pressure, AC combined and partial calls, Turbo, DHW scaling, heating switches, FSV numbers, selects, sensor conversion, startup reads and FSV polling. Sanitizers check memory access and undefined behavior.

The schema suite validates every catalog entity, compatibility defaults, diagnostics, case-normalized addresses and invalid device/entity combinations. The real runtime suite runs both default and experimental wire settings. It verifies immediate optimistic state, heating and DHW calls, foreign ACK rejection, NACK without blind retries, FSV correction by genuine readback, and temperature/fan changes while AC is off. The ESP32 fixture compiles enabled and omitted optional settings together.

These checks cannot establish physical RS-485 timing, device acceptance of experimental wire settings, Home Assistant context attribution, or behavior under real competing controllers. Follow the [hardware acceptance checklist](../docs/RELIABILITY.md#hardware-acceptance-checklist) before deployment.

Mutation checks deliberately reintroduce five known defects in temporary source copies: foreign ACK acceptance, NACK counted as success, stranded partial batches, reads waiting for ACK, and missing DHW scaling. Each must compile and fail a regression assertion. Compilation failures do not count as detection. The production tree is never modified by this check.
