#!/usr/bin/env python3
"""Compile production sources against deterministic clock/UART test doubles."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sources = Path(os.environ.get("NASACTL_TEST_SOURCES", root / "components" / "nasactl"))
with tempfile.TemporaryDirectory(prefix="nasactl-host-tests-") as build:
    build_path = Path(build)
    include = build_path / "esphome" / "components"
    include.mkdir(parents=True)
    (include / "nasactl").symlink_to(sources, target_is_directory=True)
    binary = build_path / "host-tests"
    command = [
        os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-Wno-sign-compare", "-Wno-unused-parameter", "-g",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        "-I", str(root / "tests" / "stubs"), "-I", build, "-I", str(sources),
        str(root / "tests" / "host_tests.cpp"),
        *(str(sources / name) for name in (
            "nasa_packet.cpp", "nasa_client.cpp", "nasa_controller.cpp", "nasactl_climate.cpp")),
        "-o", str(binary),
    ]
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True)
