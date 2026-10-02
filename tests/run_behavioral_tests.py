#!/usr/bin/env python3
"""Run the real ESPHome application against an independent NASA bus simulator."""
import argparse
import binascii
import os
from pathlib import Path
import pty
import select
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BRIDGE = bytes.fromhex("80ff00")
HYDRO = bytes.fromhex("200000")
AC = bytes.fromhex("200002")
BROADCAST = bytes.fromhex("b20020")


def frame(source, destination, data_type, number, messages):
    payload = source + destination + bytes((0xC0, 0x10 | data_type, number, len(messages)))
    for code, value in messages:
        width = (1, 2, 4)[(code >> 9) & 3]
        payload += code.to_bytes(2, "big") + (value & ((1 << (width * 8)) - 1)).to_bytes(width, "big")
    crc = binascii.crc_hqx(payload, 0)
    return b"\x32" + (len(payload) + 4).to_bytes(2, "big") + payload + crc.to_bytes(2, "big") + b"\x34"


def parse(packet):
    assert packet[0] == 0x32 and packet[-1] == 0x34
    assert int.from_bytes(packet[-3:-1], "big") == binascii.crc_hqx(packet[3:-3], 0)
    offset, messages = 13, []
    for _ in range(packet[12]):
        code = int.from_bytes(packet[offset:offset + 2], "big")
        width = (1, 2, 4)[(code >> 9) & 3]
        value = int.from_bytes(packet[offset + 2:offset + 2 + width], "big")
        messages.append((code, value))
        offset += 2 + width
    assert offset == len(packet) - 3
    return packet[3:6], packet[6:9], packet[10] & 15, packet[11], messages


def run_profile(esphome, experimental):
    with tempfile.TemporaryDirectory(prefix="nasactl-behavior-") as directory:
        directory = Path(directory)
        master, slave = pty.openpty()
        # ESPHome accepts two-component absolute serial paths, while Linux
        # PTYs live under /dev/pts. Keep the alias entirely in /tmp.
        alias = Path('/tmp') / (directory.name + '-uart')
        alias.symlink_to(os.ttyname(slave))
        try:
            config = (ROOT / "tests" / "behavioral.yaml").read_text()
            replacements = {
                "component_path": str(ROOT / "components"),
                "uart_port": str(alias),
                "experimental": "true" if experimental else "false",
                "control_type": "request" if experimental else "write",
            }
            for key, value in replacements.items():
                config = config.replace("${" + key + "}", value)
            config_path = directory / "behavioral.yaml"
            config_path.write_text(config)
            build_log = directory / "build.log"
            with build_log.open("w") as log:
                build = subprocess.run([esphome, "compile", str(config_path)], stdout=log,
                                       stderr=subprocess.STDOUT, cwd=directory)
            if build.returncode:
                raise AssertionError(build_log.read_text()[-12000:])
            binary = directory / ".esphome" / "build" / "nasactl-behavior" / ".pioenvs" / "nasactl-behavior" / "program"
            states = {
                HYDRO: {0x4000: 0, 0x4235: 450, 0x4269: 10, 0x4066: 0, 0x4237: 485},
                AC: {0x4000: 0, 0x4001: 1, 0x4006: 0, 0x4201: 220, 0x4204: 220},
            }
            packets, buffer, output = [], b"", b""
            process = subprocess.Popen([str(binary)], cwd=directory, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 40
            try:
                while process.poll() is None:
                    if time.monotonic() >= deadline:
                        raise AssertionError("ESPHome runtime timed out\n" + output.decode(errors="replace")[-8000:])
                    ready, _, _ = select.select([master, process.stdout], [], [], 0.1)
                    if process.stdout in ready:
                        output += os.read(process.stdout.fileno(), 65536)
                    if master not in ready:
                        continue
                    buffer += os.read(master, 65536)
                    while len(buffer) >= 3:
                        length = int.from_bytes(buffer[1:3], "big") + 2
                        if len(buffer) < length:
                            break
                        packet, buffer = buffer[:length], buffer[length:]
                        source, dest, kind, number, messages = parse(packet)
                        assert source == BRIDGE and number != 0
                        assert packet[9] == (0xC0 if experimental else 0x20)
                        packets.append((dest, kind, messages))
                        if kind == 1:
                            for unit, values in states.items():
                                if dest not in (unit, BROADCAST):
                                    continue
                                response = [(code, values[code]) for code, _ in messages if code in values]
                                if response:
                                    os.write(master, frame(unit, BRIDGE, 5, number, response))
                        else:
                            assert dest in states and kind in (2, 3)
                            assert kind == (2 if any(code == 0x4269 for code, _ in messages)
                                            else 3 if experimental else 2)
                            if any(code == 0x4269 for code, _ in messages):
                                # Refuse the FSV: readback must replace its optimistic 9 with the real 10.
                                os.write(master, frame(dest, BRIDGE, 7, number, []))
                            else:
                                for code, value in messages:
                                    assert code in states[dest]
                                    states[dest][code] = value
                                # Foreign ACKs arrive on the same bus before the correct ACK.
                                os.write(master, frame(bytes.fromhex("200009"), BRIDGE, 6, number, []))
                                os.write(master, frame(dest, BRIDGE, 6, number, []))
                output += process.stdout.read()
                assert process.returncode == 0, output.decode(errors="replace")[-12000:]
                assert b"BEHAVIORAL TEST PASSED" in output
                assert states[HYDRO][0x4000] == 1 and states[HYDRO][0x4235] == 495
                assert states[AC][0x4000] == 0 and states[AC][0x4201] == 250
                fsv_writes = [p for p in packets if p[1] != 1 and any(code == 0x4269 for code, _ in p[2])]
                assert len(fsv_writes) == 1  # NACK did not become a blind retry.
                ac_writes = [messages for dest, kind, messages in packets if dest == AC and kind != 1]
                power_values = [value for messages in ac_writes for code, value in messages if code == 0x4000]
                assert power_values == [1, 0]  # Fan/temperature-only changes never turn it back on.
                if experimental:
                    assert any(len(messages) == 4 for messages in ac_writes)
                else:
                    assert all(len(messages) == 1 for messages in ac_writes)
                print(f"Real ESPHome behavioral profile {'experimental' if experimental else 'defaults'} passed", flush=True)
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait()
        finally:
            alias.unlink()
            os.close(master)
            os.close(slave)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--esphome", default="esphome")
    args = parser.parse_args()
    for experimental in (False, True):
        run_profile(args.esphome, experimental)
